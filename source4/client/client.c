/* 
   Unix SMB/CIFS implementation.
   SMB client
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Simo Sorce 2001-2002
   Copyright (C) Jelmer Vernooij 2003-2004
   Copyright (C) James J Myers   2003 <myersjj@samba.org>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "version.h"
#include "dynconfig.h"
#include "clilist.h"
#include "lib/cmdline/popt_common.h"
#include "librpc/gen_ndr/ndr_srvsvc.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "libcli/raw/libcliraw.h"
#include "system/time.h"
#include "system/dir.h"
#include "system/filesys.h"
#include "dlinklist.h"

#ifndef REGISTER
#define REGISTER 0
#endif

struct smbcli_state *cli;
extern BOOL in_client;
static int port = 0;
pstring cur_dir = "\\";
static pstring cd_path = "";
static pstring service;
static pstring desthost;
static pstring username;
static pstring domain;
static pstring password;
static char *cmdstr = NULL;

static int io_bufsize = 64512;

static int name_type = 0x20;

static int process_tok(fstring tok);
static int cmd_help(const char **cmd_ptr);

/* 30 second timeout on most commands */
#define CLIENT_TIMEOUT (30*1000)
#define SHORT_TIMEOUT (5*1000)

/* value for unused fid field in trans2 secondary request */
#define FID_UNUSED (0xFFFF)

time_t newer_than = 0;
static int archive_level = 0;

static BOOL translation = False;

/* clitar bits insert */
extern int blocksize;
extern BOOL tar_inc;
extern BOOL tar_reset;
/* clitar bits end */
 

static BOOL prompt = True;

static int printmode = 1;

static BOOL recurse = False;
BOOL lowercase = False;

static const char *dest_ip;

#define SEPARATORS " \t\n\r"

static BOOL abort_mget = True;

static pstring fileselection = "";

/* timing globals */
uint64_t get_total_size = 0;
uint_t get_total_time_ms = 0;
static uint64_t put_total_size = 0;
static uint_t put_total_time_ms = 0;

/* totals globals */
static double dir_total;

#define USENMB

/* some forward declarations */
static struct smbcli_state *do_connect(const char *server, const char *share);


/*******************************************************************
 Reduce a file name, removing .. elements.
********************************************************************/
void dos_clean_name(char *s)
{
	char *p=NULL;

	DEBUG(3,("dos_clean_name [%s]\n",s));

	/* remove any double slashes */
	all_string_sub(s, "\\\\", "\\", 0);

	while ((p = strstr(s,"\\..\\")) != NULL) {
		pstring s1;

		*p = 0;
		pstrcpy(s1,p+3);

		if ((p=strrchr_m(s,'\\')) != NULL)
			*p = 0;
		else
			*s = 0;
		pstrcat(s,s1);
	}  

	trim_string(s,NULL,"\\..");

	all_string_sub(s, "\\.\\", "\\", 0);
}

/****************************************************************************
write to a local file with CR/LF->LF translation if appropriate. return the 
number taken from the buffer. This may not equal the number written.
****************************************************************************/
static int writefile(int f, const void *_b, int n)
{
	const uint8_t *b = _b;
	int i;

	if (!translation) {
		return write(f,b,n);
	}

	i = 0;
	while (i < n) {
		if (*b == '\r' && (i<(n-1)) && *(b+1) == '\n') {
			b++;i++;
		}
		if (write(f, b, 1) != 1) {
			break;
		}
		b++;
		i++;
	}
  
	return(i);
}

/****************************************************************************
  read from a file with LF->CR/LF translation if appropriate. return the 
  number read. read approx n bytes.
****************************************************************************/
static int readfile(void *_b, int n, XFILE *f)
{
	uint8_t *b = _b;
	int i;
	int c;

	if (!translation)
		return x_fread(b,1,n,f);
  
	i = 0;
	while (i < (n - 1)) {
		if ((c = x_getc(f)) == EOF) {
			break;
		}
      
		if (c == '\n') { /* change all LFs to CR/LF */
			b[i++] = '\r';
		}
      
		b[i++] = c;
	}
  
	return(i);
}
 

/****************************************************************************
send a message
****************************************************************************/
static void send_message(void)
{
	int total_len = 0;
	int grp_id;

	if (!smbcli_message_start(cli->tree, desthost, username, &grp_id)) {
		d_printf("message start: %s\n", smbcli_errstr(cli->tree));
		return;
	}


	d_printf("Connected. Type your message, ending it with a Control-D\n");

	while (!feof(stdin) && total_len < 1600) {
		int maxlen = MIN(1600 - total_len,127);
		pstring msg;
		int l=0;
		int c;

		ZERO_ARRAY(msg);

		for (l=0;l<maxlen && (c=fgetc(stdin))!=EOF;l++) {
			if (c == '\n')
				msg[l++] = '\r';
			msg[l] = c;   
		}

		if (!smbcli_message_text(cli->tree, msg, l, grp_id)) {
			d_printf("SMBsendtxt failed (%s)\n",smbcli_errstr(cli->tree));
			return;
		}      
		
		total_len += l;
	}

	if (total_len >= 1600)
		d_printf("the message was truncated to 1600 bytes\n");
	else
		d_printf("sent %d bytes\n",total_len);

	if (!smbcli_message_end(cli->tree, grp_id)) {
		d_printf("SMBsendend failed (%s)\n",smbcli_errstr(cli->tree));
		return;
	}      
}



/****************************************************************************
check the space on a device
****************************************************************************/
static int do_dskattr(void)
{
	int total, bsize, avail;

	if (NT_STATUS_IS_ERR(smbcli_dskattr(cli->tree, &bsize, &total, &avail))) {
		d_printf("Error in dskattr: %s\n",smbcli_errstr(cli->tree)); 
		return 1;
	}

	d_printf("\n\t\t%d blocks of size %d. %d blocks available\n",
		 total, bsize, avail);

	return 0;
}

/****************************************************************************
show cd/pwd
****************************************************************************/
static int cmd_pwd(const char **cmd_ptr)
{
	d_printf("Current directory is %s",service);
	d_printf("%s\n",cur_dir);
	return 0;
}

/*
  convert a string to dos format
*/
static void dos_format(char *s)
{
	string_replace(s, '/', '\\');
}

/****************************************************************************
change directory - inner section
****************************************************************************/
static int do_cd(char *newdir)
{
	char *p = newdir;
	pstring saved_dir;
	pstring dname;
      
	dos_format(newdir);

	/* Save the current directory in case the
	   new directory is invalid */
	pstrcpy(saved_dir, cur_dir);
	if (*p == '\\')
		pstrcpy(cur_dir,p);
	else
		pstrcat(cur_dir,p);
	if (*(cur_dir+strlen(cur_dir)-1) != '\\') {
		pstrcat(cur_dir, "\\");
	}
	dos_clean_name(cur_dir);
	pstrcpy(dname,cur_dir);
	pstrcat(cur_dir,"\\");
	dos_clean_name(cur_dir);
	
	if (!strequal(cur_dir,"\\")) {
		if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, dname))) {
			d_printf("cd %s: %s\n", dname, smbcli_errstr(cli->tree));
			pstrcpy(cur_dir,saved_dir);
		}
	}
	
	pstrcpy(cd_path,cur_dir);

	return 0;
}

/****************************************************************************
change directory
****************************************************************************/
static int cmd_cd(const char **cmd_ptr)
{
	fstring buf;
	int rc = 0;

	if (next_token(cmd_ptr,buf,NULL,sizeof(buf)))
		rc = do_cd(buf);
	else
		d_printf("Current directory is %s\n",cur_dir);

	return rc;
}


BOOL mask_match(struct smbcli_state *c, const char *string, char *pattern, 
		BOOL is_case_sensitive)
{
	fstring p2, s2;

	if (strcmp(string,"..") == 0)
		string = ".";
	if (strcmp(pattern,".") == 0)
		return False;
	
	if (is_case_sensitive)
		return ms_fnmatch(pattern, string, 
				  c->transport->negotiate.protocol) == 0;

	fstrcpy(p2, pattern);
	fstrcpy(s2, string);
	strlower(p2); 
	strlower(s2);
	return ms_fnmatch(p2, s2, c->transport->negotiate.protocol) == 0;
}



/*******************************************************************
  decide if a file should be operated on
  ********************************************************************/
static BOOL do_this_one(struct clilist_file_info *finfo)
{
	if (finfo->attrib & FILE_ATTRIBUTE_DIRECTORY) return(True);

	if (*fileselection && 
	    !mask_match(cli, finfo->name,fileselection,False)) {
		DEBUG(3,("mask_match %s failed\n", finfo->name));
		return False;
	}

	if (newer_than && finfo->mtime < newer_than) {
		DEBUG(3,("newer_than %s failed\n", finfo->name));
		return(False);
	}

	if ((archive_level==1 || archive_level==2) && !(finfo->attrib & FILE_ATTRIBUTE_ARCHIVE)) {
		DEBUG(3,("archive %s failed\n", finfo->name));
		return(False);
	}
	
	return(True);
}

/****************************************************************************
  display info about a file
  ****************************************************************************/
static void display_finfo(struct clilist_file_info *finfo)
{
	if (do_this_one(finfo)) {
		time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */
		char *astr = attrib_string(NULL, finfo->attrib);
		d_printf("  %-30s%7.7s %8.0f  %s",
			 finfo->name,
			 astr,
			 (double)finfo->size,
			 asctime(localtime(&t)));
		dir_total += finfo->size;
		talloc_free(astr);
	}
}


/****************************************************************************
   accumulate size of a file
  ****************************************************************************/
static void do_du(struct clilist_file_info *finfo)
{
	if (do_this_one(finfo)) {
		dir_total += finfo->size;
	}
}

static BOOL do_list_recurse;
static BOOL do_list_dirs;
static char *do_list_queue = 0;
static long do_list_queue_size = 0;
static long do_list_queue_start = 0;
static long do_list_queue_end = 0;
static void (*do_list_fn)(struct clilist_file_info *);

/****************************************************************************
functions for do_list_queue
  ****************************************************************************/

/*
 * The do_list_queue is a NUL-separated list of strings stored in a
 * char*.  Since this is a FIFO, we keep track of the beginning and
 * ending locations of the data in the queue.  When we overflow, we
 * double the size of the char*.  When the start of the data passes
 * the midpoint, we move everything back.  This is logically more
 * complex than a linked list, but easier from a memory management
 * angle.  In any memory error condition, do_list_queue is reset.
 * Functions check to ensure that do_list_queue is non-NULL before
 * accessing it.
 */
static void reset_do_list_queue(void)
{
	SAFE_FREE(do_list_queue);
	do_list_queue_size = 0;
	do_list_queue_start = 0;
	do_list_queue_end = 0;
}

static void init_do_list_queue(void)
{
	reset_do_list_queue();
	do_list_queue_size = 1024;
	do_list_queue = malloc(do_list_queue_size);
	if (do_list_queue == 0) { 
		d_printf("malloc fail for size %d\n",
			 (int)do_list_queue_size);
		reset_do_list_queue();
	} else {
		memset(do_list_queue, 0, do_list_queue_size);
	}
}

static void adjust_do_list_queue(void)
{
	/*
	 * If the starting point of the queue is more than half way through,
	 * move everything toward the beginning.
	 */
	if (do_list_queue && (do_list_queue_start == do_list_queue_end))
	{
		DEBUG(4,("do_list_queue is empty\n"));
		do_list_queue_start = do_list_queue_end = 0;
		*do_list_queue = '\0';
	}
	else if (do_list_queue_start > (do_list_queue_size / 2))
	{
		DEBUG(4,("sliding do_list_queue backward\n"));
		memmove(do_list_queue,
			do_list_queue + do_list_queue_start,
			do_list_queue_end - do_list_queue_start);
		do_list_queue_end -= do_list_queue_start;
		do_list_queue_start = 0;
	}
	   
}

static void add_to_do_list_queue(const char* entry)
{
	char *dlq;
	long new_end = do_list_queue_end + ((long)strlen(entry)) + 1;
	while (new_end > do_list_queue_size)
	{
		do_list_queue_size *= 2;
		DEBUG(4,("enlarging do_list_queue to %d\n",
			 (int)do_list_queue_size));
		dlq = realloc_p(do_list_queue, char, do_list_queue_size);
		if (! dlq) {
			d_printf("failure enlarging do_list_queue to %d bytes\n",
				 (int)do_list_queue_size);
			reset_do_list_queue();
		}
		else
		{
			do_list_queue = dlq;
			memset(do_list_queue + do_list_queue_size / 2,
			       0, do_list_queue_size / 2);
		}
	}
	if (do_list_queue)
	{
		safe_strcpy(do_list_queue + do_list_queue_end, entry, 
			    do_list_queue_size - do_list_queue_end - 1);
		do_list_queue_end = new_end;
		DEBUG(4,("added %s to do_list_queue (start=%d, end=%d)\n",
			 entry, (int)do_list_queue_start, (int)do_list_queue_end));
	}
}

static char *do_list_queue_head(void)
{
	return do_list_queue + do_list_queue_start;
}

static void remove_do_list_queue_head(void)
{
	if (do_list_queue_end > do_list_queue_start)
	{
		do_list_queue_start += strlen(do_list_queue_head()) + 1;
		adjust_do_list_queue();
		DEBUG(4,("removed head of do_list_queue (start=%d, end=%d)\n",
			 (int)do_list_queue_start, (int)do_list_queue_end));
	}
}

static int do_list_queue_empty(void)
{
	return (! (do_list_queue && *do_list_queue));
}

/****************************************************************************
a helper for do_list
  ****************************************************************************/
static void do_list_helper(struct clilist_file_info *f, const char *mask, void *state)
{
	if (f->attrib & FILE_ATTRIBUTE_DIRECTORY) {
		if (do_list_dirs && do_this_one(f)) {
			do_list_fn(f);
		}
		if (do_list_recurse && 
		    !strequal(f->name,".") && 
		    !strequal(f->name,"..")) {
			pstring mask2;
			char *p;

			pstrcpy(mask2, mask);
			p = strrchr_m(mask2,'\\');
			if (!p) return;
			p[1] = 0;
			pstrcat(mask2, f->name);
			pstrcat(mask2,"\\*");
			add_to_do_list_queue(mask2);
		}
		return;
	}

	if (do_this_one(f)) {
		do_list_fn(f);
	}
}


/****************************************************************************
a wrapper around smbcli_list that adds recursion
  ****************************************************************************/
void do_list(const char *mask,uint16_t attribute,
	     void (*fn)(struct clilist_file_info *),BOOL rec, BOOL dirs)
{
	static int in_do_list = 0;

	if (in_do_list && rec)
	{
		fprintf(stderr, "INTERNAL ERROR: do_list called recursively when the recursive flag is true\n");
		exit(1);
	}

	in_do_list = 1;

	do_list_recurse = rec;
	do_list_dirs = dirs;
	do_list_fn = fn;

	if (rec)
	{
		init_do_list_queue();
		add_to_do_list_queue(mask);
		
		while (! do_list_queue_empty())
		{
			/*
			 * Need to copy head so that it doesn't become
			 * invalid inside the call to smbcli_list.  This
			 * would happen if the list were expanded
			 * during the call.
			 * Fix from E. Jay Berkenbilt (ejb@ql.org)
			 */
			pstring head;
			pstrcpy(head, do_list_queue_head());
			smbcli_list(cli->tree, head, attribute, do_list_helper, NULL);
			remove_do_list_queue_head();
			if ((! do_list_queue_empty()) && (fn == display_finfo))
			{
				char* next_file = do_list_queue_head();
				char* save_ch = 0;
				if ((strlen(next_file) >= 2) &&
				    (next_file[strlen(next_file) - 1] == '*') &&
				    (next_file[strlen(next_file) - 2] == '\\'))
				{
					save_ch = next_file +
						strlen(next_file) - 2;
					*save_ch = '\0';
				}
				d_printf("\n%s\n",next_file);
				if (save_ch)
				{
					*save_ch = '\\';
				}
			}
		}
	}
	else
	{
		if (smbcli_list(cli->tree, mask, attribute, do_list_helper, NULL) == -1)
		{
			d_printf("%s listing %s\n", smbcli_errstr(cli->tree), mask);
		}
	}

	in_do_list = 0;
	reset_do_list_queue();
}

/****************************************************************************
  get a directory listing
  ****************************************************************************/
static int cmd_dir(const char **cmd_ptr)
{
	uint16_t attribute = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	pstring mask;
	fstring buf;
	char *p=buf;
	int rc;
	
	dir_total = 0;
	pstrcpy(mask,cur_dir);
	if(mask[strlen(mask)-1]!='\\')
		pstrcat(mask,"\\");
	
	if (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		dos_format(p);
		if (*p == '\\')
			pstrcpy(mask,p);
		else
			pstrcat(mask,p);
	}
	else {
		if (cli->tree->session->transport->negotiate.protocol <= 
		    PROTOCOL_LANMAN1) {	
			pstrcat(mask,"*.*");
		} else {
			pstrcat(mask,"*");
		}
	}

	do_list(mask, attribute, display_finfo, recurse, True);

	rc = do_dskattr();

	DEBUG(3, ("Total bytes listed: %.0f\n", dir_total));

	return rc;
}


/****************************************************************************
  get a directory listing
  ****************************************************************************/
static int cmd_du(const char **cmd_ptr)
{
	uint16_t attribute = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	pstring mask;
	fstring buf;
	char *p=buf;
	int rc;
	
	dir_total = 0;
	pstrcpy(mask,cur_dir);
	if(mask[strlen(mask)-1]!='\\')
		pstrcat(mask,"\\");
	
	if (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		dos_format(p);
		if (*p == '\\')
			pstrcpy(mask,p);
		else
			pstrcat(mask,p);
	} else {
		pstrcat(mask,"\\*");
	}

	do_list(mask, attribute, do_du, recurse, True);

	rc = do_dskattr();

	d_printf("Total number of bytes: %.0f\n", dir_total);

	return rc;
}


/****************************************************************************
  get a file from rname to lname
  ****************************************************************************/
static int do_get(char *rname, const char *lname, BOOL reget)
{  
	int handle = 0, fnum;
	BOOL newhandle = False;
	uint8_t *data;
	struct timeval tp_start;
	int read_size = io_bufsize;
	uint16_t attr;
	size_t size;
	off_t start = 0;
	off_t nread = 0;
	int rc = 0;

	GetTimeOfDay(&tp_start);

	if (lowercase) {
		strlower(discard_const_p(char, lname));
	}

	fnum = smbcli_open(cli->tree, rname, O_RDONLY, DENY_NONE);

	if (fnum == -1) {
		d_printf("%s opening remote file %s\n",smbcli_errstr(cli->tree),rname);
		return 1;
	}

	if(!strcmp(lname,"-")) {
		handle = fileno(stdout);
	} else {
		if (reget) {
			handle = open(lname, O_WRONLY|O_CREAT, 0644);
			if (handle >= 0) {
				start = lseek(handle, 0, SEEK_END);
				if (start == -1) {
					d_printf("Error seeking local file\n");
					return 1;
				}
			}
		} else {
			handle = open(lname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		}
		newhandle = True;
	}
	if (handle < 0) {
		d_printf("Error opening local file %s\n",lname);
		return 1;
	}


	if (NT_STATUS_IS_ERR(smbcli_qfileinfo(cli->tree, fnum, 
			   &attr, &size, NULL, NULL, NULL, NULL, NULL)) &&
	    NT_STATUS_IS_ERR(smbcli_getattrE(cli->tree, fnum, 
			  &attr, &size, NULL, NULL, NULL))) {
		d_printf("getattrib: %s\n",smbcli_errstr(cli->tree));
		return 1;
	}

	DEBUG(2,("getting file %s of size %.0f as %s ", 
		 rname, (double)size, lname));

	if(!(data = (uint8_t *)malloc(read_size))) { 
		d_printf("malloc fail for size %d\n", read_size);
		smbcli_close(cli->tree, fnum);
		return 1;
	}

	while (1) {
		int n = smbcli_read(cli->tree, fnum, data, nread + start, read_size);

		if (n <= 0) break;
 
		if (writefile(handle,data, n) != n) {
			d_printf("Error writing local file\n");
			rc = 1;
			break;
		}
      
		nread += n;
	}

	if (nread + start < size) {
		DEBUG (0, ("Short read when getting file %s. Only got %ld bytes.\n",
			    rname, (long)nread));

		rc = 1;
	}

	SAFE_FREE(data);
	
	if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnum))) {
		d_printf("Error %s closing remote file\n",smbcli_errstr(cli->tree));
		rc = 1;
	}

	if (newhandle) {
		close(handle);
	}

	if (archive_level >= 2 && (attr & FILE_ATTRIBUTE_ARCHIVE)) {
		smbcli_setatr(cli->tree, rname, attr & ~(uint16_t)FILE_ATTRIBUTE_ARCHIVE, 0);
	}

	{
		struct timeval tp_end;
		int this_time;
		
		GetTimeOfDay(&tp_end);
		this_time = 
			(tp_end.tv_sec - tp_start.tv_sec)*1000 +
			(tp_end.tv_usec - tp_start.tv_usec)/1000;
		get_total_time_ms += this_time;
		get_total_size += nread;
		
		DEBUG(2,("(%3.1f kb/s) (average %3.1f kb/s)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 get_total_size / (1.024*get_total_time_ms)));
	}
	
	return rc;
}


/****************************************************************************
  get a file
  ****************************************************************************/
static int cmd_get(const char **cmd_ptr)
{
	pstring lname;
	pstring rname;
	char *p;

	pstrcpy(rname,cur_dir);
	pstrcat(rname,"\\");
	
	p = rname + strlen(rname);
	
	if (!next_token(cmd_ptr,p,NULL,sizeof(rname)-strlen(rname))) {
		d_printf("get <filename>\n");
		return 1;
	}
	pstrcpy(lname,p);
	dos_clean_name(rname);
	
	next_token(cmd_ptr,lname,NULL,sizeof(lname));
	
	return do_get(rname, lname, False);
}

/****************************************************************************
 Put up a yes/no prompt.
****************************************************************************/
static BOOL yesno(char *p)
{
	pstring ans;
	printf("%s",p);

	if (!fgets(ans,sizeof(ans)-1,stdin))
		return(False);

	if (*ans == 'y' || *ans == 'Y')
		return(True);

	return(False);
}

/****************************************************************************
  do a mget operation on one file
  ****************************************************************************/
static void do_mget(struct clilist_file_info *finfo)
{
	pstring rname;
	pstring quest;
	pstring saved_curdir;
	pstring mget_mask;

	if (strequal(finfo->name,".") || strequal(finfo->name,".."))
		return;

	if (abort_mget)	{
		d_printf("mget aborted\n");
		return;
	}

	if (finfo->attrib & FILE_ATTRIBUTE_DIRECTORY)
		slprintf(quest,sizeof(pstring)-1,
			 "Get directory %s? ",finfo->name);
	else
		slprintf(quest,sizeof(pstring)-1,
			 "Get file %s? ",finfo->name);

	if (prompt && !yesno(quest)) return;

	if (!(finfo->attrib & FILE_ATTRIBUTE_DIRECTORY)) {
		pstrcpy(rname,cur_dir);
		pstrcat(rname,finfo->name);
		do_get(rname, finfo->name, False);
		return;
	}

	/* handle directories */
	pstrcpy(saved_curdir,cur_dir);

	pstrcat(cur_dir,finfo->name);
	pstrcat(cur_dir,"\\");

	string_replace(discard_const_p(char, finfo->name), '\\', '/');
	if (lowercase) {
		strlower(discard_const_p(char, finfo->name));
	}
	
	if (!directory_exist(finfo->name,NULL) && 
	    mkdir(finfo->name,0777) != 0) {
		d_printf("failed to create directory %s\n",finfo->name);
		pstrcpy(cur_dir,saved_curdir);
		return;
	}
	
	if (chdir(finfo->name) != 0) {
		d_printf("failed to chdir to directory %s\n",finfo->name);
		pstrcpy(cur_dir,saved_curdir);
		return;
	}

	pstrcpy(mget_mask,cur_dir);
	pstrcat(mget_mask,"*");
	
	do_list(mget_mask, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY,do_mget,False, True);
	chdir("..");
	pstrcpy(cur_dir,saved_curdir);
}


/****************************************************************************
view the file using the pager
****************************************************************************/
static int cmd_more(const char **cmd_ptr)
{
	fstring rname,lname,pager_cmd;
	char *pager;
	int fd;
	int rc = 0;

	fstrcpy(rname,cur_dir);
	fstrcat(rname,"\\");
	
	slprintf(lname,sizeof(lname)-1, "%s/smbmore.XXXXXX",tmpdir());
	fd = smb_mkstemp(lname);
	if (fd == -1) {
		d_printf("failed to create temporary file for more\n");
		return 1;
	}
	close(fd);

	if (!next_token(cmd_ptr,rname+strlen(rname),NULL,sizeof(rname)-strlen(rname))) {
		d_printf("more <filename>\n");
		unlink(lname);
		return 1;
	}
	dos_clean_name(rname);

	rc = do_get(rname, lname, False);

	pager=getenv("PAGER");

	slprintf(pager_cmd,sizeof(pager_cmd)-1,
		 "%s %s",(pager? pager:PAGER), lname);
	system(pager_cmd);
	unlink(lname);
	
	return rc;
}



/****************************************************************************
do a mget command
****************************************************************************/
static int cmd_mget(const char **cmd_ptr)
{
	uint16_t attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	pstring mget_mask;
	fstring buf;
	char *p=buf;

	*mget_mask = 0;

	if (recurse)
		attribute |= FILE_ATTRIBUTE_DIRECTORY;
	
	abort_mget = False;

	while (next_token(cmd_ptr,p,NULL,sizeof(buf))) {
		pstrcpy(mget_mask,cur_dir);
		if(mget_mask[strlen(mget_mask)-1]!='\\')
			pstrcat(mget_mask,"\\");
		
		if (*p == '\\')
			pstrcpy(mget_mask,p);
		else
			pstrcat(mget_mask,p);
		do_list(mget_mask, attribute,do_mget,False,True);
	}

	if (!*mget_mask) {
		pstrcpy(mget_mask,cur_dir);
		if(mget_mask[strlen(mget_mask)-1]!='\\')
			pstrcat(mget_mask,"\\");
		pstrcat(mget_mask,"*");
		do_list(mget_mask, attribute,do_mget,False,True);
	}
	
	return 0;
}


/****************************************************************************
make a directory of name "name"
****************************************************************************/
static NTSTATUS do_mkdir(char *name)
{
	NTSTATUS status;

	if (NT_STATUS_IS_ERR(status = smbcli_mkdir(cli->tree, name))) {
		d_printf("%s making remote directory %s\n",
			 smbcli_errstr(cli->tree),name);
		return status;
	}

	return status;
}

/****************************************************************************
show 8.3 name of a file
****************************************************************************/
static BOOL do_altname(char *name)
{
	const char *altname;
	if (!NT_STATUS_IS_OK(smbcli_qpathinfo_alt_name(cli->tree, name, &altname))) {
		d_printf("%s getting alt name for %s\n",
			 smbcli_errstr(cli->tree),name);
		return(False);
	}
	d_printf("%s\n", altname);

	return(True);
}


/****************************************************************************
 Exit client.
****************************************************************************/
static int cmd_quit(const char **cmd_ptr)
{
	smbcli_shutdown(cli);
	exit(0);
	/* NOTREACHED */
	return 0;
}


/****************************************************************************
  make a directory
  ****************************************************************************/
static int cmd_mkdir(const char **cmd_ptr)
{
	pstring mask;
	fstring buf;
	char *p=buf;
  
	pstrcpy(mask,cur_dir);

	if (!next_token(cmd_ptr,p,NULL,sizeof(buf))) {
		if (!recurse)
			d_printf("mkdir <dirname>\n");
		return 1;
	}
	pstrcat(mask,p);

	if (recurse) {
		pstring ddir;
		pstring ddir2;
		*ddir2 = 0;
		
		pstrcpy(ddir,mask);
		trim_string(ddir,".",NULL);
		p = strtok(ddir,"/\\");
		while (p) {
			pstrcat(ddir2,p);
			if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, ddir2))) { 
				do_mkdir(ddir2);
			}
			pstrcat(ddir2,"\\");
			p = strtok(NULL,"/\\");
		}	 
	} else {
		do_mkdir(mask);
	}
	
	return 0;
}


/****************************************************************************
  show alt name
  ****************************************************************************/
static int cmd_altname(const char **cmd_ptr)
{
	pstring name;
	fstring buf;
	char *p=buf;
  
	pstrcpy(name,cur_dir);

	if (!next_token(cmd_ptr,p,NULL,sizeof(buf))) {
		d_printf("altname <file>\n");
		return 1;
	}
	pstrcat(name,p);

	do_altname(name);

	return 0;
}


/****************************************************************************
  put a single file
  ****************************************************************************/
static int do_put(char *rname, char *lname, BOOL reput)
{
	int fnum;
	XFILE *f;
	size_t start = 0;
	off_t nread = 0;
	uint8_t *buf = NULL;
	int maxwrite = io_bufsize;
	int rc = 0;
	
	struct timeval tp_start;
	GetTimeOfDay(&tp_start);

	if (reput) {
		fnum = smbcli_open(cli->tree, rname, O_RDWR|O_CREAT, DENY_NONE);
		if (fnum >= 0) {
			if (NT_STATUS_IS_ERR(smbcli_qfileinfo(cli->tree, fnum, NULL, &start, NULL, NULL, NULL, NULL, NULL)) &&
			    NT_STATUS_IS_ERR(smbcli_getattrE(cli->tree, fnum, NULL, &start, NULL, NULL, NULL))) {
				d_printf("getattrib: %s\n",smbcli_errstr(cli->tree));
				return 1;
			}
		}
	} else {
		fnum = smbcli_open(cli->tree, rname, O_RDWR|O_CREAT|O_TRUNC, 
				DENY_NONE);
	}
  
	if (fnum == -1) {
		d_printf("%s opening remote file %s\n",smbcli_errstr(cli->tree),rname);
		return 1;
	}

	/* allow files to be piped into smbclient
	   jdblair 24.jun.98

	   Note that in this case this function will exit(0) rather
	   than returning. */
	if (!strcmp(lname, "-")) {
		f = x_stdin;
		/* size of file is not known */
	} else {
		f = x_fopen(lname,O_RDONLY, 0);
		if (f && reput) {
			if (x_tseek(f, start, SEEK_SET) == -1) {
				d_printf("Error seeking local file\n");
				return 1;
			}
		}
	}

	if (!f) {
		d_printf("Error opening local file %s\n",lname);
		return 1;
	}

  
	DEBUG(1,("putting file %s as %s ",lname,
		 rname));
  
	buf = (uint8_t *)malloc(maxwrite);
	if (!buf) {
		d_printf("ERROR: Not enough memory!\n");
		return 1;
	}
	while (!x_feof(f)) {
		int n = maxwrite;
		int ret;

		if ((n = readfile(buf,n,f)) < 1) {
			if((n == 0) && x_feof(f))
				break; /* Empty local file. */

			d_printf("Error reading local file: %s\n", strerror(errno));
			rc = 1;
			break;
		}

		ret = smbcli_write(cli->tree, fnum, 0, buf, nread + start, n);

		if (n != ret) {
			d_printf("Error writing file: %s\n", smbcli_errstr(cli->tree));
			rc = 1;
			break;
		} 

		nread += n;
	}

	if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnum))) {
		d_printf("%s closing remote file %s\n",smbcli_errstr(cli->tree),rname);
		x_fclose(f);
		SAFE_FREE(buf);
		return 1;
	}

	
	if (f != x_stdin) {
		x_fclose(f);
	}

	SAFE_FREE(buf);

	{
		struct timeval tp_end;
		int this_time;
		
		GetTimeOfDay(&tp_end);
		this_time = 
			(tp_end.tv_sec - tp_start.tv_sec)*1000 +
			(tp_end.tv_usec - tp_start.tv_usec)/1000;
		put_total_time_ms += this_time;
		put_total_size += nread;
		
		DEBUG(1,("(%3.1f kb/s) (average %3.1f kb/s)\n",
			 nread / (1.024*this_time + 1.0e-4),
			 put_total_size / (1.024*put_total_time_ms)));
	}

	if (f == x_stdin) {
		smbcli_shutdown(cli);
		exit(0);
	}
	
	return rc;
}

 

/****************************************************************************
  put a file
  ****************************************************************************/
static int cmd_put(const char **cmd_ptr)
{
	pstring lname;
	pstring rname;
	fstring buf;
	char *p=buf;
	
	pstrcpy(rname,cur_dir);
	pstrcat(rname,"\\");
  
	if (!next_token(cmd_ptr,p,NULL,sizeof(buf))) {
		d_printf("put <filename>\n");
		return 1;
	}
	pstrcpy(lname,p);
  
	if (next_token(cmd_ptr,p,NULL,sizeof(buf)))
		pstrcat(rname,p);      
	else
		pstrcat(rname,lname);
	
	dos_clean_name(rname);

	{
		struct stat st;
		/* allow '-' to represent stdin
		   jdblair, 24.jun.98 */
		if (!file_exist(lname,&st) &&
		    (strcmp(lname,"-"))) {
			d_printf("%s does not exist\n",lname);
			return 1;
		}
	}

	return do_put(rname, lname, False);
}

/*************************************
  File list structure
*************************************/

static struct file_list {
	struct file_list *prev, *next;
	char *file_path;
	BOOL isdir;
} *file_list;

/****************************************************************************
  Free a file_list structure
****************************************************************************/

static void free_file_list (struct file_list * list)
{
	struct file_list *tmp;
	
	while (list)
	{
		tmp = list;
		DLIST_REMOVE(list, list);
		SAFE_FREE(tmp->file_path);
		SAFE_FREE(tmp);
	}
}

/****************************************************************************
  seek in a directory/file list until you get something that doesn't start with
  the specified name
  ****************************************************************************/
static BOOL seek_list(struct file_list *list, char *name)
{
	while (list) {
		trim_string(list->file_path,"./","\n");
		if (strncmp(list->file_path, name, strlen(name)) != 0) {
			return(True);
		}
		list = list->next;
	}
      
	return(False);
}

/****************************************************************************
  set the file selection mask
  ****************************************************************************/
static int cmd_select(const char **cmd_ptr)
{
	pstrcpy(fileselection,"");
	next_token(cmd_ptr,fileselection,NULL,sizeof(fileselection));

	return 0;
}

/*******************************************************************
  A readdir wrapper which just returns the file name.
 ********************************************************************/
static const char *readdirname(DIR *p)
{
	struct dirent *ptr;
	char *dname;

	if (!p)
		return(NULL);
  
	ptr = (struct dirent *)readdir(p);
	if (!ptr)
		return(NULL);

	dname = ptr->d_name;

#ifdef NEXT2
	if (telldir(p) < 0)
		return(NULL);
#endif

#ifdef HAVE_BROKEN_READDIR
	/* using /usr/ucb/cc is BAD */
	dname = dname - 2;
#endif

	{
		static pstring buf;
		int len = NAMLEN(ptr);
		memcpy(buf, dname, len);
		buf[len] = 0;
		dname = buf;
	}

	return(dname);
}

/****************************************************************************
  Recursive file matching function act as find
  match must be always set to True when calling this function
****************************************************************************/
static int file_find(struct file_list **list, const char *directory, 
		      const char *expression, BOOL match)
{
	DIR *dir;
	struct file_list *entry;
        struct stat statbuf;
        int ret;
        char *path;
	BOOL isdir;
	const char *dname;

        dir = opendir(directory);
	if (!dir) return -1;
	
        while ((dname = readdirname(dir))) {
		if (!strcmp("..", dname)) continue;
		if (!strcmp(".", dname)) continue;
		
		if (asprintf(&path, "%s/%s", directory, dname) <= 0) {
			continue;
		}

		isdir = False;
		if (!match || !gen_fnmatch(expression, dname)) {
			if (recurse) {
				ret = stat(path, &statbuf);
				if (ret == 0) {
					if (S_ISDIR(statbuf.st_mode)) {
						isdir = True;
						ret = file_find(list, path, expression, False);
					}
				} else {
					d_printf("file_find: cannot stat file %s\n", path);
				}
				
				if (ret == -1) {
					SAFE_FREE(path);
					closedir(dir);
					return -1;
				}
			}
			entry = malloc_p(struct file_list);
			if (!entry) {
				d_printf("Out of memory in file_find\n");
				closedir(dir);
				return -1;
			}
			entry->file_path = path;
			entry->isdir = isdir;
                        DLIST_ADD(*list, entry);
		} else {
			SAFE_FREE(path);
		}
        }

	closedir(dir);
	return 0;
}

/****************************************************************************
  mput some files
  ****************************************************************************/
static int cmd_mput(const char **cmd_ptr)
{
	fstring buf;
	char *p=buf;
	
	while (next_token(cmd_ptr,p,NULL,sizeof(buf))) {
		int ret;
		struct file_list *temp_list;
		char *quest, *lname, *rname;
	
		file_list = NULL;

		ret = file_find(&file_list, ".", p, True);
		if (ret) {
			free_file_list(file_list);
			continue;
		}
		
		quest = NULL;
		lname = NULL;
		rname = NULL;
				
		for (temp_list = file_list; temp_list; 
		     temp_list = temp_list->next) {

			SAFE_FREE(lname);
			if (asprintf(&lname, "%s/", temp_list->file_path) <= 0)
				continue;
			trim_string(lname, "./", "/");
			
			/* check if it's a directory */
			if (temp_list->isdir) {
				/* if (!recurse) continue; */
				
				SAFE_FREE(quest);
				if (asprintf(&quest, "Put directory %s? ", lname) < 0) break;
				if (prompt && !yesno(quest)) { /* No */
					/* Skip the directory */
					lname[strlen(lname)-1] = '/';
					if (!seek_list(temp_list, lname))
						break;		    
				} else { /* Yes */
	      				SAFE_FREE(rname);
					if(asprintf(&rname, "%s%s", cur_dir, lname) < 0) break;
					dos_format(rname);
					if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, rname)) && 
					    NT_STATUS_IS_ERR(do_mkdir(rname))) {
						DEBUG (0, ("Unable to make dir, skipping..."));
						/* Skip the directory */
						lname[strlen(lname)-1] = '/';
						if (!seek_list(temp_list, lname))
							break;
					}
				}
				continue;
			} else {
				SAFE_FREE(quest);
				if (asprintf(&quest,"Put file %s? ", lname) < 0) break;
				if (prompt && !yesno(quest)) /* No */
					continue;
				
				/* Yes */
				SAFE_FREE(rname);
				if (asprintf(&rname, "%s%s", cur_dir, lname) < 0) break;
			}

			dos_format(rname);

			do_put(rname, lname, False);
		}
		free_file_list(file_list);
		SAFE_FREE(quest);
		SAFE_FREE(lname);
		SAFE_FREE(rname);
	}

	return 0;
}


/****************************************************************************
  cancel a print job
  ****************************************************************************/
static int do_cancel(int job)
{
	d_printf("REWRITE: print job cancel not implemented\n");
	return 1;
}


/****************************************************************************
  cancel a print job
  ****************************************************************************/
static int cmd_cancel(const char **cmd_ptr)
{
	fstring buf;
	int job; 

	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("cancel <jobid> ...\n");
		return 1;
	}
	do {
		job = atoi(buf);
		do_cancel(job);
	} while (next_token(cmd_ptr,buf,NULL,sizeof(buf)));
	
	return 0;
}


/****************************************************************************
  print a file
  ****************************************************************************/
static int cmd_print(const char **cmd_ptr)
{
	pstring lname;
	pstring rname;
	char *p;

	if (!next_token(cmd_ptr,lname,NULL, sizeof(lname))) {
		d_printf("print <filename>\n");
		return 1;
	}

	pstrcpy(rname,lname);
	p = strrchr_m(rname,'/');
	if (p) {
		slprintf(rname, sizeof(rname)-1, "%s-%d", p+1, (int)getpid());
	}

	if (strequal(lname,"-")) {
		slprintf(rname, sizeof(rname)-1, "stdin-%d", (int)getpid());
	}

	return do_put(rname, lname, False);
}


/****************************************************************************
 show a print queue
****************************************************************************/
static int cmd_queue(const char **cmd_ptr)
{
	d_printf("REWRITE: print job queue not implemented\n");
	
	return 0;
}

/****************************************************************************
delete some files
****************************************************************************/
static int cmd_del(const char **cmd_ptr)
{
	pstring mask;
	fstring buf;
	uint16_t attribute = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;

	if (recurse)
		attribute |= FILE_ATTRIBUTE_DIRECTORY;
	
	pstrcpy(mask,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("del <filename>\n");
		return 1;
	}
	pstrcat(mask,buf);

	if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, mask))) {
		d_printf("%s deleting remote file %s\n",smbcli_errstr(cli->tree),mask);
	}
	
	return 0;
}


/****************************************************************************
delete a whole directory tree
****************************************************************************/
static int cmd_deltree(const char **cmd_ptr)
{
	pstring dname;
	fstring buf;
	int ret;

	pstrcpy(dname,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("deltree <dirname>\n");
		return 1;
	}
	pstrcat(dname,buf);

	ret = smbcli_deltree(cli->tree, dname);

	if (ret == -1) {
		printf("Failed to delete tree %s - %s\n", dname, smbcli_errstr(cli->tree));
		return -1;
	}

	printf("Deleted %d files in %s\n", ret, dname);
	
	return 0;
}


/****************************************************************************
show as much information as possible about a file
****************************************************************************/
static int cmd_allinfo(const char **cmd_ptr)
{
	pstring fname;
	fstring buf;
	int ret = 0;
	TALLOC_CTX *mem_ctx;
	union smb_fileinfo finfo;
	NTSTATUS status;

	pstrcpy(fname,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("allinfo <filename>\n");
		return 1;
	}
	pstrcat(fname,buf);

	mem_ctx = talloc_init("%s", fname);

	/* first a ALL_INFO QPATHINFO */
	finfo.generic.level = RAW_FILEINFO_ALL_INFO;
	finfo.generic.in.fname = fname;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s - %s\n", fname, nt_errstr(status));
		ret = 1;
		goto done;
	}

	d_printf("\tcreate_time:    %s\n", nt_time_string(mem_ctx, finfo.all_info.out.create_time));
	d_printf("\taccess_time:    %s\n", nt_time_string(mem_ctx, finfo.all_info.out.access_time));
	d_printf("\twrite_time:     %s\n", nt_time_string(mem_ctx, finfo.all_info.out.write_time));
	d_printf("\tchange_time:    %s\n", nt_time_string(mem_ctx, finfo.all_info.out.change_time));
	d_printf("\tattrib:         0x%x\n", finfo.all_info.out.attrib);
	d_printf("\talloc_size:     %lu\n", (unsigned long)finfo.all_info.out.alloc_size);
	d_printf("\tsize:           %lu\n", (unsigned long)finfo.all_info.out.size);
	d_printf("\tnlink:          %u\n", finfo.all_info.out.nlink);
	d_printf("\tdelete_pending: %u\n", finfo.all_info.out.delete_pending);
	d_printf("\tdirectory:      %u\n", finfo.all_info.out.directory);
	d_printf("\tea_size:        %u\n", finfo.all_info.out.ea_size);
	d_printf("\tfname:          '%s'\n", finfo.all_info.out.fname.s);

	/* 8.3 name if any */
	finfo.generic.level = RAW_FILEINFO_ALT_NAME_INFO;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\talt_name:       %s\n", finfo.alt_name_info.out.fname.s);
	}

	/* file_id if available */
	finfo.generic.level = RAW_FILEINFO_INTERNAL_INFORMATION;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\tfile_id         %.0f\n", 
			 (double)finfo.internal_information.out.file_id);
	}

	/* the EAs, if any */
	finfo.generic.level = RAW_FILEINFO_ALL_EAS;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		for (i=0;i<finfo.all_eas.out.num_eas;i++) {
			d_printf("\tEA[%d] flags=%d len=%d '%s'\n", i,
				 finfo.all_eas.out.eas[i].flags,
				 finfo.all_eas.out.eas[i].value.length,
				 finfo.all_eas.out.eas[i].name.s);
		}
	}

	/* streams, if available */
	finfo.generic.level = RAW_FILEINFO_STREAM_INFO;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		for (i=0;i<finfo.stream_info.out.num_streams;i++) {
			d_printf("\tstream %d:\n", i);
			d_printf("\t\tsize       %ld\n", 
				 (long)finfo.stream_info.out.streams[i].size);
			d_printf("\t\talloc size %ld\n", 
				 (long)finfo.stream_info.out.streams[i].alloc_size);
			d_printf("\t\tname       %s\n", finfo.stream_info.out.streams[i].stream_name.s);
		}
	}	

	/* dev/inode if available */
	finfo.generic.level = RAW_FILEINFO_COMPRESSION_INFORMATION;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	if (NT_STATUS_IS_OK(status)) {
		d_printf("\tcompressed size %ld\n", (long)finfo.compression_info.out.compressed_size);
		d_printf("\tformat          %ld\n", (long)finfo.compression_info.out.format);
		d_printf("\tunit_shift      %ld\n", (long)finfo.compression_info.out.unit_shift);
		d_printf("\tchunk_shift     %ld\n", (long)finfo.compression_info.out.chunk_shift);
		d_printf("\tcluster_shift   %ld\n", (long)finfo.compression_info.out.cluster_shift);
	}

	talloc_destroy(mem_ctx);

done:
	return ret;
}


/****************************************************************************
shows EA contents
****************************************************************************/
static int cmd_eainfo(const char **cmd_ptr)
{
	pstring fname;
	fstring buf;
	int ret = 0;
	TALLOC_CTX *mem_ctx;
	union smb_fileinfo finfo;
	NTSTATUS status;
	int i;

	pstrcpy(fname,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("eainfo <filename>\n");
		return 1;
	}
	pstrcat(fname,buf);

	mem_ctx = talloc_init("%s", fname);

	finfo.generic.in.fname = fname;
	finfo.generic.level = RAW_FILEINFO_ALL_EAS;
	status = smb_raw_pathinfo(cli->tree, mem_ctx, &finfo);
	
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("RAW_FILEINFO_ALL_EAS - %s\n", nt_errstr(status));
		talloc_destroy(mem_ctx);
		return 1;
	}

	d_printf("%s has %d EAs\n", fname, finfo.all_eas.out.num_eas);

	for (i=0;i<finfo.all_eas.out.num_eas;i++) {
		d_printf("\tEA[%d] flags=%d len=%d '%s'\n", i,
			 finfo.all_eas.out.eas[i].flags,
			 finfo.all_eas.out.eas[i].value.length,
			 finfo.all_eas.out.eas[i].name.s);
		fflush(stdout);
		dump_data(0, 
			  finfo.all_eas.out.eas[i].value.data,
			  finfo.all_eas.out.eas[i].value.length);
	}

	talloc_destroy(mem_ctx);

	return ret;
}


/****************************************************************************
show any ACL on a file
****************************************************************************/
static int cmd_acl(const char **cmd_ptr)
{
	pstring fname;
	fstring buf;
	int ret = 0;
	TALLOC_CTX *mem_ctx;
	union smb_fileinfo query;
	NTSTATUS status;
	int fnum;

	pstrcpy(fname,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("acl <filename>\n");
		return 1;
	}
	pstrcat(fname,buf);

	fnum = smbcli_nt_create_full(cli->tree, fname, 0, 
				     SEC_STD_READ_CONTROL,
				     0,
				     NTCREATEX_SHARE_ACCESS_DELETE|
				     NTCREATEX_SHARE_ACCESS_READ|
				     NTCREATEX_SHARE_ACCESS_WRITE, 
				     NTCREATEX_DISP_OPEN,
				     0, 0);
	if (fnum == -1) {
		d_printf("%s - %s\n", fname, smbcli_errstr(cli->tree));
		return -1;
	}

	mem_ctx = talloc_init("%s", fname);

	query.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	query.query_secdesc.in.fnum = fnum;
	query.query_secdesc.in.secinfo_flags = 0x7;

	status = smb_raw_fileinfo(cli->tree, mem_ctx, &query);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("%s - %s\n", fname, nt_errstr(status));
		ret = 1;
		goto done;
	}

	NDR_PRINT_DEBUG(security_descriptor, query.query_secdesc.out.sd);

	talloc_destroy(mem_ctx);

done:
	return ret;
}

/****************************************************************************
lookup a name or sid
****************************************************************************/
static int cmd_lookup(const char **cmd_ptr)
{
	fstring buf;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	NTSTATUS status;
	struct dom_sid *sid;

	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("lookup <sid|name>\n");
		talloc_free(mem_ctx);
		return 1;
	}

	sid = dom_sid_parse_talloc(mem_ctx, buf);
	if (sid == NULL) {
		const char *sidstr;
		status = smblsa_lookup_name(cli, buf, mem_ctx, &sidstr);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			talloc_free(mem_ctx);
			return 1;
		}

		d_printf("%s\n", sidstr);
	} else {
		const char *name;
		status = smblsa_lookup_sid(cli, buf, mem_ctx, &name);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupSids - %s\n", nt_errstr(status));
			talloc_free(mem_ctx);
			return 1;
		}

		d_printf("%s\n", name);
	}

	talloc_free(mem_ctx);

	return 0;
}

/****************************************************************************
show privileges for a user
****************************************************************************/
static int cmd_privileges(const char **cmd_ptr)
{
	fstring buf;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	NTSTATUS status;
	struct dom_sid *sid;
	struct lsa_RightSet rights;
	unsigned i;

	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("privileges <sid|name>\n");
		talloc_free(mem_ctx);
		return 1;
	}

	sid = dom_sid_parse_talloc(mem_ctx, buf);
	if (sid == NULL) {
		const char *sid_str;
		status = smblsa_lookup_name(cli, buf, mem_ctx, &sid_str);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			talloc_free(mem_ctx);
			return 1;
		}
		sid = dom_sid_parse_talloc(mem_ctx, sid_str);
	}

	status = smblsa_sid_privileges(cli, sid, mem_ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("lsa_EnumAccountRights - %s\n", nt_errstr(status));
		talloc_free(mem_ctx);
		return 1;
	}

	for (i=0;i<rights.count;i++) {
		d_printf("\t%s\n", rights.names[i].string);
	}

	talloc_free(mem_ctx);

	return 0;
}


/****************************************************************************
add privileges for a user
****************************************************************************/
static int cmd_addprivileges(const char **cmd_ptr)
{
	fstring buf;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	NTSTATUS status;
	struct dom_sid *sid;
	struct lsa_RightSet rights;

	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("addprivileges <sid|name> <privilege...>\n");
		talloc_free(mem_ctx);
		return 1;
	}

	sid = dom_sid_parse_talloc(mem_ctx, buf);
	if (sid == NULL) {
		const char *sid_str;
		status = smblsa_lookup_name(cli, buf, mem_ctx, &sid_str);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			talloc_free(mem_ctx);
			return 1;
		}
		sid = dom_sid_parse_talloc(mem_ctx, sid_str);
	}

	ZERO_STRUCT(rights);
	while (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		rights.names = talloc_realloc_p(mem_ctx, rights.names, 
						struct lsa_String, rights.count+1);
		rights.names[rights.count].string = talloc_strdup(mem_ctx, buf);
		rights.count++;
	}


	status = smblsa_sid_add_privileges(cli, sid, mem_ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("lsa_AddAccountRights - %s\n", nt_errstr(status));
		talloc_free(mem_ctx);
		return 1;
	}

	talloc_free(mem_ctx);

	return 0;
}

/****************************************************************************
delete privileges for a user
****************************************************************************/
static int cmd_delprivileges(const char **cmd_ptr)
{
	fstring buf;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	NTSTATUS status;
	struct dom_sid *sid;
	struct lsa_RightSet rights;

	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("delprivileges <sid|name> <privilege...>\n");
		talloc_free(mem_ctx);
		return 1;
	}

	sid = dom_sid_parse_talloc(mem_ctx, buf);
	if (sid == NULL) {
		const char *sid_str;
		status = smblsa_lookup_name(cli, buf, mem_ctx, &sid_str);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("lsa_LookupNames - %s\n", nt_errstr(status));
			talloc_free(mem_ctx);
			return 1;
		}
		sid = dom_sid_parse_talloc(mem_ctx, sid_str);
	}

	ZERO_STRUCT(rights);
	while (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		rights.names = talloc_realloc_p(mem_ctx, rights.names, 
						struct lsa_String, rights.count+1);
		rights.names[rights.count].string = talloc_strdup(mem_ctx, buf);
		rights.count++;
	}


	status = smblsa_sid_del_privileges(cli, sid, mem_ctx, &rights);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("lsa_RemoveAccountRights - %s\n", nt_errstr(status));
		talloc_free(mem_ctx);
		return 1;
	}

	talloc_free(mem_ctx);

	return 0;
}


/****************************************************************************
****************************************************************************/
static int cmd_open(const char **cmd_ptr)
{
	pstring mask;
	fstring buf;
	
	pstrcpy(mask,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("open <filename>\n");
		return 1;
	}
	pstrcat(mask,buf);

	smbcli_open(cli->tree, mask, O_RDWR, DENY_ALL);

	return 0;
}


/****************************************************************************
remove a directory
****************************************************************************/
static int cmd_rmdir(const char **cmd_ptr)
{
	pstring mask;
	fstring buf;
  
	pstrcpy(mask,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		d_printf("rmdir <dirname>\n");
		return 1;
	}
	pstrcat(mask,buf);

	if (NT_STATUS_IS_ERR(smbcli_rmdir(cli->tree, mask))) {
		d_printf("%s removing remote directory file %s\n",
			 smbcli_errstr(cli->tree),mask);
	}
	
	return 0;
}

/****************************************************************************
 UNIX hardlink.
****************************************************************************/
static int cmd_link(const char **cmd_ptr)
{
	pstring src,dest;
	fstring buf,buf2;
  
	if (!(cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	pstrcpy(src,cur_dir);
	pstrcpy(dest,cur_dir);
  
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf)) || 
	    !next_token(cmd_ptr,buf2,NULL, sizeof(buf2))) {
		d_printf("link <src> <dest>\n");
		return 1;
	}

	pstrcat(src,buf);
	pstrcat(dest,buf2);

	if (NT_STATUS_IS_ERR(smbcli_unix_hardlink(cli->tree, src, dest))) {
		d_printf("%s linking files (%s -> %s)\n", smbcli_errstr(cli->tree), src, dest);
		return 1;
	}  

	return 0;
}

/****************************************************************************
 UNIX symlink.
****************************************************************************/

static int cmd_symlink(const char **cmd_ptr)
{
	pstring src,dest;
	fstring buf,buf2;
  
	if (!(cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	pstrcpy(src,cur_dir);
	pstrcpy(dest,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf)) || 
	    !next_token(cmd_ptr,buf2,NULL, sizeof(buf2))) {
		d_printf("symlink <src> <dest>\n");
		return 1;
	}

	pstrcat(src,buf);
	pstrcat(dest,buf2);

	if (NT_STATUS_IS_ERR(smbcli_unix_symlink(cli->tree, src, dest))) {
		d_printf("%s symlinking files (%s -> %s)\n",
			smbcli_errstr(cli->tree), src, dest);
		return 1;
	} 

	return 0;
}

/****************************************************************************
 UNIX chmod.
****************************************************************************/

static int cmd_chmod(const char **cmd_ptr)
{
	pstring src;
	mode_t mode;
	fstring buf, buf2;
  
	if (!(cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	pstrcpy(src,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf)) || 
	    !next_token(cmd_ptr,buf2,NULL, sizeof(buf2))) {
		d_printf("chmod mode file\n");
		return 1;
	}

	mode = (mode_t)strtol(buf, NULL, 8);
	pstrcat(src,buf2);

	if (NT_STATUS_IS_ERR(smbcli_unix_chmod(cli->tree, src, mode))) {
		d_printf("%s chmod file %s 0%o\n",
			smbcli_errstr(cli->tree), src, (uint_t)mode);
		return 1;
	} 

	return 0;
}

/****************************************************************************
 UNIX chown.
****************************************************************************/

static int cmd_chown(const char **cmd_ptr)
{
	pstring src;
	uid_t uid;
	gid_t gid;
	fstring buf, buf2, buf3;
  
	if (!(cli->transport->negotiate.capabilities & CAP_UNIX)) {
		d_printf("Server doesn't support UNIX CIFS calls.\n");
		return 1;
	}

	pstrcpy(src,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf)) || 
	    !next_token(cmd_ptr,buf2,NULL, sizeof(buf2)) ||
	    !next_token(cmd_ptr,buf3,NULL, sizeof(buf3))) {
		d_printf("chown uid gid file\n");
		return 1;
	}

	uid = (uid_t)atoi(buf);
	gid = (gid_t)atoi(buf2);
	pstrcat(src,buf3);

	if (NT_STATUS_IS_ERR(smbcli_unix_chown(cli->tree, src, uid, gid))) {
		d_printf("%s chown file %s uid=%d, gid=%d\n",
			smbcli_errstr(cli->tree), src, (int)uid, (int)gid);
		return 1;
	} 

	return 0;
}

/****************************************************************************
rename some files
****************************************************************************/
static int cmd_rename(const char **cmd_ptr)
{
	pstring src,dest;
	fstring buf,buf2;
  
	pstrcpy(src,cur_dir);
	pstrcpy(dest,cur_dir);
	
	if (!next_token(cmd_ptr,buf,NULL,sizeof(buf)) || 
	    !next_token(cmd_ptr,buf2,NULL, sizeof(buf2))) {
		d_printf("rename <src> <dest>\n");
		return 1;
	}

	pstrcat(src,buf);
	pstrcat(dest,buf2);

	if (NT_STATUS_IS_ERR(smbcli_rename(cli->tree, src, dest))) {
		d_printf("%s renaming files\n",smbcli_errstr(cli->tree));
		return 1;
	}
	
	return 0;
}


/****************************************************************************
toggle the prompt flag
****************************************************************************/
static int cmd_prompt(const char **cmd_ptr)
{
	prompt = !prompt;
	DEBUG(2,("prompting is now %s\n",prompt?"on":"off"));
	
	return 1;
}


/****************************************************************************
set the newer than time
****************************************************************************/
static int cmd_newer(const char **cmd_ptr)
{
	fstring buf;
	BOOL ok;
	struct stat sbuf;

	ok = next_token(cmd_ptr,buf,NULL,sizeof(buf));
	if (ok && (stat(buf,&sbuf) == 0)) {
		newer_than = sbuf.st_mtime;
		DEBUG(1,("Getting files newer than %s",
			 asctime(localtime(&newer_than))));
	} else {
		newer_than = 0;
	}

	if (ok && newer_than == 0) {
		d_printf("Error setting newer-than time\n");
		return 1;
	}

	return 0;
}

/****************************************************************************
set the archive level
****************************************************************************/
static int cmd_archive(const char **cmd_ptr)
{
	fstring buf;

	if (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		archive_level = atoi(buf);
	} else
		d_printf("Archive level is %d\n",archive_level);

	return 0;
}

/****************************************************************************
toggle the lowercaseflag
****************************************************************************/
static int cmd_lowercase(const char **cmd_ptr)
{
	lowercase = !lowercase;
	DEBUG(2,("filename lowercasing is now %s\n",lowercase?"on":"off"));

	return 0;
}




/****************************************************************************
toggle the recurse flag
****************************************************************************/
static int cmd_recurse(const char **cmd_ptr)
{
	recurse = !recurse;
	DEBUG(2,("directory recursion is now %s\n",recurse?"on":"off"));

	return 0;
}

/****************************************************************************
toggle the translate flag
****************************************************************************/
static int cmd_translate(const char **cmd_ptr)
{
	translation = !translation;
	DEBUG(2,("CR/LF<->LF and print text translation now %s\n",
		 translation?"on":"off"));

	return 0;
}


/****************************************************************************
do a printmode command
****************************************************************************/
static int cmd_printmode(const char **cmd_ptr)
{
	fstring buf;
	fstring mode;

	if (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		if (strequal(buf,"text")) {
			printmode = 0;      
		} else {
			if (strequal(buf,"graphics"))
				printmode = 1;
			else
				printmode = atoi(buf);
		}
	}

	switch(printmode)
		{
		case 0: 
			fstrcpy(mode,"text");
			break;
		case 1: 
			fstrcpy(mode,"graphics");
			break;
		default: 
			slprintf(mode,sizeof(mode)-1,"%d",printmode);
			break;
		}
	
	DEBUG(2,("the printmode is now %s\n",mode));

	return 0;
}

/****************************************************************************
 do the lcd command
 ****************************************************************************/
static int cmd_lcd(const char **cmd_ptr)
{
	fstring buf;
	pstring d;
	
	if (next_token(cmd_ptr,buf,NULL,sizeof(buf)))
		chdir(buf);
	DEBUG(2,("the local directory is now %s\n",sys_getwd(d)));

	return 0;
}

/****************************************************************************
 get a file restarting at end of local file
 ****************************************************************************/
static int cmd_reget(const char **cmd_ptr)
{
	pstring local_name;
	pstring remote_name;
	char *p;

	pstrcpy(remote_name, cur_dir);
	pstrcat(remote_name, "\\");
	
	p = remote_name + strlen(remote_name);
	
	if (!next_token(cmd_ptr, p, NULL, sizeof(remote_name) - strlen(remote_name))) {
		d_printf("reget <filename>\n");
		return 1;
	}
	pstrcpy(local_name, p);
	dos_clean_name(remote_name);
	
	next_token(cmd_ptr, local_name, NULL, sizeof(local_name));
	
	return do_get(remote_name, local_name, True);
}

/****************************************************************************
 put a file restarting at end of local file
 ****************************************************************************/
static int cmd_reput(const char **cmd_ptr)
{
	pstring local_name;
	pstring remote_name;
	fstring buf;
	char *p = buf;
	struct stat st;
	
	pstrcpy(remote_name, cur_dir);
	pstrcat(remote_name, "\\");
  
	if (!next_token(cmd_ptr, p, NULL, sizeof(buf))) {
		d_printf("reput <filename>\n");
		return 1;
	}
	pstrcpy(local_name, p);
  
	if (!file_exist(local_name, &st)) {
		d_printf("%s does not exist\n", local_name);
		return 1;
	}

	if (next_token(cmd_ptr, p, NULL, sizeof(buf)))
		pstrcat(remote_name, p);
	else
		pstrcat(remote_name, local_name);
	
	dos_clean_name(remote_name);

	return do_put(remote_name, local_name, True);
}


/*
  return a string representing a share type
*/
static const char *share_type_str(uint32_t type)
{
	switch (type & 0xF) {
	case STYPE_DISKTREE: 
		return "Disk";
	case STYPE_PRINTQ: 
		return "Printer";
	case STYPE_DEVICE: 
		return "Device";
	case STYPE_IPC: 
		return "IPC";
	default:
		return "Unknown";
	}
}


/*
  display a list of shares from a level 1 share enum
*/
static void display_share_result(struct srvsvc_NetShareCtr1 *ctr1)
{
	int i;

	for (i=0;i<ctr1->count;i++) {
		struct srvsvc_NetShareInfo1 *info = ctr1->array+i;

		printf("\t%-15s %-10.10s %s\n", 
		       info->name, 
		       share_type_str(info->type), 
		       info->comment);
	}
}



/****************************************************************************
try and browse available shares on a host
****************************************************************************/
static BOOL browse_host(const char *query_host)
{
	struct dcerpc_pipe *p;
	char *binding;
	NTSTATUS status;
	struct srvsvc_NetShareEnumAll r;
	uint32_t resume_handle = 0;
	TALLOC_CTX *mem_ctx = talloc_init("browse_host");
	struct srvsvc_NetShareCtr1 ctr1;

	binding = talloc_asprintf(mem_ctx, "ncacn_np:%s", query_host);

	status = dcerpc_pipe_connect(&p, binding, 
				     DCERPC_SRVSVC_UUID, 
				     DCERPC_SRVSVC_VERSION,
				     domain, 
				     username, password);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Failed to connect to %s - %s\n", 
			 binding, nt_errstr(status));
		talloc_destroy(mem_ctx);
		return False;
	}
	talloc_steal(mem_ctx, p);

	r.in.server_unc = talloc_asprintf(mem_ctx,"\\\\%s",dcerpc_server_name(p));
	r.in.level = 1;
	r.in.ctr.ctr1 = &ctr1;
	r.in.max_buffer = ~0;
	r.in.resume_handle = &resume_handle;

	d_printf("\n\tSharename       Type       Comment\n");
	d_printf("\t---------       ----       -------\n");

	do {
		ZERO_STRUCT(ctr1);
		status = dcerpc_srvsvc_NetShareEnumAll(p, mem_ctx, &r);

		if (NT_STATUS_IS_OK(status) && 
		    (W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA) ||
		     W_ERROR_IS_OK(r.out.result)) &&
		    r.out.ctr.ctr1) {
			display_share_result(r.out.ctr.ctr1);
			resume_handle += r.out.ctr.ctr1->count;
		}
	} while (NT_STATUS_IS_OK(status) && W_ERROR_EQUAL(r.out.result, WERR_MORE_DATA));

	talloc_destroy(mem_ctx);

	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("Failed NetShareEnumAll %s - %s/%s\n", 
			 binding, nt_errstr(status), win_errstr(r.out.result));
		return False;
	}

	return False;
}

/****************************************************************************
try and browse available connections on a host
****************************************************************************/
static BOOL list_servers(const char *wk_grp)
{
	d_printf("REWRITE: list servers not implemented\n");
	return False;
}

/* Some constants for completing filename arguments */

#define COMPL_NONE        0          /* No completions */
#define COMPL_REMOTE      1          /* Complete remote filename */
#define COMPL_LOCAL       2          /* Complete local filename */

/* This defines the commands supported by this client.
 * NOTE: The "!" must be the last one in the list because it's fn pointer
 *       field is NULL, and NULL in that field is used in process_tok()
 *       (below) to indicate the end of the list.  crh
 */
static struct
{
  const char *name;
  int (*fn)(const char **cmd_ptr);
  const char *description;
  char compl_args[2];      /* Completion argument info */
} commands[] = 
{
  {"?",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"addprivileges",cmd_addprivileges,"<sid|name> <privilege...> add privileges for a user",{COMPL_NONE,COMPL_NONE}},
  {"altname",cmd_altname,"<file> show alt name",{COMPL_NONE,COMPL_NONE}},
  {"acl",cmd_acl,"<file> show file ACL",{COMPL_NONE,COMPL_NONE}},
  {"allinfo",cmd_allinfo,"<file> show all possible info about a file",{COMPL_NONE,COMPL_NONE}},
  {"archive",cmd_archive,"<level>\n0=ignore archive bit\n1=only get archive files\n2=only get archive files and reset archive bit\n3=get all files and reset archive bit",{COMPL_NONE,COMPL_NONE}},
  {"cancel",cmd_cancel,"<jobid> cancel a print queue entry",{COMPL_NONE,COMPL_NONE}},
  {"cd",cmd_cd,"[directory] change/report the remote directory",{COMPL_REMOTE,COMPL_NONE}},
  {"chmod",cmd_chmod,"<src> <mode> chmod a file using UNIX permission",{COMPL_REMOTE,COMPL_REMOTE}},
  {"chown",cmd_chown,"<src> <uid> <gid> chown a file using UNIX uids and gids",{COMPL_REMOTE,COMPL_REMOTE}},
  {"del",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"delprivileges",cmd_delprivileges,"<sid|name> <privilege...> remove privileges for a user",{COMPL_NONE,COMPL_NONE}},
  {"deltree",cmd_deltree,"<dir> delete a whole directory tree",{COMPL_REMOTE,COMPL_NONE}},
  {"dir",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"du",cmd_du,"<mask> computes the total size of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"eainfo",cmd_eainfo,"<file> show EA contents for a file",{COMPL_NONE,COMPL_NONE}},
  {"exit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"get",cmd_get,"<remote name> [local name] get a file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"help",cmd_help,"[command] give help on a command",{COMPL_NONE,COMPL_NONE}},
  {"history",cmd_history,"displays the command history",{COMPL_NONE,COMPL_NONE}},
  {"lcd",cmd_lcd,"[directory] change/report the local current working directory",{COMPL_LOCAL,COMPL_NONE}},
  {"link",cmd_link,"<src> <dest> create a UNIX hard link",{COMPL_REMOTE,COMPL_REMOTE}},
  {"lookup",cmd_lookup,"<sid|name> show SID for name or name for SID",{COMPL_NONE,COMPL_NONE}},
  {"lowercase",cmd_lowercase,"toggle lowercasing of filenames for get",{COMPL_NONE,COMPL_NONE}},  
  {"ls",cmd_dir,"<mask> list the contents of the current directory",{COMPL_REMOTE,COMPL_NONE}},
  {"mask",cmd_select,"<mask> mask all filenames against this",{COMPL_REMOTE,COMPL_NONE}},
  {"md",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"mget",cmd_mget,"<mask> get all the matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"mkdir",cmd_mkdir,"<directory> make a directory",{COMPL_NONE,COMPL_NONE}},
  {"more",cmd_more,"<remote name> view a remote file with your pager",{COMPL_REMOTE,COMPL_NONE}},  
  {"mput",cmd_mput,"<mask> put all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"newer",cmd_newer,"<file> only mget files newer than the specified local file",{COMPL_LOCAL,COMPL_NONE}},
  {"open",cmd_open,"<mask> open a file",{COMPL_REMOTE,COMPL_NONE}},
  {"privileges",cmd_privileges,"<user> show privileges for a user",{COMPL_NONE,COMPL_NONE}},
  {"print",cmd_print,"<file name> print a file",{COMPL_NONE,COMPL_NONE}},
  {"printmode",cmd_printmode,"<graphics or text> set the print mode",{COMPL_NONE,COMPL_NONE}},
  {"prompt",cmd_prompt,"toggle prompting for filenames for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"put",cmd_put,"<local name> [remote name] put a file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"pwd",cmd_pwd,"show current remote directory (same as 'cd' with no args)",{COMPL_NONE,COMPL_NONE}},
  {"q",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"queue",cmd_queue,"show the print queue",{COMPL_NONE,COMPL_NONE}},
  {"quit",cmd_quit,"logoff the server",{COMPL_NONE,COMPL_NONE}},
  {"rd",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"recurse",cmd_recurse,"toggle directory recursion for mget and mput",{COMPL_NONE,COMPL_NONE}},  
  {"reget",cmd_reget,"<remote name> [local name] get a file restarting at end of local file",{COMPL_REMOTE,COMPL_LOCAL}},
  {"rename",cmd_rename,"<src> <dest> rename some files",{COMPL_REMOTE,COMPL_REMOTE}},
  {"reput",cmd_reput,"<local name> [remote name] put a file restarting at end of remote file",{COMPL_LOCAL,COMPL_REMOTE}},
  {"rm",cmd_del,"<mask> delete all matching files",{COMPL_REMOTE,COMPL_NONE}},
  {"rmdir",cmd_rmdir,"<directory> remove a directory",{COMPL_NONE,COMPL_NONE}},
  {"symlink",cmd_symlink,"<src> <dest> create a UNIX symlink",{COMPL_REMOTE,COMPL_REMOTE}},
  {"translate",cmd_translate,"toggle text translation for printing",{COMPL_NONE,COMPL_NONE}},
  
  /* Yes, this must be here, see crh's comment above. */
  {"!",NULL,"run a shell command on the local system",{COMPL_NONE,COMPL_NONE}},
  {NULL,NULL,NULL,{COMPL_NONE,COMPL_NONE}}
};


/*******************************************************************
  lookup a command string in the list of commands, including 
  abbreviations
  ******************************************************************/
static int process_tok(fstring tok)
{
	int i = 0, matches = 0;
	int cmd=0;
	int tok_len = strlen(tok);
	
	while (commands[i].fn != NULL) {
		if (strequal(commands[i].name,tok)) {
			matches = 1;
			cmd = i;
			break;
		} else if (strncasecmp(commands[i].name, tok, tok_len) == 0) {
			matches++;
			cmd = i;
		}
		i++;
	}
  
	if (matches == 0)
		return(-1);
	else if (matches == 1)
		return(cmd);
	else
		return(-2);
}

/****************************************************************************
help
****************************************************************************/
static int cmd_help(const char **cmd_ptr)
{
	int i=0,j;
	fstring buf;
	
	if (next_token(cmd_ptr,buf,NULL,sizeof(buf))) {
		if ((i = process_tok(buf)) >= 0)
			d_printf("HELP %s:\n\t%s\n\n",commands[i].name,commands[i].description);
	} else {
		while (commands[i].description) {
			for (j=0; commands[i].description && (j<5); j++) {
				d_printf("%-15s",commands[i].name);
				i++;
			}
			d_printf("\n");
		}
	}
	return 0;
}

/****************************************************************************
process a -c command string
****************************************************************************/
static int process_command_string(char *cmd)
{
	pstring line;
	const char *ptr;
	int rc = 0;

	/* establish the connection if not already */
	
	if (!cli) {
		cli = do_connect(desthost, service);
		if (!cli)
			return 0;
	}
	
	while (cmd[0] != '\0')    {
		char *p;
		fstring tok;
		int i;
		
		if ((p = strchr_m(cmd, ';')) == 0) {
			strncpy(line, cmd, 999);
			line[1000] = '\0';
			cmd += strlen(cmd);
		} else {
			if (p - cmd > 999) p = cmd + 999;
			strncpy(line, cmd, p - cmd);
			line[p - cmd] = '\0';
			cmd = p + 1;
		}
		
		/* and get the first part of the command */
		ptr = line;
		if (!next_token(&ptr,tok,NULL,sizeof(tok))) continue;
		
		if ((i = process_tok(tok)) >= 0) {
			rc = commands[i].fn(&ptr);
		} else if (i == -2) {
			d_printf("%s: command abbreviation ambiguous\n",tok);
		} else {
			d_printf("%s: command not found\n",tok);
		}
	}
	
	return rc;
}	

#define MAX_COMPLETIONS 100

typedef struct {
	pstring dirmask;
	char **matches;
	int count, samelen;
	const char *text;
	int len;
} completion_remote_t;

static void completion_remote_filter(struct clilist_file_info *f, const char *mask, void *state)
{
	completion_remote_t *info = (completion_remote_t *)state;

	if ((info->count < MAX_COMPLETIONS - 1) && (strncmp(info->text, f->name, info->len) == 0) && (strcmp(f->name, ".") != 0) && (strcmp(f->name, "..") != 0)) {
		if ((info->dirmask[0] == 0) && !(f->attrib & FILE_ATTRIBUTE_DIRECTORY))
			info->matches[info->count] = strdup(f->name);
		else {
			pstring tmp;

			if (info->dirmask[0] != 0)
				pstrcpy(tmp, info->dirmask);
			else
				tmp[0] = 0;
			pstrcat(tmp, f->name);
			if (f->attrib & FILE_ATTRIBUTE_DIRECTORY)
				pstrcat(tmp, "/");
			info->matches[info->count] = strdup(tmp);
		}
		if (info->matches[info->count] == NULL)
			return;
		if (f->attrib & FILE_ATTRIBUTE_DIRECTORY)
			smb_readline_ca_char(0);

		if (info->count == 1)
			info->samelen = strlen(info->matches[info->count]);
		else
			while (strncmp(info->matches[info->count], info->matches[info->count-1], info->samelen) != 0)
				info->samelen--;
		info->count++;
	}
}

static char **remote_completion(const char *text, int len)
{
	pstring dirmask;
	int i;
	completion_remote_t info = { "", NULL, 1, 0, NULL, 0 };

	info.samelen = len;
	info.text = text;
	info.len = len;
 
	if (len >= PATH_MAX)
		return(NULL);

	info.matches = malloc_array_p(char *, MAX_COMPLETIONS);
	if (!info.matches) return NULL;
	info.matches[0] = NULL;

	for (i = len-1; i >= 0; i--)
		if ((text[i] == '/') || (text[i] == '\\'))
			break;
	info.text = text+i+1;
	info.samelen = info.len = len-i-1;

	if (i > 0) {
		strncpy(info.dirmask, text, i+1);
		info.dirmask[i+1] = 0;
		snprintf(dirmask, sizeof(dirmask), "%s%*s*", cur_dir, i-1, text);
	} else
		snprintf(dirmask, sizeof(dirmask), "%s*", cur_dir);

	if (smbcli_list(cli->tree, dirmask, 
		     FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN, 
		     completion_remote_filter, &info) < 0)
		goto cleanup;

	if (info.count == 2)
		info.matches[0] = strdup(info.matches[1]);
	else {
		info.matches[0] = malloc(info.samelen+1);
		if (!info.matches[0])
			goto cleanup;
		strncpy(info.matches[0], info.matches[1], info.samelen);
		info.matches[0][info.samelen] = 0;
	}
	info.matches[info.count] = NULL;
	return info.matches;

cleanup:
	for (i = 0; i < info.count; i++)
		free(info.matches[i]);
	free(info.matches);
	return NULL;
}

static char **completion_fn(const char *text, int start, int end)
{
	smb_readline_ca_char(' ');

	if (start) {
		const char *buf, *sp;
		int i;
		char compl_type;

		buf = smb_readline_get_line_buffer();
		if (buf == NULL)
			return NULL;
		
		sp = strchr(buf, ' ');
		if (sp == NULL)
			return NULL;
		
		for (i = 0; commands[i].name; i++)
			if ((strncmp(commands[i].name, text, sp - buf) == 0) && (commands[i].name[sp - buf] == 0))
				break;
		if (commands[i].name == NULL)
			return NULL;

		while (*sp == ' ')
			sp++;

		if (sp == (buf + start))
			compl_type = commands[i].compl_args[0];
		else
			compl_type = commands[i].compl_args[1];

		if (compl_type == COMPL_REMOTE)
			return remote_completion(text, end - start);
		else /* fall back to local filename completion */
			return NULL;
	} else {
		char **matches;
		int i, len, samelen = 0, count=1;

		matches = malloc_array_p(char *, MAX_COMPLETIONS);
		if (!matches) return NULL;
		matches[0] = NULL;

		len = strlen(text);
		for (i=0;commands[i].fn && count < MAX_COMPLETIONS-1;i++) {
			if (strncmp(text, commands[i].name, len) == 0) {
				matches[count] = strdup(commands[i].name);
				if (!matches[count])
					goto cleanup;
				if (count == 1)
					samelen = strlen(matches[count]);
				else
					while (strncmp(matches[count], matches[count-1], samelen) != 0)
						samelen--;
				count++;
			}
		}

		switch (count) {
		case 0:	/* should never happen */
		case 1:
			goto cleanup;
		case 2:
			matches[0] = strdup(matches[1]);
			break;
		default:
			matches[0] = malloc(samelen+1);
			if (!matches[0])
				goto cleanup;
			strncpy(matches[0], matches[1], samelen);
			matches[0][samelen] = 0;
		}
		matches[count] = NULL;
		return matches;

cleanup:
		while (i >= 0) {
			free(matches[i]);
			i--;
		}
		free(matches);
		return NULL;
	}
}

/****************************************************************************
make sure we swallow keepalives during idle time
****************************************************************************/
static void readline_callback(void)
{
	static time_t last_t;
	time_t t;

	t = time(NULL);

	if (t - last_t < 5) return;

	last_t = t;

	smbcli_transport_process(cli->transport);

	if (cli->tree) {
		smbcli_chkpath(cli->tree, "\\");
	}
}


/****************************************************************************
process commands on stdin
****************************************************************************/
static void process_stdin(void)
{
	while (1) {
		fstring tok;
		fstring the_prompt;
		char *cline;
		pstring line;
		const char *ptr;
		int i;
		
		/* display a prompt */
		slprintf(the_prompt, sizeof(the_prompt)-1, "smb: %s> ", cur_dir);
		cline = smb_readline(the_prompt, readline_callback, completion_fn);
			
		if (!cline) break;
		
		pstrcpy(line, cline);

		/* special case - first char is ! */
		if (*line == '!') {
			system(line + 1);
			continue;
		}
      
		/* and get the first part of the command */
		ptr = line;
		if (!next_token(&ptr,tok,NULL,sizeof(tok))) continue;

		if ((i = process_tok(tok)) >= 0) {
			commands[i].fn(&ptr);
		} else if (i == -2) {
			d_printf("%s: command abbreviation ambiguous\n",tok);
		} else {
			d_printf("%s: command not found\n",tok);
		}
	}
}


/***************************************************** 
return a connection to a server
*******************************************************/
static struct smbcli_state *do_connect(const char *server, const char *share)
{
	struct smbcli_state *c;
	NTSTATUS status;

	if (strncmp(share, "\\\\", 2) == 0 ||
	    strncmp(share, "//", 2) == 0) {
		smbcli_parse_unc(share, NULL, &server, &share);
	}
	
	status = smbcli_full_connection(NULL, &c, lp_netbios_name(), server,
					share, NULL, username, domain, password);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("Connection to \\\\%s\\%s failed - %s\n", 
			 server, share, nt_errstr(status));
		return NULL;
	}

	return c;
}


/****************************************************************************
  process commands from the client
****************************************************************************/
static int process(char *base_directory)
{
	int rc = 0;

	cli = do_connect(desthost, service);
	if (!cli) {
		return 1;
	}

	if (*base_directory) do_cd(base_directory);
	
	if (cmdstr) {
		rc = process_command_string(cmdstr);
	} else {
		process_stdin();
	}
  
	smbcli_shutdown(cli);
	return rc;
}

/****************************************************************************
handle a -L query
****************************************************************************/
static int do_host_query(char *query_host)
{
	browse_host(query_host);
	list_servers(lp_workgroup());
	return(0);
}


/****************************************************************************
handle a message operation
****************************************************************************/
static int do_message_op(void)
{
	struct nmb_name called, calling;
	const char *server_name;

	make_nmb_name(&calling, lp_netbios_name(), 0x0);
	choose_called_name(&called, desthost, name_type);

	server_name = dest_ip ? dest_ip : desthost;

	if (!(cli=smbcli_state_init(NULL)) || !smbcli_socket_connect(cli, server_name)) {
		d_printf("Connection to %s failed\n", server_name);
		return 1;
	}

	if (!smbcli_transport_establish(cli, &calling, &called)) {
		d_printf("session request failed\n");
		smbcli_shutdown(cli);
		return 1;
	}

	send_message();
	smbcli_shutdown(cli);

	return 0;
}


/**
 * Process "-L hostname" option.
 *
 * We don't actually do anything yet -- we just stash the name in a
 * global variable and do the query when all options have been read.
 **/
static void remember_query_host(const char *arg,
				pstring query_host)
{
	char *slash;
	
	while (*arg == '\\' || *arg == '/')
		arg++;
	pstrcpy(query_host, arg);
	if ((slash = strchr(query_host, '/'))
	    || (slash = strchr(query_host, '\\'))) {
		*slash = 0;
	}
}


/****************************************************************************
  main program
****************************************************************************/
 int main(int argc,char *argv[])
{
	fstring base_directory;
	int opt;
	pstring query_host;
	BOOL message = False;
	pstring term_code;
	poptContext pc;
	char *p;
	int rc = 0;
	TALLOC_CTX *mem_ctx;
	struct poptOption long_options[] = {
		POPT_AUTOHELP

		{ "message", 'M', POPT_ARG_STRING, NULL, 'M', "Send message", "HOST" },
		{ "ip-address", 'I', POPT_ARG_STRING, NULL, 'I', "Use this IP to connect to", "IP" },
		{ "stderr", 'E', POPT_ARG_NONE, NULL, 'E', "Write messages to stderr instead of stdout" },
		{ "list", 'L', POPT_ARG_STRING, NULL, 'L', "Get a list of shares available on a host", "HOST" },
		{ "terminal", 't', POPT_ARG_STRING, NULL, 't', "Terminal I/O code {sjis|euc|jis7|jis8|junet|hex}", "CODE" },
		{ "directory", 'D', POPT_ARG_STRING, NULL, 'D', "Start from directory", "DIR" },
		{ "command", 'c', POPT_ARG_STRING, &cmdstr, 'c', "Execute semicolon separated commands" }, 
		{ "send-buffer", 'b', POPT_ARG_INT, NULL, 'b', "Changes the transmit/send buffer", "BYTES" },
		{ "port", 'p', POPT_ARG_INT, &port, 'p', "Port to connect to", "PORT" },
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		POPT_COMMON_CREDENTIALS
		POPT_COMMON_VERSION
		POPT_TABLEEND
	};
	
#ifdef KANJI
	pstrcpy(term_code, KANJI);
#else /* KANJI */
	*term_code = 0;
#endif /* KANJI */

	*query_host = 0;
	*base_directory = 0;

	setup_logging(argv[0],DEBUG_STDOUT);
	mem_ctx = talloc_init("client.c/main");
	if (!mem_ctx) {
		d_printf("\nclient.c: Not enough memory\n");
		exit(1);
	}

	if (!lp_load(dyn_CONFIGFILE,True,False,False)) {
		fprintf(stderr, "%s: Can't load %s - run testparm to debug it\n",
			argv[0], dyn_CONFIGFILE);
	}
	
	pc = poptGetContext("smbclient", argc, (const char **) argv, long_options, 0);
	poptSetOtherOptionHelp(pc, "[OPTIONS] service <password>");

	in_client = True;   /* Make sure that we tell lp_load we are */

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'M':
			/* Messages are sent to NetBIOS name type 0x3
			 * (Messenger Service).  Make sure we default
			 * to port 139 instead of port 445. srl,crh
			 */
			name_type = 0x03; 
			pstrcpy(desthost,poptGetOptArg(pc));
			if( 0 == port ) port = 139;
 			message = True;
 			break;
		case 'I':
			dest_ip = poptGetOptArg(pc);
			break;
		case 'E':
			setup_logging("client", DEBUG_STDERR);
			break;

		case 'L':
			remember_query_host(poptGetOptArg(pc), query_host);
			break;
		case 't':
			pstrcpy(term_code, poptGetOptArg(pc));
			break;
		case 'D':
			fstrcpy(base_directory,poptGetOptArg(pc));
			break;
		case 'b':
			io_bufsize = MAX(1, atoi(poptGetOptArg(pc)));
			break;
		}
	}

	load_interfaces();

	smbclient_init_subsystems;

	if(poptPeekArg(pc)) {
		pstrcpy(service,poptGetArg(pc));  
		/* Convert any '/' characters in the service name to '\' characters */
		string_replace(service, '/','\\');

		if (count_chars(service,'\\') < 3) {
			d_printf("\n%s: Not enough '\\' characters in service\n",service);
			poptPrintUsage(pc, stderr, 0);
			exit(1);
		}
	}

	if (poptPeekArg(pc)) { 
		cmdline_set_userpassword(poptGetArg(pc));
	}

	/*init_names(); */

	if (!*query_host && !*service && !message) {
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}

	poptFreeContext(pc);

	pstrcpy(username, cmdline_get_username());
	pstrcpy(domain, cmdline_get_userdomain());
	pstrcpy(password, cmdline_get_userpassword());

	DEBUG( 3, ( "Client started (version %s).\n", SAMBA_VERSION_STRING ) );

	talloc_destroy(mem_ctx);

	if ((p=strchr_m(query_host,'#'))) {
		*p = 0;
		p++;
		sscanf(p, "%x", &name_type);
	}
  
	if (*query_host) {
		return do_host_query(query_host);
	}

	if (message) {
		return do_message_op();
	}
	
	if (process(base_directory)) {
		return 1;
	}

	return rc;
}
