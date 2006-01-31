/*
   CIFSDD - dd for SMB.
   IO routines, generic and specific.

   Copyright (C) James Peach 2005-2006

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
#include "libcli/raw/libcliraw.h"
#include "libcli/libcli.h"
#include "lib/cmdline/popt_common.h"

#include "cifsdd.h"

/* ------------------------------------------------------------------------- */
/* UNIX file descriptor IO.						     */
/* ------------------------------------------------------------------------- */

struct fd_handle
{
	struct dd_iohandle	h;
	int			fd;
};

#define IO_HANDLE_TO_FD(h) (((struct fd_handle *)(h))->fd)

static BOOL fd_seek_func(void * handle, uint64_t offset)
{
	ssize_t ret;

	ret = lseek(IO_HANDLE_TO_FD(handle), offset, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "%s: seek failed: %s\n",
				PROGNAME, strerror(errno));
		return(False);
	}

	return(True);
}

static BOOL fd_read_func(void * handle, uint8_t * buf, uint64_t wanted, uint64_t * actual)
{
	ssize_t ret;

	ret = read(IO_HANDLE_TO_FD(handle), buf, wanted);
	if (ret < 0) {
		fprintf(stderr, "%s: %llu byte read failed: %s\n",
				PROGNAME, (unsigned long long)wanted, strerror(errno));
		return(False);
	}

	*actual = (uint64_t)ret;
	return(True);
}

static BOOL
fd_write_func(void * handle, uint8_t * buf, uint64_t wanted, uint64_t * actual)
{
	ssize_t ret;

	ret = write(IO_HANDLE_TO_FD(handle), buf, wanted);
	if (ret < 0) {
		fprintf(stderr, "%s: %llu byte write failed: %s\n",
				PROGNAME, (unsigned long long)wanted, strerror(errno));
		return(False);
	}

	*actual = (uint64_t)ret;
	return(True);
}

static struct dd_iohandle *
open_fd_handle(const char * path, uint64_t iosz, int options)
{
	struct fd_handle * fdh;
	int oflags = 0;

	DEBUG(4, ("opening fd stream for %s\n", path));
	if ((fdh = talloc_zero(NULL, struct fd_handle)) == NULL) {
		return(NULL);
	}

	fdh->h.io_read = fd_read_func;
	fdh->h.io_write = fd_write_func;
	fdh->h.io_seek = fd_seek_func;

	if (options & DD_DIRECT_IO) {
#ifdef HAVE_OPEN_O_DIRECT
		oflags |= O_DIRECT;
#else
		DEBUG(1, ("no support for direct IO on this platform\n"));
#endif
	}

	if (options & DD_SYNC_IO)
		oflags |= O_SYNC;

	oflags |= (options & DD_WRITE) ?  (O_WRONLY | O_CREAT) : (O_RDONLY);

	fdh->fd = open(path, oflags, 0644);
	if (fdh->fd < 0) {
		fprintf(stderr, "%s: %s: %s\n",
			PROGNAME, path, strerror(errno));
		talloc_free(fdh);
		return(NULL);
	}

	if (options & DD_OPLOCK) {
		DEBUG(2, ("FIXME: take local oplock on %s\n", path));
	}

	SMB_ASSERT((void *)fdh == (void *)&fdh->h);
	return(&fdh->h);
}

/* ------------------------------------------------------------------------- */
/* CIFS client IO.							     */
/* ------------------------------------------------------------------------- */

struct smb_handle
{
	struct dd_iohandle	h;
	struct smbcli_state *	cli;
	int			fnum;
	uint64_t		offset;
};

#define IO_HANDLE_TO_SMB(h) ((struct smb_handle *)(h))

BOOL smb_seek_func(void * handle, uint64_t offset)
{
	IO_HANDLE_TO_SMB(handle)->offset = offset;
	return(True);
}

BOOL smb_read_func(void * handle, uint8_t * buf,
				uint64_t wanted, uint64_t * actual)
{
	NTSTATUS		ret;
	union smb_read		r;
	struct smb_handle *	smbh;

	ZERO_STRUCT(r);
	smbh = IO_HANDLE_TO_SMB(handle);

	r.generic.level = RAW_READ_READX;
	r.readx.in.fnum = smbh->fnum;
	r.readx.in.offset = smbh->offset;
	r.readx.in.mincnt = wanted;
	r.readx.in.maxcnt = wanted;
	r.readx.out.data = buf;

	/* FIXME: Should I really set readx.in.remaining? That just seems
	 * redundant.
	 */
	ret = smb_raw_read(smbh->cli->tree, &r);
	if (!NT_STATUS_IS_OK(ret)) {
		fprintf(stderr, "%s: %llu byte read failed: %s\n",
				PROGNAME, (unsigned long long)wanted, nt_errstr(ret));
		return(False);
	}

	/* Trap integer wrap. */
	SMB_ASSERT((smbh->offset + r.readx.out.nread) >= smbh->offset);

	*actual = r.readx.out.nread;
	smbh->offset += r.readx.out.nread;
	return(True);
}

BOOL smb_write_func(void * handle, uint8_t * buf,
				uint64_t wanted, uint64_t * actual)
{
	NTSTATUS		ret;
	union smb_write		w;
	struct smb_handle *	smbh;

	ZERO_STRUCT(w);
	smbh = IO_HANDLE_TO_SMB(handle);

	w.generic.level = RAW_WRITE_WRITEX;
	w.writex.in.fnum = smbh->fnum;
	w.writex.in.offset = smbh->offset;
	w.writex.in.count = wanted;
	w.writex.in.data = buf;

	ret = smb_raw_write(smbh->cli->tree, &w);
	if (!NT_STATUS_IS_OK(ret)) {
		fprintf(stderr, "%s: %llu byte write failed: %s\n",
				PROGNAME, (unsigned long long)wanted, nt_errstr(ret));
		return(False);
	}

	*actual = w.writex.out.nwritten;
	smbh->offset += w.writex.out.nwritten;
	return(True);
}

static struct smbcli_state * init_smb_session(const char * host, const char * share)
{
	NTSTATUS		ret;
	struct smbcli_state *	cli = NULL;

	/* When we support SMB URLs, we can get different user credentials for
	 * each connection, but for now, we just use the same one for both.
	 */
	ret = smbcli_full_connection(NULL, &cli, host, share,
			 NULL /* devtype */, cmdline_credentials, NULL /* events */);

	if (!NT_STATUS_IS_OK(ret)) {
		fprintf(stderr, "%s: connecting to //%s/%s: %s\n",
			PROGNAME, host, share, nt_errstr(ret));
		return(NULL);
	}

	return(cli);
}

static int open_smb_file(struct smbcli_state * cli, const char * path, int options)
{
	NTSTATUS	ret;
	union smb_open	o;

	ZERO_STRUCT(o);

	o.ntcreatex.level = RAW_OPEN_NTCREATEX;
	o.ntcreatex.in.fname = path;

	/* TODO: It's not clear whether to use these flags or to use the
	 * similarly named NTCREATEX flags in the create_options field.
	 */
	if (options & DD_DIRECT_IO)
		o.ntcreatex.in.flags |= FILE_FLAG_NO_BUFFERING;

	if (options & DD_SYNC_IO)
		o.ntcreatex.in.flags |= FILE_FLAG_WRITE_THROUGH;

	o.ntcreatex.in.access_mask |=
		(options & DD_WRITE) ? SEC_FILE_WRITE_DATA
					: SEC_FILE_READ_DATA;

	/* Try to create the file only if we will be writing to it. */
	o.ntcreatex.in.open_disposition =
		(options & DD_WRITE) ? NTCREATEX_DISP_OPEN_IF
					: NTCREATEX_DISP_OPEN;

	o.ntcreatex.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;

	if (options & DD_OPLOCK) {
		o.ntcreatex.in.flags |= NTCREATEX_FLAGS_REQUEST_OPLOCK;
	}

	ret = smb_raw_open(cli->tree, NULL, &o);
	if (!NT_STATUS_IS_OK(ret)) {
		fprintf(stderr, "%s: opening %s: %s\n",
			PROGNAME, path, nt_errstr(ret));
		return(-1);
	}

	return(o.ntcreatex.out.fnum);
}

static struct dd_iohandle * open_smb_handle(const char * host, const char * share,
					const char * path, uint64_t iosz, int options)
{
	struct smb_handle * smbh;

	DEBUG(4, ("opening SMB stream to //%s/%s for %s\n",
		host, share, path));

	if ((smbh = talloc_zero(NULL, struct smb_handle)) == NULL) {
		return(NULL);
	}

	smbh->h.io_read = smb_read_func;
	smbh->h.io_write = smb_write_func;
	smbh->h.io_seek = smb_seek_func;

	if ((smbh->cli = init_smb_session(host, share)) == NULL) {
		return(NULL);
	}

	DEBUG(4, ("connected to //%s/%s with xmit size of %u bytes\n",
		host, share, smbh->cli->transport->negotiate.max_xmit));

	smbh->fnum = open_smb_file(smbh->cli, path, options);
	return(&smbh->h);
}

/* ------------------------------------------------------------------------- */
/* Abstract IO interface.						     */
/* ------------------------------------------------------------------------- */

struct dd_iohandle * dd_open_path(const char * path, uint64_t iosz, int options)
{
	if (file_exist(path)) {
		return(open_fd_handle(path, iosz, options));
	} else {
		char * host;
		char * share;

		if (smbcli_parse_unc(path, NULL, &host, &share)) {
			const char * remain;
			remain = strstr(path, share) + strlen(share);

			/* Skip over leading directory separators. */
			while (*remain == '/' || *remain == '\\') { remain++; }

			return(open_smb_handle(host, share, remain,
						iosz, options));
		}

		return(open_fd_handle(path, iosz, options));
	}
}

/* Fill the buffer till it has at least needsz bytes. Use read operations of
 * blocksz bytes. Return the number of bytes read and fill bufsz with the new
 * buffer size.
 *
 * NOTE: The IO buffer is guaranteed to be big enough to fit needsz + blocksz
 * bytes into it.
 */
BOOL dd_fill_block(struct dd_iohandle * h, uint8_t * buf,
		uint64_t * bufsz, uint64_t needsz, uint64_t blocksz)
{
	uint64_t readsz;

	SMB_ASSERT(blocksz > 0);
	SMB_ASSERT(needsz > 0);

	while (*bufsz < needsz) {

		if (!h->io_read(h, buf + (*bufsz), blocksz, &readsz)) {
			return(False);
		}

		if (readsz == 0) {
			h->io_flags |= DD_END_OF_FILE;
			break;
		}

		DEBUG(6, ("added %llu bytes to IO buffer (need %llu bytes)\n",
			(unsigned long long)readsz, (unsigned long long)needsz));

		*bufsz += readsz;
		dd_stats.in.bytes += readsz;

		if (readsz == blocksz) {
			dd_stats.in.fblocks++;
		} else {
			DEBUG(3, ("partial read of %llu bytes (expected %llu)\n",
				(unsigned long long)readsz, (unsigned long long)blocksz));
			dd_stats.in.pblocks++;
		}
	}

	return(True);
}

/* Flush a buffer that contains bufsz bytes. Use writes of blocksz to do it,
 * and shift any remaining bytes back to the head of the buffer when there are
 * no more blocksz sized IOs left.
 */
BOOL dd_flush_block(struct dd_iohandle * h, uint8_t * buf,
		uint64_t * bufsz, uint64_t blocksz)
{
	uint64_t writesz;
	uint64_t totalsz = 0;

	SMB_ASSERT(blocksz > 0);

	/* We have explicitly been asked to write a partial block. */
	if ((*bufsz) < blocksz) {

		if (!h->io_write(h, buf, *bufsz, &writesz)) {
			return(False);
		}

		if (writesz == 0) {
			fprintf(stderr, "%s: unexpectedly wrote 0 bytes\n",
					PROGNAME);
			return(False);
		}

		totalsz += writesz;
		dd_stats.out.bytes += writesz;
		dd_stats.out.pblocks++;
	}

	/* Write as many full blocks as there are in the buffer. */
	while (((*bufsz) - totalsz) >= blocksz) {

		if (!h->io_write(h, buf + totalsz, blocksz, &writesz)) {
			return(False);
		}

		if (writesz == 0) {
			fprintf(stderr, "%s: unexpectedly wrote 0 bytes\n",
					PROGNAME);
			return(False);
		}

		if (writesz == blocksz) {
			dd_stats.out.fblocks++;
		} else {
			dd_stats.out.pblocks++;
		}

		totalsz += writesz;
		dd_stats.out.bytes += writesz;

		DEBUG(6, ("flushed %llu bytes from IO buffer of %llu bytes (%llu remain)\n",
			(unsigned long long)blocksz, (unsigned long long)blocksz,
			(unsigned long long)(blocksz - totalsz)));
	}

	SMB_ASSERT(totalsz > 0);

	/* We have flushed as much of the IO buffer as we can while
	 * still doing blocksz-sized operations. Shift any remaining data
	 * to the front of the IO buffer.
	 */
	if ((*bufsz) > totalsz) {
		uint64_t remain = (*bufsz) - totalsz;

		DEBUG(3, ("shifting %llu remainder bytes to IO buffer head\n",
			(unsigned long long)remain));

		memmove(buf, buf + totalsz, remain);
		(*bufsz) = remain;
	} else if ((*bufsz) == totalsz) {
		(*bufsz) = 0;
	} else {
		/* Else buffer contains bufsz bytes that we will append
		 * to next time round.
		 */
		DEBUG(3, ("%llu unflushed bytes left in IO buffer\n",
			(unsigned long long)(*bufsz)));
	}

	return(True);
}

/* vim: set sw=8 sts=8 ts=8 tw=79 : */
