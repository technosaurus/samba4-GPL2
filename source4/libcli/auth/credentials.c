/* 
   Unix SMB/CIFS implementation.

   code to manipulate domain credentials

   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004
   
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
  initialise the credentials state for old-style 64 bit session keys

  this call is made after the netr_ServerReqChallenge call
*/
static void creds_init_64bit(struct creds_CredentialState *creds,
			     const struct netr_Credential *client_challenge,
			     const struct netr_Credential *server_challenge,
			     const uint8_t machine_password[16])
{
	uint32_t sum[2];
	uint8_t sum2[8];

	sum[0] = IVAL(client_challenge->data, 0) + IVAL(server_challenge->data, 0);
	sum[1] = IVAL(client_challenge->data, 4) + IVAL(server_challenge->data, 4);

	SIVAL(sum2,0,sum[0]);
	SIVAL(sum2,4,sum[1]);

	ZERO_STRUCT(creds->session_key);

	des_crypt128(creds->session_key, sum2, machine_password);

	des_crypt112(creds->client.data, client_challenge->data, creds->session_key, 1);
	des_crypt112(creds->server.data, server_challenge->data, creds->session_key, 1);

	creds->seed = creds->client;
}

/*
  initialise the credentials state for ADS-style 128 bit session keys

  this call is made after the netr_ServerReqChallenge call
*/
static void creds_init_128bit(struct creds_CredentialState *creds,
			      const struct netr_Credential *client_challenge,
			      const struct netr_Credential *server_challenge,
			      const uint8_t machine_password[16])
{
	unsigned char zero[4], tmp[16];
	HMACMD5Context ctx;
	struct MD5Context md5;

	ZERO_STRUCT(creds->session_key);

	memset(zero, 0, sizeof(zero));

	hmac_md5_init_rfc2104(machine_password, 16, &ctx);	
	MD5Init(&md5);
	MD5Update(&md5, zero, sizeof(zero));
	MD5Update(&md5, client_challenge->data, 8);
	MD5Update(&md5, server_challenge->data, 8);
	MD5Final(tmp, &md5);
	hmac_md5_update(tmp, 16, &ctx);
	hmac_md5_final(creds->session_key, &ctx);

	creds->client = *client_challenge;
	creds->server = *server_challenge;

	des_crypt112(creds->client.data, client_challenge->data, creds->session_key, 1);
	des_crypt112(creds->server.data, server_challenge->data, creds->session_key, 1);

	creds->seed = creds->client;
}


/*
  step the credentials to the next element in the chain, updating the
  current client and server credentials and the seed
*/
static void creds_step(struct creds_CredentialState *creds)
{
	struct netr_Credential time_cred;

	DEBUG(5,("\tseed        %08x:%08x\n", 
		 IVAL(creds->seed.data, 0), IVAL(creds->seed.data, 4)));

	SIVAL(time_cred.data, 0, IVAL(creds->seed.data, 0) + creds->sequence);
	SIVAL(time_cred.data, 4, IVAL(creds->seed.data, 4));

	DEBUG(5,("\tseed+time   %08x:%08x\n", IVAL(time_cred.data, 0), IVAL(time_cred.data, 4)));

	des_crypt112(creds->client.data, time_cred.data, creds->session_key, 1);

	DEBUG(5,("\tCLIENT      %08x:%08x\n", 
		 IVAL(creds->client.data, 0), IVAL(creds->client.data, 4)));

	SIVAL(time_cred.data, 0, IVAL(creds->seed.data, 0) + creds->sequence + 1);
	SIVAL(time_cred.data, 4, IVAL(creds->seed.data, 4));

	DEBUG(5,("\tseed+time+1 %08x:%08x\n", 
		 IVAL(time_cred.data, 0), IVAL(time_cred.data, 4)));

	des_crypt112(creds->server.data, time_cred.data, creds->session_key, 1);

	DEBUG(5,("\tSERVER      %08x:%08x\n", 
		 IVAL(creds->server.data, 0), IVAL(creds->server.data, 4)));

	creds->seed = time_cred;
}


/*
  DES encrypt a 16 byte password buffer using the session key
*/
void creds_des_encrypt(struct creds_CredentialState *creds, struct samr_Password *pass)
{
	struct samr_Password tmp;
	des_crypt112_16(tmp.hash, pass->hash, creds->session_key, 1);
	*pass = tmp;
}

/*
  DES decrypt a 16 byte password buffer using the session key
*/
void creds_des_decrypt(struct creds_CredentialState *creds, struct samr_Password *pass)
{
	struct samr_Password tmp;
	des_crypt112_16(tmp.hash, pass->hash, creds->session_key, 0);
	*pass = tmp;
}

/*
  ARCFOUR encrypt/decrypt a password buffer using the session key
*/
void creds_arcfour_crypt(struct creds_CredentialState *creds, char *data, size_t len)
{
	DATA_BLOB session_key = data_blob(creds->session_key, 16);

	arcfour_crypt_blob(data, len, &session_key);

	data_blob_free(&session_key);
}

/*****************************************************************
The above functions are common to the client and server interface
next comes the client specific functions
******************************************************************/

/*
  initialise the credentials chain and return the first client
  credentials
*/
void creds_client_init(struct creds_CredentialState *creds,
		       const struct netr_Credential *client_challenge,
		       const struct netr_Credential *server_challenge,
		       const uint8_t machine_password[16],
		       struct netr_Credential *initial_credential,
		       uint32_t negotiate_flags)
{
	creds->sequence = time(NULL);
	creds->negotiate_flags = negotiate_flags;

	dump_data_pw("Client chall", client_challenge->data, sizeof(client_challenge->data));
	dump_data_pw("Server chall", server_challenge->data, sizeof(server_challenge->data));
	dump_data_pw("Machine Pass", machine_password, 16);

	if (negotiate_flags & NETLOGON_NEG_128BIT) {
		creds_init_128bit(creds, client_challenge, server_challenge, machine_password);
	} else {
		creds_init_64bit(creds, client_challenge, server_challenge, machine_password);
	}

	dump_data_pw("Session key", creds->session_key, 16);
	dump_data_pw("Credential ", creds->client.data, 8);

	*initial_credential = creds->client;
}

/*
  step the credentials to the next element in the chain, updating the
  current client and server credentials and the seed

  produce the next authenticator in the sequence ready to send to 
  the server
*/
void creds_client_authenticator(struct creds_CredentialState *creds,
				struct netr_Authenticator *next)
{	
	creds->sequence += 2;
	creds_step(creds);

	next->cred = creds->client;
	next->timestamp = creds->sequence;
}

/*
  check that a credentials reply from a server is correct
*/
BOOL creds_client_check(struct creds_CredentialState *creds,
			const struct netr_Credential *received_credentials)
{
	if (!received_credentials || 
	    memcmp(received_credentials->data, creds->server.data, 8) != 0) {
		DEBUG(2,("credentials check failed\n"));
		return False;
	}
	return True;
}


/*****************************************************************
The above functions are common to the client and server interface
next comes the server specific functions
******************************************************************/

/*
  initialise the credentials chain and return the first server
  credentials
*/
void creds_server_init(struct creds_CredentialState *creds,
		       const struct netr_Credential *client_challenge,
		       const struct netr_Credential *server_challenge,
		       const uint8_t machine_password[16],
		       struct netr_Credential *initial_credential,
		       uint32_t negotiate_flags)
{
	if (negotiate_flags & NETLOGON_NEG_128BIT) {
		creds_init_128bit(creds, client_challenge, server_challenge, 
				  machine_password);
	} else {
		creds_init_64bit(creds, client_challenge, server_challenge, 
				 machine_password);
	}

	*initial_credential = creds->server;
}

/*
  check that a credentials reply from a server is correct
*/
BOOL creds_server_check(const struct creds_CredentialState *creds,
			const struct netr_Credential *received_credentials)
{
	if (memcmp(received_credentials->data, creds->client.data, 8) != 0) {
		DEBUG(2,("credentials check failed\n"));
		dump_data_pw("client creds", creds->client.data, 8);
		dump_data_pw("calc   creds", received_credentials->data, 8);
		return False;
	}
	return True;
}

BOOL creds_server_step_check(struct creds_CredentialState *creds,
			     struct netr_Authenticator *received_authenticator,
			     struct netr_Authenticator *return_authenticator) 
{
	/* Should we check that this is increasing? */
	creds->sequence = received_authenticator->timestamp;
	creds_step(creds);
	if (creds_server_check(creds, &received_authenticator->cred)) {
		return_authenticator->cred = creds->server;
		return_authenticator->timestamp = creds->sequence;
		return True;
	} else {
		ZERO_STRUCTP(return_authenticator);
		return False;
	}
}
