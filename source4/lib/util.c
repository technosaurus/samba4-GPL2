/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2002
   Copyright (C) Simo Sorce 2001
   Copyright (C) Anthony Liguori 2003
   Copyright (C) James J Myers 2003
   
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

/**************************************************************************n
 Find a suitable temporary directory. The result should be copied immediately
 as it may be overwritten by a subsequent call.
****************************************************************************/

const char *tmpdir(void)
{
	char *p;
	if ((p = getenv("TMPDIR")))
		return p;
	return "/tmp";
}

/****************************************************************************
 Determine whether we are in the specified group.
****************************************************************************/

BOOL in_group(gid_t group, gid_t current_gid, int ngroups, const gid_t *groups)
{
	int i;

	if (group == current_gid)
		return(True);

	for (i=0;i<ngroups;i++)
		if (group == groups[i])
			return(True);

	return(False);
}

/*******************************************************************
 Check if a file exists - call vfs_file_exist for samba files.
********************************************************************/

BOOL file_exist(const char *fname,SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_STAT st;
	if (!sbuf)
		sbuf = &st;
  
	if (sys_stat(fname,sbuf) != 0) 
		return(False);

	return((S_ISREG(sbuf->st_mode)) || (S_ISFIFO(sbuf->st_mode)));
}

/*******************************************************************
 Check a files mod time.
********************************************************************/

time_t file_modtime(const char *fname)
{
	SMB_STRUCT_STAT st;
  
	if (sys_stat(fname,&st) != 0) 
		return(0);

	return(st.st_mtime);
}

/*******************************************************************
 Check if a directory exists.
********************************************************************/

BOOL directory_exist(char *dname,SMB_STRUCT_STAT *st)
{
	SMB_STRUCT_STAT st2;
	BOOL ret;

	if (!st)
		st = &st2;

	if (sys_stat(dname,st) != 0) 
		return(False);

	ret = S_ISDIR(st->st_mode);
	if(!ret)
		errno = ENOTDIR;
	return ret;
}

/*******************************************************************
 Returns the size in bytes of the named file.
********************************************************************/
SMB_OFF_T get_file_size(char *file_name)
{
	SMB_STRUCT_STAT buf;
	buf.st_size = 0;
	if(sys_stat(file_name,&buf) != 0)
		return (SMB_OFF_T)-1;
	return(buf.st_size);
}

/*******************************************************************
 Close the low 3 fd's and open dev/null in their place.
********************************************************************/
void close_low_fds(BOOL stderr_too)
{
#ifndef VALGRIND
	int fd;
	int i;

	close(0);
	close(1); 

	if (stderr_too)
		close(2);

	/* try and use up these file descriptors, so silly
		library routines writing to stdout etc won't cause havoc */
	for (i=0;i<3;i++) {
		if (i == 2 && !stderr_too)
			continue;

		fd = sys_open("/dev/null",O_RDWR,0);
		if (fd < 0)
			fd = sys_open("/dev/null",O_WRONLY,0);
		if (fd < 0) {
			DEBUG(0,("Can't open /dev/null\n"));
			return;
		}
		if (fd != i) {
			DEBUG(0,("Didn't get file descriptor %d\n",i));
			return;
		}
	}
#endif
}

/****************************************************************************
 Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
 else
  if SYSV use O_NDELAY
  if BSD use FNDELAY
****************************************************************************/

int set_blocking(int fd, BOOL set)
{
	int val;
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif

	if((val = sys_fcntl_long(fd, F_GETFL, 0)) == -1)
		return -1;
	if(set) /* Turn blocking on - ie. clear nonblock flag */
		val &= ~FLAG_TO_SET;
	else
		val |= FLAG_TO_SET;
	return sys_fcntl_long( fd, F_SETFL, val);
#undef FLAG_TO_SET
}


/*******************************************************************
 Sleep for a specified number of milliseconds.
********************************************************************/

void msleep(uint_t t)
{
	struct timeval tval;  

	tval.tv_sec = t/1000;
	tval.tv_usec = 1000*(t%1000);
	/* this should be the real select - do NOT replace
	   with sys_select() */
	select(0,NULL,NULL,NULL,&tval);
}

/****************************************************************************
 Become a daemon, discarding the controlling terminal.
****************************************************************************/

void become_daemon(BOOL Fork)
{
	if (Fork) {
		if (fork()) {
			_exit(0);
		}
	}

  /* detach from the terminal */
#ifdef HAVE_SETSID
	setsid();
#elif defined(TIOCNOTTY)
	{
		int i = sys_open("/dev/tty", O_RDWR, 0);
		if (i != -1) {
			ioctl(i, (int) TIOCNOTTY, (char *)0);      
			close(i);
		}
	}
#endif /* HAVE_SETSID */

	/* Close fd's 0,1,2. Needed if started by rsh */
	close_low_fds(False);  /* Don't close stderr, let the debug system
				  attach it to the logfile */
}


/****************************************************************************
 Expand a pointer to be a particular size.
****************************************************************************/

void *Realloc(void *p,size_t size)
{
	void *ret=NULL;

	if (size == 0) {
		SAFE_FREE(p);
		DEBUG(5,("Realloc asked for 0 bytes\n"));
		return NULL;
	}

	if (!p)
		ret = (void *)malloc(size);
	else
		ret = (void *)realloc(p,size);

	if (!ret)
		DEBUG(0,("Memory allocation error: failed to expand to %d bytes\n",(int)size));

	return(ret);
}

/****************************************************************************
 Free memory, checks for NULL.
 Use directly SAFE_FREE()
 Exists only because we need to pass a function pointer somewhere --SSS
****************************************************************************/

void safe_free(void *p)
{
	SAFE_FREE(p);
}


/*
  see if a string matches either our primary or one of our secondary 
  netbios aliases. do a case insensitive match
*/
BOOL is_myname(const char *name)
{
	const char **aliases;
	int i;

	if (strcasecmp(name, lp_netbios_name()) == 0) {
		return True;
	}

	aliases = lp_netbios_aliases();
	for (i=0; aliases && aliases[i]; i++) {
		if (strcasecmp(name, aliases[i]) == 0) {
			return True;
		}
	}

	return False;
}


/****************************************************************************
 Get my own name, return in malloc'ed storage.
****************************************************************************/

char* get_myname(void)
{
	char *hostname;
	const int host_name_max = 255;
	char *p;

	hostname = malloc(host_name_max+1);
	*hostname = 0;

	/* get my host name */
	if (gethostname(hostname, host_name_max+1) == -1) {
		DEBUG(0,("gethostname failed\n"));
		return NULL;
	} 

	/* Ensure null termination. */
	hostname[host_name_max] = '\0';

	/* split off any parts after an initial . */
	p = strchr_m(hostname,'.');

	if (p)
		*p = 0;
	
	return hostname;
}

/****************************************************************************
 Get my own name, including domain.
****************************************************************************/

BOOL get_myfullname(char *my_name)
{
	pstring hostname;

	*hostname = 0;

	/* get my host name */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		DEBUG(0,("gethostname failed\n"));
		return False;
	} 

	/* Ensure null termination. */
	hostname[sizeof(hostname)-1] = '\0';

	if (my_name)
		fstrcpy(my_name, hostname);
	return True;
}

/****************************************************************************
 Get my own domain name.
****************************************************************************/

BOOL get_mydomname(fstring my_domname)
{
	pstring hostname;
	char *p;

	*hostname = 0;
	/* get my host name */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		DEBUG(0,("gethostname failed\n"));
		return False;
	} 

	/* Ensure null termination. */
	hostname[sizeof(hostname)-1] = '\0';

	p = strchr_m(hostname, '.');

	if (!p)
		return False;

	p++;
	
	if (my_domname)
		fstrcpy(my_domname, p);

	return True;
}

/****************************************************************************
 Interpret a protocol description string, with a default.
****************************************************************************/

int interpret_protocol(char *str,int def)
{
	if (strequal(str,"NT1"))
		return(PROTOCOL_NT1);
	if (strequal(str,"LANMAN2"))
		return(PROTOCOL_LANMAN2);
	if (strequal(str,"LANMAN1"))
		return(PROTOCOL_LANMAN1);
	if (strequal(str,"CORE"))
		return(PROTOCOL_CORE);
	if (strequal(str,"COREPLUS"))
		return(PROTOCOL_COREPLUS);
	if (strequal(str,"CORE+"))
		return(PROTOCOL_COREPLUS);
  
	DEBUG(0,("Unrecognised protocol level %s\n",str));
  
	return(def);
}

/****************************************************************************
 Return true if a string could be a pure IP address.
****************************************************************************/

BOOL is_ipaddress(const char *str)
{
	BOOL pure_address = True;
	int i;
  
	for (i=0; pure_address && str[i]; i++)
		if (!(isdigit((int)str[i]) || str[i] == '.'))
			pure_address = False;

	/* Check that a pure number is not misinterpreted as an IP */
	pure_address = pure_address && (strchr_m(str, '.') != NULL);

	return pure_address;
}

/****************************************************************************
 Interpret an internet address or name into an IP address in 4 byte form.
****************************************************************************/

uint32_t interpret_addr(const char *str)
{
	struct hostent *hp;
	uint32_t res;

	if (strcmp(str,"0.0.0.0") == 0)
		return(0);
	if (strcmp(str,"255.255.255.255") == 0)
		return(0xFFFFFFFF);

	/* if it's in the form of an IP address then get the lib to interpret it */
	if (is_ipaddress(str)) {
		res = inet_addr(str);
	} else {
		/* otherwise assume it's a network name of some sort and use 
			sys_gethostbyname */
		if ((hp = sys_gethostbyname(str)) == 0) {
			DEBUG(3,("sys_gethostbyname: Unknown host. %s\n",str));
			return 0;
		}

		if(hp->h_addr == NULL) {
			DEBUG(3,("sys_gethostbyname: host address is invalid for host %s\n",str));
			return 0;
		}
		putip((char *)&res,(char *)hp->h_addr);
	}

	if (res == (uint32_t)-1)
		return(0);

	return(res);
}

/*******************************************************************
 A convenient addition to interpret_addr().
******************************************************************/

struct in_addr *interpret_addr2(TALLOC_CTX *mem_ctx, const char *str)
{
	struct in_addr *ret;
	uint32_t a = interpret_addr(str);
	
	ret = talloc(mem_ctx, sizeof(struct in_addr));
	if (!ret) return NULL;
	ret->s_addr = a;
	return(ret);
}

/*******************************************************************
 Check if an IP is the 0.0.0.0.
******************************************************************/

BOOL is_zero_ip(struct in_addr ip)
{
	uint32_t a;
	putip((char *)&a,(char *)&ip);
	return(a == 0);
}

/*******************************************************************
 Set an IP to 0.0.0.0.
******************************************************************/

void zero_ip(struct in_addr *ip)
{
	*ip = inet_makeaddr(0,0);
	return;
}


/*******************************************************************
 Are two IPs on the same subnet?
********************************************************************/

BOOL same_net(struct in_addr ip1,struct in_addr ip2,struct in_addr mask)
{
	uint32_t net1,net2,nmask;

	nmask = ntohl(mask.s_addr);
	net1  = ntohl(ip1.s_addr);
	net2  = ntohl(ip2.s_addr);
            
	return((net1 & nmask) == (net2 & nmask));
}


/****************************************************************************
 Check if a process exists. Does this work on all unixes?
****************************************************************************/

BOOL process_exists(pid_t pid)
{
	/* Doing kill with a non-positive pid causes messages to be
	 * sent to places we don't want. */
	SMB_ASSERT(pid > 0);
	return(kill(pid,0) == 0 || errno != ESRCH);
}

/*******************************************************************
 Convert a gid into a group name.
********************************************************************/

char *gidtoname(TALLOC_CTX *mem_ctx, gid_t gid)
{
	char *name;
	struct group *grp;

	grp = getgrgid(gid);
	if (grp)
		return(grp->gr_name);
	name = talloc_asprintf(mem_ctx, "%d",(int)gid);
	return(name);
}


/*******************************************************************
 Convert a name to a gid_t if possible. Return -1 if not a group. 
********************************************************************/

gid_t nametogid(const char *name)
{
	struct group *grp;
	char *p;
	gid_t g;

	g = (gid_t)strtol(name, &p, 0);
	if ((p != name) && (*p == '\0'))
		return g;

	grp = sys_getgrnam(name);
	if (grp)
		return(grp->gr_gid);
	return (gid_t)-1;
}

/*******************************************************************
 Something really nasty happened - panic !
********************************************************************/

void smb_panic(const char *why)
{
	char *cmd = lp_panic_action();
	int result;

	if (cmd && *cmd) {
		DEBUG(0, ("smb_panic(): calling panic action [%s]\n", cmd));
		result = system(cmd);

		if (result == -1)
			DEBUG(0, ("smb_panic(): fork failed in panic action: %s\n",
				  strerror(errno)));
		else
			DEBUG(0, ("smb_panic(): action returned status %d\n",
				  WEXITSTATUS(result)));
	}
	DEBUG(0,("PANIC: %s\n", why));
	abort();
}

/****************************************************************************
 Simple routine to do POSIX file locking. Cruft in NFS and 64->32 bit mapping
 is dealt with in posix.c
****************************************************************************/

BOOL fcntl_lock(int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	SMB_STRUCT_FLOCK lock;
	int ret;

	DEBUG(8,("fcntl_lock %d %d %.0f %.0f %d\n",fd,op,(double)offset,(double)count,type));

	lock.l_type = type;
	lock.l_whence = SEEK_SET;
	lock.l_start = offset;
	lock.l_len = count;
	lock.l_pid = 0;

	ret = sys_fcntl_ptr(fd,op,&lock);

	if (ret == -1 && errno != 0)
		DEBUG(3,("fcntl_lock: fcntl lock gave errno %d (%s)\n",errno,strerror(errno)));

	/* a lock query */
	if (op == SMB_F_GETLK) {
		if ((ret != -1) &&
				(lock.l_type != F_UNLCK) && 
				(lock.l_pid != 0) && 
				(lock.l_pid != getpid())) {
			DEBUG(3,("fcntl_lock: fd %d is locked by pid %d\n",fd,(int)lock.l_pid));
			return(True);
		}

		/* it must be not locked or locked by me */
		return(False);
	}

	/* a lock set or unset */
	if (ret == -1) {
		DEBUG(3,("fcntl_lock: lock failed at offset %.0f count %.0f op %d type %d (%s)\n",
			(double)offset,(double)count,op,type,strerror(errno)));
		return(False);
	}

	/* everything went OK */
	DEBUG(8,("fcntl_lock: Lock call successful\n"));

	return(True);
}

/*******************************************************************
 Set the remote_arch string based on an enum. This is used in places
where we desperately need to distinguish client type. 
********************************************************************/
void set_remote_arch(struct server_context *smb, enum remote_arch_types type)
{
	const char *arch;

	smb->negotiate.ra_type = type;
	switch (type) {
	case RA_WFWG:
		arch = "WfWg";
		return;
	case RA_OS2:
		arch = "OS2";
		return;
	case RA_WIN95:
		arch = "Win95";
		return;
	case RA_WINNT:
		arch = "WinNT";
		return;
	case RA_WIN2K:
		arch = "Win2K";
		return;
	case RA_WINXP:
		arch = "WinXP";
		return;
	case RA_SAMBA:
		arch = "Samba";
		return;
	default:
		smb->negotiate.ra_type = RA_UNKNOWN;
		arch = "UNKNOWN";
		break;
	}

	sub_set_remote_arch(arch);
}


void print_asc(int level, const uint8_t *buf,int len)
{
	int i;
	for (i=0;i<len;i++)
		DEBUGADD(level,("%c", isprint(buf[i])?buf[i]:'.'));
}

void dump_data(int level, const char *buf1,int len)
{
	const uint8_t *buf = (const uint8_t *)buf1;
	int i=0;
	if (len<=0) return;

	if (!DEBUGLVL(level)) return;
	
	DEBUGADD(level,("[%03X] ",i));
	for (i=0;i<len;) {
		DEBUGADD(level,("%02X ",(int)buf[i]));
		i++;
		if (i%8 == 0) DEBUGADD(level,(" "));
		if (i%16 == 0) {      
			print_asc(level,&buf[i-16],8); DEBUGADD(level,(" "));
			print_asc(level,&buf[i-8],8); DEBUGADD(level,("\n"));
			if (i<len) DEBUGADD(level,("[%03X] ",i));
		}
	}
	if (i%16) {
		int n;
		n = 16 - (i%16);
		DEBUGADD(level,(" "));
		if (n>8) DEBUGADD(level,(" "));
		while (n--) DEBUGADD(level,("   "));
		n = MIN(8,i%16);
		print_asc(level,&buf[i-(i%16)],n); DEBUGADD(level,( " " ));
		n = (i%16) - n;
		if (n>0) print_asc(level,&buf[i-n],n); 
		DEBUGADD(level,("\n"));    
	}	
}

/*****************************************************************
 Possibly replace mkstemp if it is broken.
*****************************************************************/  

int smb_mkstemp(char *template)
{
#if HAVE_SECURE_MKSTEMP
	return mkstemp(template);
#else
	/* have a reasonable go at emulating it. Hope that
	   the system mktemp() isn't completly hopeless */
	char *p = mktemp(template);
	if (!p)
		return -1;
	return open(p, O_CREAT|O_EXCL|O_RDWR, 0600);
#endif
}

/*****************************************************************
 malloc that aborts with smb_panic on fail or zero size.
 *****************************************************************/  

void *smb_xmalloc(size_t size)
{
	void *p;
	if (size == 0)
		smb_panic("smb_xmalloc: called with zero size.\n");
	if ((p = malloc(size)) == NULL)
		smb_panic("smb_xmalloc: malloc fail.\n");
	return p;
}

/**
 Memdup with smb_panic on fail.
**/

void *smb_xmemdup(const void *p, size_t size)
{
	void *p2;
	p2 = smb_xmalloc(size);
	memcpy(p2, p, size);
	return p2;
}

/**
 strdup that aborts on malloc fail.
**/

char *smb_xstrdup(const char *s)
{
	char *s1 = strdup(s);
	if (!s1)
		smb_panic("smb_xstrdup: malloc fail\n");
	return s1;
}


/*
  vasprintf that aborts on malloc fail
*/

 int smb_xvasprintf(char **ptr, const char *format, va_list ap)
{
	int n;
	va_list ap2;

	VA_COPY(ap2, ap);

	n = vasprintf(ptr, format, ap2);
	if (n == -1 || ! *ptr)
		smb_panic("smb_xvasprintf: out of memory");
	return n;
}

/*****************************************************************
 Like strdup but for memory.
*****************************************************************/  

void *memdup(const void *p, size_t size)
{
	void *p2;
	if (size == 0)
		return NULL;
	p2 = malloc(size);
	if (!p2)
		return NULL;
	memcpy(p2, p, size);
	return p2;
}

/*****************************************************************
 Get local hostname and cache result.
*****************************************************************/  

char *myhostname(TALLOC_CTX *mem_ctx)
{
	char *myname, *ret;
	myname = get_myname();
	ret = talloc_strdup(mem_ctx, myname);
	free(myname);
	return ret;

}

/*****************************************************************
 A useful function for returning a path in the Samba lock directory.
*****************************************************************/  

char *lock_path(TALLOC_CTX* mem_ctx, const char *name)
{
	char *fname;

	fname = talloc_strdup(mem_ctx, lp_lockdir());
	trim_string(fname,"","/");
	
	if (!directory_exist(fname,NULL))
		mkdir(fname,0755);
	
	fname = talloc_asprintf(mem_ctx, "%s/%s", fname, name);

	return fname;
}

/**
 * @brief Returns an absolute path to a file in the Samba lib directory.
 *
 * @param name File to find, relative to LIBDIR.
 *
 * @retval Pointer to a talloc'ed string containing the full path.
 **/

char *lib_path(TALLOC_CTX* mem_ctx, const char *name)
{
	char *fname;
	fname = talloc_asprintf(mem_ctx, "%s/%s", dyn_LIBDIR, name);
	return fname;
}

/**
 * @brief Returns the platform specific shared library extension.
 *
 * @retval Pointer to a static #fstring containing the extension.
 **/

const char *shlib_ext(void)
{
  return dyn_SHLIBEXT;
}


/*********************************************************
 Recursive routine that is called by unix_wild_match.
*********************************************************/

static BOOL unix_do_match(char *regexp, char *str)
{
	char *p;

	for( p = regexp; *p && *str; ) {

		switch(*p) {
			case '?':
				str++;
				p++;
				break;

			case '*':

				/*
				 * Look for a character matching 
				 * the one after the '*'.
				 */
				p++;
				if(!*p)
					return True; /* Automatic match */
				while(*str) {

					while(*str && (*p != *str))
						str++;

					/*
					 * Patch from weidel@multichart.de. In the case of the regexp
					 * '*XX*' we want to ensure there are at least 2 'X' characters
					 * in the string after the '*' for a match to be made.
					 */

					{
						int matchcount=0;

						/*
						 * Eat all the characters that match, but count how many there were.
						 */

						while(*str && (*p == *str)) {
							str++;
							matchcount++;
						}

						/*
						 * Now check that if the regexp had n identical characters that
						 * matchcount had at least that many matches.
						 */

						while ( *(p+1) && (*(p+1) == *p)) {
							p++;
							matchcount--;
						}

						if ( matchcount <= 0 )
							return False;
					}

					str--; /* We've eaten the match char after the '*' */

					if(unix_do_match(p, str))
						return True;

					if(!*str)
						return False;
					else
						str++;
				}
				return False;

			default:
				if(*str != *p)
					return False;
				str++;
				p++;
				break;
		}
	}

	if(!*p && !*str)
		return True;

	if (!*p && str[0] == '.' && str[1] == 0)
		return(True);
  
	if (!*str && *p == '?') {
		while (*p == '?')
			p++;
		return(!*p);
	}

	if(!*str && (*p == '*' && p[1] == '\0'))
		return True;

	return False;
}

void dump_data_pw(const char *msg, const uint8_t * data, size_t len)
{
#ifdef DEBUG_PASSWORD
	DEBUG(11, ("%s", msg));
	if (data != NULL && len > 0)
	{
		dump_data(11, data, len);
	}
#endif
}


/* see if a range of memory is all zero. A NULL pointer is considered
   to be all zero */
BOOL all_zero(const char *ptr, uint_t size)
{
	int i;
	if (!ptr) return True;
	for (i=0;i<size;i++) {
		if (ptr[i]) return False;
	}
	return True;
}

