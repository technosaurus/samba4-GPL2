/* 
   Unix SMB/CIFS implementation.

   Functions to create reasonable random numbers for crypto use.

   Copyright (C) Jeremy Allison 2001
   
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

static uint8_t hash[258];
static uint32_t counter;
static uint8_t *reseed_data;
static size_t reseed_data_size;

/**************************************************************** 
 Copy any user given reseed data.
*****************************************************************/

void set_rand_reseed_data(uint8_t *data, size_t len)
{
	SAFE_FREE(reseed_data);
	reseed_data_size = 0;

	reseed_data = (uint8_t *)memdup(data, len);
	if (reseed_data)
		reseed_data_size = len;
}

/**************************************************************** 
 Setup the seed.
*****************************************************************/

static void seed_random_stream(uint8_t *seedval, size_t seedlen)
{
	uint8_t j = 0;
	size_t ind;

	for (ind = 0; ind < 256; ind++)
		hash[ind] = (uint8_t)ind;

	for( ind = 0; ind < 256; ind++) {
		uint8_t tc;

		j += (hash[ind] + seedval[ind%seedlen]);

		tc = hash[ind];
		hash[ind] = hash[j];
		hash[j] = tc;
	}

	hash[256] = 0;
	hash[257] = 0;
}

/**************************************************************** 
 Get datasize bytes worth of random data.
*****************************************************************/

static void get_random_stream(uint8_t *data, size_t datasize)
{
	uint8_t index_i = hash[256];
	uint8_t index_j = hash[257];
	size_t ind;

	for( ind = 0; ind < datasize; ind++) {
		uint8_t tc;
		uint8_t t;

		index_i++;
		index_j += hash[index_i];

		tc = hash[index_i];
		hash[index_i] = hash[index_j];
		hash[index_j] = tc;

		t = hash[index_i] + hash[index_j];
		data[ind] = hash[t];
	}

	hash[256] = index_i;
	hash[257] = index_j;
}

/****************************************************************
 Get a 16 byte hash from the contents of a file.
 Note that the hash is not initialised.
*****************************************************************/

static void do_filehash(const char *fname, uint8_t *the_hash)
{
	uint8_t buf[1011]; /* deliberate weird size */
	uint8_t tmp_md4[16];
	int fd, n;

	fd = sys_open(fname,O_RDONLY,0);
	if (fd == -1)
		return;

	while ((n = read(fd, (char *)buf, sizeof(buf))) > 0) {
		mdfour(tmp_md4, buf, n);
		for (n=0;n<16;n++)
			the_hash[n] ^= tmp_md4[n];
	}
	close(fd);
}

/**************************************************************
 Try and get a good random number seed. Try a number of
 different factors. Firstly, try /dev/urandom - use if exists.

 We use /dev/urandom as a read of /dev/random can block if
 the entropy pool dries up. This leads clients to timeout
 or be very slow on connect.

 If we can't use /dev/urandom then seed the stream random generator
 above...
**************************************************************/

static int do_reseed(BOOL use_fd, int fd)
{
	uint8_t seed_inbuf[40];
	uint32_t v1, v2; struct timeval tval; pid_t mypid;

	if (use_fd) {
		if (fd != -1)
			return fd;

		fd = sys_open( "/dev/urandom", O_RDONLY,0);
		if(fd >= 0)
			return fd;
	}

	/* Add in some secret file contents */

	do_filehash("/etc/shadow", &seed_inbuf[0]);
	do_filehash(lp_smb_passwd_file(), &seed_inbuf[16]);

	/*
	 * Add the counter, time of day, and pid.
	 */

	GetTimeOfDay(&tval);
	mypid = getpid();
	v1 = (counter++) + mypid + tval.tv_sec;
	v2 = (counter++) * mypid + tval.tv_usec;

	SIVAL(seed_inbuf, 32, v1 ^ IVAL(seed_inbuf, 32));
	SIVAL(seed_inbuf, 36, v2 ^ IVAL(seed_inbuf, 36));

	/*
	 * Add any user-given reseed data.
	 */

	if (reseed_data) {
		size_t i;
		for (i = 0; i < sizeof(seed_inbuf); i++)
			seed_inbuf[i] ^= reseed_data[i % reseed_data_size];
	}

	seed_random_stream(seed_inbuf, sizeof(seed_inbuf));

	return -1;
}

/*******************************************************************
 Interface to the (hopefully) good crypto random number generator.
********************************************************************/

void generate_random_buffer( uint8_t *out, int len, BOOL do_reseed_now)
{
	static BOOL done_reseed = False;
	static int urand_fd = -1;
	uint8_t md4_buf[64];
	uint8_t tmp_buf[16];
	uint8_t *p;

	if(!done_reseed || do_reseed_now) {
		urand_fd = do_reseed(True, urand_fd);
		done_reseed = True;
	}

	if (urand_fd != -1 && len > 0) {

		if (read(urand_fd, out, len) == len)
			return; /* len bytes of random data read from urandom. */

		/* Read of urand error, drop back to non urand method. */
		close(urand_fd);
		urand_fd = -1;
		do_reseed(False, -1);
		done_reseed = True;
	}

	/*
	 * Generate random numbers in chunks of 64 bytes,
	 * then md4 them & copy to the output buffer.
	 * This way the raw state of the stream is never externally
	 * seen.
	 */

	p = out;
	while(len > 0) {
		int copy_len = len > 16 ? 16 : len;

		get_random_stream(md4_buf, sizeof(md4_buf));
		mdfour(tmp_buf, md4_buf, sizeof(md4_buf));
		memcpy(p, tmp_buf, copy_len);
		p += copy_len;
		len -= copy_len;
	}
}


/*
  very basic password quality checker
*/
BOOL check_password_quality(const char *s)
{
	int has_digit=0, has_capital=0, has_lower=0;
	while (*s) {
		if (isdigit(*s)) {
			has_digit++;
		} else if (isupper(*s)) {
			has_capital++;
		} else if (islower(*s)) {
			has_lower++;
		}
		s++;
	}

	return has_digit && has_lower && has_capital;
}

/*******************************************************************
 Use the random number generator to generate a random string.
********************************************************************/

static char c_list[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_-#.,";

char *generate_random_str(TALLOC_CTX *mem_ctx, size_t len)
{
	size_t i;

	char *retstr = talloc(mem_ctx, len + 1);

	if (!retstr) 
		return NULL;

again:
	generate_random_buffer(retstr, len, False);
	for (i = 0; i < len; i++)
		retstr[i] = c_list[retstr[i] % (sizeof(c_list)-1) ];

	retstr[i] = '\0';

	/* we need to make sure the random string passes basic quality tests
	   or it might be rejected by windows as a password */
	if (len >= 7 && !check_password_quality(retstr)) {
		goto again;
	}

	return retstr;
}
