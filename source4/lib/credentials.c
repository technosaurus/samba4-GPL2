/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Tim Potter 2001
   
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
#include "system/filesys.h"

const char *cli_credentials_get_username(struct cli_credentials *cred)
{
	if (cred == NULL) {
		return NULL;
	}

	if (cred->username_obtained == CRED_CALLBACK) {
		cred->username = cred->username_cb(cred);
		cred->username_obtained = CRED_SPECIFIED;
	}

	return cred->username;
}

BOOL cli_credentials_set_username(struct cli_credentials *cred, const char *val, enum credentials_obtained obtained)
{
	if (obtained >= cred->username_obtained) {
		cred->username = talloc_strdup(cred, val);
		cred->username_obtained = obtained;
		return True;
	}

	return False;
}

const char *cli_credentials_get_password(struct cli_credentials *cred)
{
	if (cred == NULL) {
		return NULL;
	}
	
	if (cred->password_obtained == CRED_CALLBACK) {
		cred->password = cred->password_cb(cred);
		cred->password_obtained = CRED_SPECIFIED;
	}

	return cred->password;
}

BOOL cli_credentials_set_password(struct cli_credentials *cred, const char *val, enum credentials_obtained obtained)
{
	if (obtained >= cred->password_obtained) {
		cred->password = talloc_strdup(cred, val);
		cred->password_obtained = obtained;
		return True;
	}

	return False;
}

const char *cli_credentials_get_domain(struct cli_credentials *cred)
{
	if (cred == NULL) {
		return NULL;
	}

	if (cred->domain_obtained == CRED_CALLBACK) {
		cred->domain = cred->domain_cb(cred);
		cred->domain_obtained = CRED_SPECIFIED;
	}

	return cred->domain;
}


BOOL cli_credentials_set_domain(struct cli_credentials *cred, const char *val, enum credentials_obtained obtained)
{
	if (obtained >= cred->domain_obtained) {
		cred->domain = talloc_strdup(cred, val);
		cred->domain_obtained = obtained;
		return True;
	}

	return False;
}

const char *cli_credentials_get_realm(struct cli_credentials *cred)
{	
	if (cred == NULL) {
		return NULL;
	}

	if (cred->realm_obtained == CRED_CALLBACK) {
		cred->realm = cred->realm_cb(cred);
		cred->realm_obtained = CRED_SPECIFIED;
	}

	return cred->realm;
}

BOOL cli_credentials_set_realm(struct cli_credentials *cred, const char *val, enum credentials_obtained obtained)
{
	if (obtained >= cred->realm_obtained) {
		cred->realm = talloc_strdup(cred, val);
		cred->realm_obtained = obtained;
		return True;
	}

	return False;
}

const char *cli_credentials_get_workstation(struct cli_credentials *cred)
{
	if (cred == NULL) {
		return NULL;
	}

	if (cred->workstation_obtained == CRED_CALLBACK) {
		cred->workstation = cred->workstation_cb(cred);
		cred->workstation_obtained = CRED_SPECIFIED;
	}

	return cred->workstation;
}

BOOL cli_credentials_set_workstation(struct cli_credentials *cred, const char *val, enum credentials_obtained obtained)
{
	if (obtained >= cred->workstation_obtained) {
		cred->workstation = talloc_strdup(cred, val);
		cred->workstation_obtained = obtained;
		return True;
	}

	return False;
}

BOOL cli_credentials_parse_password_fd(struct cli_credentials *credentials, int fd, enum credentials_obtained obtained)
{
	char *p;
	char pass[128];

	for(p = pass, *p = '\0'; /* ensure that pass is null-terminated */
		p && p - pass < sizeof(pass);) {
		switch (read(fd, p, 1)) {
		case 1:
			if (*p != '\n' && *p != '\0') {
				*++p = '\0'; /* advance p, and null-terminate pass */
				break;
			}
		case 0:
			if (p - pass) {
				*p = '\0'; /* null-terminate it, just in case... */
				p = NULL; /* then force the loop condition to become false */
				break;
			} else {
				fprintf(stderr, "Error reading password from file descriptor %d: %s\n", fd, "empty password\n");
				return False;
			}

		default:
			fprintf(stderr, "Error reading password from file descriptor %d: %s\n",
					fd, strerror(errno));
			return False;
		}
	}

	cli_credentials_set_password(credentials, pass, obtained);
	return True;
}

BOOL cli_credentials_parse_password_file(struct cli_credentials *credentials, const char *file, enum credentials_obtained obtained)
{
	int fd = open(file, O_RDONLY, 0);
	BOOL ret;

	if (fd < 0) {
		fprintf(stderr, "Error opening PASSWD_FILE %s: %s\n",
				file, strerror(errno));
		return False;
	}

	ret = cli_credentials_parse_password_fd(credentials, fd, obtained);

	close(fd);
	
	return ret;
}

BOOL cli_credentials_parse_file(struct cli_credentials *cred, const char *file, enum credentials_obtained obtained) 
{
	XFILE *auth;
	char buf[128];
	uint16_t len = 0;
	char *ptr, *val, *param;

	if ((auth=x_fopen(file, O_RDONLY, 0)) == NULL)
	{
		/* fail if we can't open the credentials file */
		d_printf("ERROR: Unable to open credentials file!\n");
		return False;
	}

	while (!x_feof(auth))
	{
		/* get a line from the file */
		if (!x_fgets(buf, sizeof(buf), auth))
			continue;
		len = strlen(buf);

		if ((len) && (buf[len-1]=='\n'))
		{
			buf[len-1] = '\0';
			len--;
		}
		if (len == 0)
			continue;

		/* break up the line into parameter & value.
		 * will need to eat a little whitespace possibly */
		param = buf;
		if (!(ptr = strchr_m (buf, '=')))
			continue;

		val = ptr+1;
		*ptr = '\0';

		/* eat leading white space */
		while ((*val!='\0') && ((*val==' ') || (*val=='\t')))
			val++;

		if (strwicmp("password", param) == 0) {
			cli_credentials_set_password(cred, val, obtained);
		} else if (strwicmp("username", param) == 0) {
			cli_credentials_set_username(cred, val, obtained);
		} else if (strwicmp("domain", param) == 0) {
			cli_credentials_set_domain(cred, val, obtained);
		} else if (strwicmp("realm", param) == 0) {
			cli_credentials_set_realm(cred, val, obtained);
		}
		memset(buf, 0, sizeof(buf));
	}

	x_fclose(auth);
	return True;
}


void cli_credentials_parse_string(struct cli_credentials *credentials, const char *data, enum credentials_obtained obtained)
{
	char *uname, *p;

	uname = talloc_strdup(credentials, data); 
	cli_credentials_set_username(credentials, uname, obtained);

	if ((p = strchr_m(uname,'\\')) || (p = strchr_m(uname, '/'))) {
		*p = 0;
		cli_credentials_set_domain(credentials, uname, obtained);
		credentials->username = uname = p+1;
	}

	if ((p = strchr_m(uname,'@'))) {
		*p = 0;
		cli_credentials_set_realm(credentials, p+1, obtained);
	}

	if ((p = strchr_m(uname,'%'))) {
		*p = 0;
		cli_credentials_set_password(credentials, p+1, obtained);
	}
}

void cli_credentials_guess(struct cli_credentials *cred)
{
	char *p;

	cli_credentials_set_domain(cred, lp_workgroup(), CRED_GUESSED);
	cli_credentials_set_workstation(cred, lp_netbios_name(), CRED_GUESSED);
	cli_credentials_set_realm(cred, lp_realm(), CRED_GUESSED);

	if (getenv("LOGNAME")) {
		cli_credentials_set_username(cred, getenv("LOGNAME"), CRED_GUESSED);
	}

	if (getenv("USER")) {
		cli_credentials_parse_string(cred, getenv("USER"), CRED_GUESSED);
		if ((p = strchr_m(getenv("USER"),'%'))) {
			memset(p,0,strlen(cred->password));
		}
	}

	if (getenv("DOMAIN")) {
		cli_credentials_set_domain(cred, getenv("DOMAIN"), CRED_GUESSED);
	}

	if (getenv("PASSWD")) {
		cli_credentials_set_password(cred, getenv("PASSWD"), CRED_GUESSED);
	}

	if (getenv("PASSWD_FD")) {
		cli_credentials_parse_password_fd(cred, atoi(getenv("PASSWD_FD")), CRED_GUESSED);
	}
	
	if (getenv("PASSWD_FILE")) {
		cli_credentials_parse_password_file(cred, getenv("PASSWD_FILE"), CRED_GUESSED);
	}
}

BOOL cli_credentials_is_anonymous(struct cli_credentials *credentials)
{
	const char *username = cli_credentials_get_username(credentials);

	if (!username || !username[0]) 
		return True;

	return False;
}
