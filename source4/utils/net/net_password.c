/* 
   Samba Unix/Linux SMB client library 
   Distributed SMB/CIFS Server Management Utility 

   Copyright (C) 2004 Stefan Metzmacher (metze@samba.org)

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

/*
 * Code for Changing and setting a password
 */


static int net_password_change(struct net_context *ctx, int argc, const char **argv)
{
	NTSTATUS status;
	struct libnet_context *libnetctx;
	union libnet_ChangePassword r;
	char *password_prompt = NULL;
	const char *new_password;

	if (argc > 0 && argv[0]) {
		new_password = argv[0];
	} else {
		password_prompt = talloc_asprintf(ctx->mem_ctx, "Enter new password for account [%s\\%s]:", 
							ctx->user.domain_name, ctx->user.account_name);
		new_password = getpass(password_prompt);
	}

	libnetctx = libnet_context_init();
	if (!libnetctx) {
		return -1;	
	}

	/* prepare password change */
	r.generic.level			= LIBNET_CHANGE_PASSWORD_GENERIC;
	r.generic.in.account_name	= ctx->user.account_name;
	r.generic.in.domain_name	= ctx->user.domain_name;
	r.generic.in.oldpassword	= ctx->user.password;
	r.generic.in.newpassword	= new_password;

	/* do password change */
	status = libnet_ChangePassword(libnetctx, ctx->mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("net_password_change: %s\n",r.generic.out.error_string));
		return -1;
	}

	libnet_context_destroy(&libnetctx);

	return 0;
}

static int net_password_change_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net_password_change_usage: TODO\n");
	return 0;	
}

static int net_password_change_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net_password_change_help: TODO\n");
	return 0;	
}

static const struct net_functable const net_password_functable[] = {
	{"change", net_password_change, net_password_change_usage,  net_password_change_help},
	{NULL, NULL}
};

int net_password(struct net_context *ctx, int argc, const char **argv) 
{
	return net_run_function(ctx, argc, argv, net_password_functable, net_password_usage);
}

int net_password_usage(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net_password_usage: TODO\n");
	return 0;	
}

int net_password_help(struct net_context *ctx, int argc, const char **argv)
{
	d_printf("net_password_help: TODO\n");
	return 0;	
}
