/* 
   Samba Unix/Linux SMB client library 
   Distributed SMB/CIFS Server Management Utility 
   Copyright (C) 2001 Steve French  (sfrench@us.ibm.com)
   Copyright (C) 2001 Jim McDonough (jmcd@us.ibm.com)
   Copyright (C) 2001 Andrew Tridgell (tridge@samba.org)
   Copyright (C) 2001 Andrew Bartlett (abartlet@samba.org)

   Originally written by Steve and Jim. Largely rewritten by tridge in
   November 2001.

   Reworked again by abartlet in December 2001

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
 
/*****************************************************/
/*                                                   */
/*   Distributed SMB/CIFS Server Management Utility  */
/*                                                   */
/*   The intent was to make the syntax similar       */
/*   to the NET utility (first developed in DOS      */
/*   with additional interesting & useful functions  */
/*   added in later SMB server network operating     */
/*   systems).                                       */
/*                                                   */
/*****************************************************/

#include "includes.h"
#include "../utils/net.h"

/***********************************************************************/
/* Beginning of internationalization section.  Translatable constants  */
/* should be kept in this area and referenced in the rest of the code. */
/*                                                                     */
/* No functions, outside of Samba or LSB (Linux Standards Base) should */
/* be used (if possible).                                              */
/***********************************************************************/

#define YES_STRING              "Yes"
#define NO_STRING               "No"

/************************************************************************************/
/*                       end of internationalization section                        */
/************************************************************************************/

/* Yes, these buggers are globals.... */
const char *opt_requester_name = NULL;
const char *opt_host = NULL; 
const char *opt_password = NULL;
const char *opt_user_name = NULL;
BOOL opt_user_specified = False;
const char *opt_workgroup = NULL;
int opt_long_list_entries = 0;
int opt_reboot = 0;
int opt_force = 0;
int opt_port = 0;
int opt_maxusers = -1;
const char *opt_comment = "";
char *opt_container = "cn=Users";
int opt_flags = -1;
int opt_jobid = 0;
int opt_timeout = 0;
const char *opt_target_workgroup = NULL;
static int opt_machine_pass = 0;

BOOL opt_have_ip = False;
struct in_addr opt_dest_ip;

extern BOOL AllowDebugChange;

/*
  run a function from a function table. If not found then
  call the specified usage function 
*/
int net_run_function(int argc, const char **argv, struct functable *table, 
		     int (*usage_fn)(int argc, const char **argv))
{
	int i;
	
	if (argc < 1) {
		d_printf("\nUsage: \n");
		return usage_fn(argc, argv);
	}
	for (i=0; table[i].funcname; i++) {
		if (StrCaseCmp(argv[0], table[i].funcname) == 0)
			return table[i].fn(argc-1, argv+1);
	}
	d_printf("No command: %s\n", argv[0]);
	return usage_fn(argc, argv);
}


/****************************************************************************
connect to \\server\ipc$  
****************************************************************************/
NTSTATUS connect_to_ipc(struct cli_state **c, struct in_addr *server_ip,
					const char *server_name)
{
	NTSTATUS nt_status;

	if (!opt_password) {
		char *pass = getpass("Password:");
		if (pass) {
			opt_password = strdup(pass);
		}
	}
	
	nt_status = cli_full_connection(c, opt_requester_name, server_name, 
					server_ip, opt_port,
					"IPC$", "IPC",  
					opt_user_name, opt_workgroup,
					opt_password, 0, NULL);
	
	if (NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	} else {
		DEBUG(1,("Cannot connect to server.  Error was %s\n", 
			 nt_errstr(nt_status)));

		/* Display a nicer message depending on the result */

		if (NT_STATUS_V(nt_status) == 
		    NT_STATUS_V(NT_STATUS_LOGON_FAILURE))
			d_printf("The username or password was not correct.\n");

		return nt_status;
	}
}

/****************************************************************************
connect to \\server\ipc$ anonymously
****************************************************************************/
NTSTATUS connect_to_ipc_anonymous(struct cli_state **c,
			struct in_addr *server_ip, const char *server_name)
{
	NTSTATUS nt_status;

	nt_status = cli_full_connection(c, opt_requester_name, server_name, 
					server_ip, opt_port,
					"IPC$", "IPC",  
					"", "",
					"", 0, NULL);
	
	if (NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	} else {
		DEBUG(1,("Cannot connect to server (anonymously).  Error was %s\n", nt_errstr(nt_status)));
		return nt_status;
	}
}

BOOL net_find_server(uint_t flags, struct in_addr *server_ip, char **server_name)
{

	if (opt_host) {
		*server_name = strdup(opt_host);
	}		

	if (opt_have_ip) {
		*server_ip = opt_dest_ip;
		if (!*server_name) {
			*server_name = strdup(inet_ntoa(opt_dest_ip));
		}
	} else if (*server_name) {
		/* resolve the IP address */
		if (!resolve_name(*server_name, server_ip, 0x20))  {
			DEBUG(1,("Unable to resolve server name\n"));
			return False;
		}
	} else if (flags & NET_FLAGS_PDC) {
		struct in_addr pdc_ip;

		if (get_pdc_ip(opt_target_workgroup, &pdc_ip)) {
			fstring dc_name;
			
			if (is_zero_ip(pdc_ip))
				return False;
			
			if (!lookup_dc_name(lp_netbios_name(), opt_target_workgroup, &pdc_ip, dc_name))
				return False;
				
			*server_name = strdup(dc_name);
			*server_ip = pdc_ip;
		}
		
	} else if (flags & NET_FLAGS_DMB) {
		struct in_addr msbrow_ip;
		/*  if (!resolve_name(MSBROWSE, &msbrow_ip, 1)) */
		if (!resolve_name(opt_target_workgroup, &msbrow_ip, 0x1B))  {
			DEBUG(1,("Unable to resolve domain browser via name lookup\n"));
			return False;
		} else {
			*server_ip = msbrow_ip;
		}
		*server_name = strdup(inet_ntoa(opt_dest_ip));
	} else if (flags & NET_FLAGS_MASTER) {
		struct in_addr brow_ips;
		if (!resolve_name(opt_target_workgroup, &brow_ips, 0x1D))  {
				/* go looking for workgroups */
			DEBUG(1,("Unable to resolve master browser via name lookup\n"));
			return False;
		} else {
			*server_ip = brow_ips;
		}
		*server_name = strdup(inet_ntoa(opt_dest_ip));
	} else if (!(flags & NET_FLAGS_LOCALHOST_DEFAULT_INSANE)) {
		extern struct in_addr loopback_ip;
		*server_ip = loopback_ip;
		*server_name = strdup("127.0.0.1");
	}

	if (!server_name || !*server_name) {
		DEBUG(1,("no server to connect to\n"));
		return False;
	}

	return True;
}


BOOL net_find_dc(struct in_addr *server_ip, fstring server_name, const char *domain_name)
{
	if (get_pdc_ip(domain_name, server_ip)) {
		fstring dc_name;
			
		if (is_zero_ip(*server_ip))
			return False;
		
		if (!lookup_dc_name(lp_netbios_name(), domain_name, server_ip, dc_name))
			return False;
			
		safe_strcpy(server_name, dc_name, FSTRING_LEN);
		return True;
	} else
		return False;
}


struct cli_state *net_make_ipc_connection(uint_t flags)
{
	char *server_name = NULL;
	struct in_addr server_ip;
	struct cli_state *cli = NULL;
	NTSTATUS nt_status;

	if (!net_find_server(flags, &server_ip, &server_name)) {
		d_printf("\nUnable to find a suitable server\n");
		return NULL;
	}

	if (flags & NET_FLAGS_ANONYMOUS) {
		nt_status = connect_to_ipc_anonymous(&cli, &server_ip, server_name);
	} else {
		nt_status = connect_to_ipc(&cli, &server_ip, server_name);
	}

	SAFE_FREE(server_name);
	if (NT_STATUS_IS_OK(nt_status)) {
		return cli;
	} else {
		return NULL;
	}
}

static int net_user(int argc, const char **argv)
{
	if (net_ads_check() == 0)
		return net_ads_user(argc, argv);

	/* if server is not specified, default to PDC? */
	if (net_rpc_check(NET_FLAGS_PDC))
		return net_rpc_user(argc, argv);

	return net_rap_user(argc, argv);
}

static int net_group(int argc, const char **argv)
{
	if (net_ads_check() == 0)
		return net_ads_group(argc, argv);

	if (argc == 0 && net_rpc_check(NET_FLAGS_PDC))
		return net_rpc_group(argc, argv);

	return net_rap_group(argc, argv);
}

static int net_join(int argc, const char **argv)
{
	if (net_ads_check() == 0) {
		if (net_ads_join(argc, argv) == 0)
			return 0;
		else
			d_printf("ADS join did not work, trying RPC...\n");
	}
	return net_rpc_join(argc, argv);
}

static int net_share(int argc, const char **argv)
{
	if (net_rpc_check(0))
		return net_rpc_share(argc, argv);
	return net_rap_share(argc, argv);
}

static int net_file(int argc, const char **argv)
{
	if (net_rpc_check(0))
		return net_rpc_file(argc, argv);
	return net_rap_file(argc, argv);
}

/*
 Retrieve our local SID or the SID for the specified name
 */
static int net_getlocalsid(int argc, const char **argv)
{
        DOM_SID sid;
	const char *name;
	fstring sid_str;

	if (argc >= 1) {
		name = argv[0];
        }
	else {
		name = lp_netbios_name();
	}

	if (!secrets_fetch_domain_sid(name, &sid)) {
		DEBUG(0, ("Can't fetch domain SID for name: %s\n", name));	
		return 1;
	}
	sid_to_string(sid_str, &sid);
	d_printf("SID for domain %s is: %s\n", name, sid_str);
	return 0;
}

static int net_setlocalsid(int argc, const char **argv)
{
	DOM_SID sid;

	if ( (argc != 1)
	     || (strncmp(argv[0], "S-1-5-21-", strlen("S-1-5-21-")) != 0)
	     || (!string_to_sid(&sid, argv[0]))
	     || (sid.num_auths != 4)) {
		d_printf("usage: net setlocalsid S-1-5-21-x-y-z\n");
		return 1;
	}

	if (!secrets_store_domain_sid(lp_netbios_name(), &sid)) {
		DEBUG(0,("Can't store domain SID as a pdc/bdc.\n"));
		return 1;
	}

	return 0;
}

static int net_getdomainsid(int argc, const char **argv)
{
	DOM_SID domain_sid;
	fstring sid_str;

	if (!secrets_fetch_domain_sid(lp_netbios_name(), &domain_sid)) {
		d_printf("Could not fetch local SID\n");
		return 1;
	}
	sid_to_string(sid_str, &domain_sid);
	d_printf("SID for domain %s is: %s\n", lp_netbios_name(), sid_str);

	if (!secrets_fetch_domain_sid(lp_workgroup(), &domain_sid)) {
		d_printf("Could not fetch domain SID\n");
		return 1;
	}

	sid_to_string(sid_str, &domain_sid);
	d_printf("SID for domain %s is: %s\n", lp_workgroup(), sid_str);

	return 0;
}

static uint32_t get_maxrid(void)
{
	SAM_ACCOUNT *pwd = NULL;
	uint32_t max_rid = 0;
	GROUP_MAP *map = NULL;
	int num_entries = 0;
	int i;

	if (!pdb_setsampwent(False)) {
		DEBUG(0, ("load_sampwd_entries: Unable to open passdb.\n"));
		return 0;
	}

	for (; (NT_STATUS_IS_OK(pdb_init_sam(&pwd))) 
		     && pdb_getsampwent(pwd) == True; pwd=NULL) {
		uint32_t rid;

		if (!sid_peek_rid(pdb_get_user_sid(pwd), &rid)) {
			DEBUG(0, ("can't get RID for user '%s'\n",
				  pdb_get_username(pwd)));
			pdb_free_sam(&pwd);
			continue;
		}

		if (rid > max_rid)
			max_rid = rid;

		DEBUG(1,("%d is user '%s'\n", rid, pdb_get_username(pwd)));
		pdb_free_sam(&pwd);
	}

	pdb_endsampwent();
	pdb_free_sam(&pwd);

	if (!pdb_enum_group_mapping(SID_NAME_UNKNOWN, &map, &num_entries,
				    ENUM_ONLY_MAPPED, MAPPING_WITHOUT_PRIV))
		return max_rid;

	for (i = 0; i < num_entries; i++) {
		uint32_t rid;

		if (!sid_peek_check_rid(get_global_sam_sid(), &map[i].sid,
					&rid)) {
			DEBUG(3, ("skipping map for group '%s', SID %s\n",
				  map[i].nt_name,
				  sid_string_static(&map[i].sid)));
			continue;
		}
		DEBUG(1,("%d is group '%s'\n", rid, map[i].nt_name));

		if (rid > max_rid)
			max_rid = rid;
	}

	SAFE_FREE(map);

	return max_rid;
}

static int net_maxrid(int argc, const char **argv)
{
	uint32_t rid;

	if (argc != 0) {
	        DEBUG(0, ("usage: net initrid\n"));
		return 1;
	}

	if ((rid = get_maxrid()) == 0) {
		DEBUG(0, ("can't get current maximum rid\n"));
		return 1;
	}

	d_printf("Currently used maximum rid: %d\n", rid);

	return 0;
}

/* main function table */
static struct functable net_func[] = {
	{"RPC", net_rpc},
	{"RAP", net_rap},
	{"ADS", net_ads},

	/* eventually these should auto-choose the transport ... */
	{"FILE", net_file},
	{"SHARE", net_share},
	{"SESSION", net_rap_session},
	{"SERVER", net_rap_server},
	{"DOMAIN", net_rap_domain},
	{"PRINTQ", net_rap_printq},
	{"USER", net_user},
	{"GROUP", net_group},
	{"VALIDATE", net_rap_validate},
	{"GROUPMEMBER", net_rap_groupmember},
	{"ADMIN", net_rap_admin},
	{"SERVICE", net_rap_service},	
	{"PASSWORD", net_rap_password},
	{"TIME", net_time},
	{"LOOKUP", net_lookup},
	{"JOIN", net_join},
	{"CACHE", net_cache},
	{"GETLOCALSID", net_getlocalsid},
	{"SETLOCALSID", net_setlocalsid},
	{"GETDOMAINSID", net_getdomainsid},
	{"MAXRID", net_maxrid},

	{"HELP", net_help},
	{NULL, NULL}
};


/****************************************************************************
  main program
****************************************************************************/
 int main(int argc, const char **argv)
{
	int opt,i;
	char *p;
	int rc = 0;
	int argc_new = 0;
	const char ** argv_new;
	poptContext pc;
	static char *servicesf = dyn_CONFIGFILE;
	static char *debuglevel = NULL;

	struct poptOption long_options[] = {
		{"help",	'h', POPT_ARG_NONE,   0, 'h'},
		{"workgroup",	'w', POPT_ARG_STRING, &opt_target_workgroup},
		{"myworkgroup",	'W', POPT_ARG_STRING, &opt_workgroup},
		{"user",	'U', POPT_ARG_STRING, &opt_user_name, 'U'},
		{"ipaddress",	'I', POPT_ARG_STRING, 0,'I'},
		{"port",	'p', POPT_ARG_INT,    &opt_port},
		{"myname",	'n', POPT_ARG_STRING, &opt_requester_name},
		{"conf",	's', POPT_ARG_STRING, &servicesf},
		{"server",	'S', POPT_ARG_STRING, &opt_host},
		{"container",	'c', POPT_ARG_STRING, &opt_container},
		{"comment",	'C', POPT_ARG_STRING, &opt_comment},
		{"maxusers",	'M', POPT_ARG_INT,    &opt_maxusers},
		{"flags",	'F', POPT_ARG_INT,    &opt_flags},
		{"jobid",	'j', POPT_ARG_INT,    &opt_jobid},
		{"long",	'l', POPT_ARG_NONE,   &opt_long_list_entries},
		{"reboot",	'r', POPT_ARG_NONE,   &opt_reboot},
		{"force",	'f', POPT_ARG_NONE,   &opt_force},
		{"timeout",	't', POPT_ARG_INT,    &opt_timeout},
		{"machine-pass",'P', POPT_ARG_NONE,   &opt_machine_pass},
		{"debuglevel",  'd', POPT_ARG_STRING, &debuglevel},
		{NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_common_version},
		{ 0, 0, 0, 0}
	};

	zero_ip(&opt_dest_ip);

	dbf = x_stderr;
	
	pc = poptGetContext(NULL, argc, (const char **) argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);
	
	while((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'h':
			net_help(argc, argv);
			exit(0);
			break;
		case 'I':
			opt_dest_ip = *interpret_addr2(poptGetOptArg(pc));
			if (is_zero_ip(opt_dest_ip))
				d_printf("\nInvalid ip address specified\n");
			else
				opt_have_ip = True;
			break;
		case 'U':
			opt_user_specified = True;
			opt_user_name = strdup(opt_user_name);
			p = strchr(opt_user_name,'%');
			if (p) {
				*p = 0;
				opt_password = p+1;
			}
			break;
		default:
			d_printf("\nInvalid option %s: %s\n", 
				 poptBadOption(pc, 0), poptStrerror(opt));
			net_help(argc, argv);
			exit(1);
		}
	}

	if (debuglevel) {
		debug_parse_levels(debuglevel);
		AllowDebugChange = False;
	}

	lp_load(servicesf,True,False,False);       

	argv_new = (const char **)poptGetArgs(pc);

	argc_new = argc;
	for (i=0; i<argc; i++) {
		if (argv_new[i] == NULL) {
			argc_new = i;
			break;
		}
	}

	if (!opt_requester_name) {
		opt_requester_name = get_myname();
	}

	if (!opt_user_name && getenv("LOGNAME")) {
		opt_user_name = getenv("LOGNAME");
	}

	if (!opt_workgroup) {
		opt_workgroup = lp_workgroup();
	}
	
	if (!opt_target_workgroup) {
		opt_target_workgroup = strdup(lp_workgroup());
	}
	
	if (!init_names())
		exit(1);

	load_interfaces();

	if (opt_machine_pass) {
		char *user;
		/* it is very useful to be able to make ads queries as the
		   machine account for testing purposes and for domain leave */

		if (!secrets_init()) {
			d_printf("ERROR: Unable to open secrets database\n");
			exit(1);
		}

		asprintf(&user,"%s$", lp_netbios_name());
		opt_user_name = user;
		opt_password = secrets_fetch_machine_password();
		if (!opt_password) {
			d_printf("ERROR: Unable to fetch machine password\n");
			exit(1);
		}
	}
  	 
	rc = net_run_function(argc_new-1, argv_new+1, net_func, net_help);
	
	DEBUG(2,("return code = %d\n", rc));
	return rc;
}
