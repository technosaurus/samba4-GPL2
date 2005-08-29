/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Jelmer Vernooij 2005

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
#include "system/filesys.h"
#include "system/passwd.h"
#include "lib/cmdline/popt_common.h"

static const char *cmdline_get_userpassword(struct cli_credentials *credentials)
{
	char *prompt;
	char *ret;
	char *domain;
	char *username;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);

	domain = cli_credentials_get_domain(credentials);
	username = cli_credentials_get_username(credentials, mem_ctx);
	if (domain && domain[0]) {
		prompt = talloc_asprintf(mem_ctx, "Password for [%s\\%s]:", 
					 domain, username);
	} else {
		prompt = talloc_asprintf(mem_ctx, "Password for [%s]:", 
					 username);
	}

	ret = getpass(prompt);

	talloc_free(mem_ctx);
	return ret;
}

void cli_credentials_set_cmdline_callbacks(struct cli_credentials *cred)
{
	if (cred->password_obtained <= CRED_CALLBACK) {
		cred->password_cb = cmdline_get_userpassword;
		cred->password_obtained = CRED_CALLBACK;
	}
}
