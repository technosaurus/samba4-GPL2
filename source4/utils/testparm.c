/* 
   Unix SMB/CIFS implementation.
   Test validity of smb.conf
   Copyright (C) Karl Auer 1993, 1994-1998

   Extensively modified by Andrew Tridgell, 1995
   Converted to popt by Jelmer Vernooij (jelmer@nl.linux.org), 2002
   
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

/*
 * Testbed for loadparm.c/params.c
 *
 * This module simply loads a specified configuration file and
 * if successful, dumps it's contents to stdout. Note that the
 * operation is performed with DEBUGLEVEL at 3.
 *
 * Useful for a quick 'syntax check' of a configuration file.
 *
 */

#include "includes.h"

extern BOOL AllowDebugChange;

/***********************************************
 Here we do a set of 'hard coded' checks for bad
 configuration settings.
************************************************/

static int do_global_checks(void)
{
	int ret = 0;
	SMB_STRUCT_STAT st;

	if (lp_security() >= SEC_DOMAIN && !lp_encrypted_passwords()) {
		printf("ERROR: in 'security=domain' mode the 'encrypt passwords' parameter must always be set to 'true'.\n");
		ret = 1;
	}

	if (lp_wins_support() && lp_wins_server_list()) {
		printf("ERROR: both 'wins support = true' and 'wins server = <server list>' \
cannot be set in the smb.conf file. nmbd will abort with this setting.\n");
		ret = 1;
	}

	if (!directory_exist(lp_lockdir(), &st)) {
		printf("ERROR: lock directory %s does not exist\n",
		       lp_lockdir());
		ret = 1;
	} else if ((st.st_mode & 0777) != 0755) {
		printf("WARNING: lock directory %s should have permissions 0755 for browsing to work\n",
		       lp_lockdir());
		ret = 1;
	}

	if (!directory_exist(lp_piddir(), &st)) {
		printf("ERROR: pid directory %s does not exist\n",
		       lp_piddir());
		ret = 1;
	}

	/*
	 * Password server sanity checks.
	 */

	if((lp_security() == SEC_SERVER || lp_security() >= SEC_DOMAIN) && !lp_passwordserver()) {
		pstring sec_setting;
		if(lp_security() == SEC_SERVER)
			pstrcpy(sec_setting, "server");
		else if(lp_security() == SEC_DOMAIN)
			pstrcpy(sec_setting, "domain");

		printf("ERROR: The setting 'security=%s' requires the 'password server' parameter be set \
to a valid password server.\n", sec_setting );
		ret = 1;
	}

	
	/*
	 * Check 'hosts equiv' and 'use rhosts' compatibility with 'hostname lookup' value.
	 */

	if(*lp_hosts_equiv() && !lp_hostname_lookups()) {
		printf("ERROR: The setting 'hosts equiv = %s' requires that 'hostname lookups = yes'.\n", lp_hosts_equiv());
		ret = 1;
	}

	/*
	 * Password chat sanity checks.
	 */

	if(lp_security() == SEC_USER && lp_unix_password_sync()) {

		/*
		 * Check that we have a valid lp_passwd_program() if not using pam.
		 */

#ifdef WITH_PAM
		if (!lp_pam_password_change()) {
#endif

			if(lp_passwd_program() == NULL) {
				printf("ERROR: the 'unix password sync' parameter is set and there is no valid 'passwd program' \
parameter.\n" );
				ret = 1;
			} else {
				pstring passwd_prog;
				pstring truncated_prog;
				const char *p;

				pstrcpy( passwd_prog, lp_passwd_program());
				p = passwd_prog;
				*truncated_prog = '\0';
				next_token(&p, truncated_prog, NULL, sizeof(pstring));

				if(access(truncated_prog, F_OK) == -1) {
					printf("ERROR: the 'unix password sync' parameter is set and the 'passwd program' (%s) \
cannot be executed (error was %s).\n", truncated_prog, strerror(errno) );
					ret = 1;
				}
			}

#ifdef WITH_PAM
		}
#endif

		if(lp_passwd_chat() == NULL) {
			printf("ERROR: the 'unix password sync' parameter is set and there is no valid 'passwd chat' \
parameter.\n");
			ret = 1;
		}

		/*
		 * Check that we have a valid script and that it hasn't
		 * been written to expect the old password.
		 */

		if(lp_encrypted_passwords()) {
			if(strstr( lp_passwd_chat(), "%o")!=NULL) {
				printf("ERROR: the 'passwd chat' script [%s] expects to use the old plaintext password \
via the %%o substitution. With encrypted passwords this is not possible.\n", lp_passwd_chat() );
				ret = 1;
			}
		}
	}

	if (strlen(lp_winbind_separator()) != 1) {
		printf("ERROR: the 'winbind separator' parameter must be a single character.\n");
		ret = 1;
	}

	if (*lp_winbind_separator() == '+') {
		printf("'winbind separator = +' might cause problems with group membership.\n");
	}

	if (lp_algorithmic_rid_base() < BASE_RID) {
		/* Try to prevent admin foot-shooting, we can't put algorithmic
		   rids below 1000, that's the 'well known RIDs' on NT */
		printf("'algorithmic rid base' must be equal to or above %lu\n", BASE_RID);
	}

	if (lp_algorithmic_rid_base() & 1) {
		printf("'algorithmic rid base' must be even.\n");
	}

#ifndef HAVE_DLOPEN
	if (lp_preload_modules()) {
		printf("WARNING: 'preload modules = ' set while loading plugins not supported.\n");
	}
#endif

	return ret;
}   

int main(int argc, const char *argv[])
{
	extern char *optarg;
	extern int optind;
	const char *config_file = dyn_CONFIGFILE;
	int s;
	static BOOL silent_mode = False;
	int ret = 0;
	int opt;
	poptContext pc;
	static const char *term_code = "";
	static char *new_local_machine = NULL;
	const char *cname;
	const char *caddr;
	static int show_defaults;

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{"suppress-prompt", 's', POPT_ARG_VAL, &silent_mode, 1, "Suppress prompt for enter"},
		{"verbose", 'v', POPT_ARG_NONE, &show_defaults, 1, "Show default options too"},
		{"server", 'L',POPT_ARG_STRING, &new_local_machine, 0, "Set %%L macro to servername\n"},
		{"encoding", 't', POPT_ARG_STRING, &term_code, 0, "Print parameters with encoding"},
		{NULL, 0, POPT_ARG_INCLUDE_TABLE, popt_common_version},
		{0,0,0,0}
	};

	pc = poptGetContext(NULL, argc, argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);
	poptSetOtherOptionHelp(pc, "[OPTION...] <config-file> [host-name] [host-ip]");

	while((opt = poptGetNextOpt(pc)) != -1);

	setup_logging(poptGetArg(pc), DEBUG_STDOUT);

	if (poptPeekArg(pc)) 
		config_file = poptGetArg(pc);

	cname = poptGetArg(pc);
	caddr = poptGetArg(pc);
	
	if (new_local_machine) {
		set_local_machine_name(new_local_machine);
	}

	dbf = x_stdout;
	DEBUGLEVEL = 2;
	AllowDebugChange = False;

	printf("Load smb config files from %s\n",config_file);

	if (!lp_load(config_file,False,True,False)) {
		printf("Error loading services.\n");
		return(1);
	}

	printf("Loaded services file OK.\n");

	ret = do_global_checks();

	for (s=0;s<1000;s++) {
		if (VALID_SNUM(s))
			if (strlen(lp_servicename(s)) > 8) {
				printf("WARNING: You have some share names that are longer than 8 chars\n");
				printf("These may give errors while browsing or may not be accessible\nto some older clients\n");
				break;
			}
	}

	for (s=0;s<1000;s++) {
		if (VALID_SNUM(s)) {
			const char **deny_list = lp_hostsdeny(s);
			const char **allow_list = lp_hostsallow(s);
			int i;
			if(deny_list) {
				for (i=0; deny_list[i]; i++) {
					char *hasstar = strchr_m(deny_list[i], '*');
					char *hasquery = strchr_m(deny_list[i], '?');
					if(hasstar || hasquery) {
						printf("Invalid character %c in hosts deny list (%s) for service %s.\n",
							   hasstar ? *hasstar : *hasquery, deny_list[i], lp_servicename(s) );
					}
				}
			}

			if(allow_list) {
				for (i=0; allow_list[i]; i++) {
					char *hasstar = strchr_m(allow_list[i], '*');
					char *hasquery = strchr_m(allow_list[i], '?');
					if(hasstar || hasquery) {
						printf("Invalid character %c in hosts allow list (%s) for service %s.\n",
							   hasstar ? *hasstar : *hasquery, allow_list[i], lp_servicename(s) );
					}
				}
			}

			if(lp_level2_oplocks(s) && !lp_oplocks(s)) {
				printf("Invalid combination of parameters for service %s. \
					   Level II oplocks can only be set if oplocks are also set.\n",
					   lp_servicename(s) );
			}
		}
	}


	if (!silent_mode) {
		printf("Server role: ");
		switch(lp_server_role()) {
			case ROLE_STANDALONE:
				printf("ROLE_STANDALONE\n");
				break;
			case ROLE_DOMAIN_MEMBER:
				printf("ROLE_DOMAIN_MEMBER\n");
				break;
			case ROLE_DOMAIN_BDC:
				printf("ROLE_DOMAIN_BDC\n");
				break;
			case ROLE_DOMAIN_PDC:
				printf("ROLE_DOMAIN_PDC\n");
				break;
			default:
				printf("Unknown -- internal error?\n");
				break;
		}
	}

	if (!cname) {
		if (!silent_mode) {
			printf("Press enter to see a dump of your service definitions\n");
			fflush(stdout);
			getc(stdin);
		}
		lp_dump(stdout, show_defaults, lp_numservices());
	}

	if(cname && caddr){
		/* this is totally ugly, a real `quick' hack */
		for (s=0;s<1000;s++) {
			if (VALID_SNUM(s)) {		 
				if (allow_access(lp_hostsdeny(s), lp_hostsallow(s), cname, caddr)) {
					printf("Allow connection from %s (%s) to %s\n",
						   cname,caddr,lp_servicename(s));
				} else {
					printf("Deny connection from %s (%s) to %s\n",
						   cname,caddr,lp_servicename(s));
				}
			}
		}
	}
	return(ret);
}
