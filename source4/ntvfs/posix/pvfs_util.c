/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2004

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
  utility functions for posix backend
*/

#include "includes.h"
#include "vfs_posix.h"

/*
  return True if a string contains one of the CIFS wildcard characters
*/
BOOL pvfs_has_wildcard(const char *str)
{
	if (strpbrk(str, "*?<>\"")) {
		return True;
	}
	return False;
}

/*
  map a unix errno to a NTSTATUS
*/
NTSTATUS pvfs_map_errno(struct pvfs_state *pvfs, int unix_errno)
{
	return map_nt_error_from_unix(unix_errno);
}


/*
  check if a filename has an attribute matching the given attribute search value
  this is used by calls like unlink and search which take an attribute
  and only include special files if they match the given attribute
*/
NTSTATUS pvfs_match_attrib(struct pvfs_state *pvfs, struct pvfs_filename *name, 
			   uint32_t attrib, uint32_t must_attrib)
{
	if ((name->dos.attrib & ~attrib) & FILE_ATTRIBUTE_DIRECTORY) {
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}
	if ((name->dos.attrib & ~attrib) & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) {
		return NT_STATUS_NO_SUCH_FILE;
	}
	if (must_attrib & ~name->dos.attrib) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}
	return NT_STATUS_OK;
}


/*
  normalise a file attribute
*/
uint32_t pvfs_attrib_normalise(uint32_t attrib)
{
	if (attrib != FILE_ATTRIBUTE_NORMAL) {
		attrib &= ~FILE_ATTRIBUTE_NORMAL;
	}
	return attrib;
}


/*
  copy a file. Caller is supposed to have already ensured that the
  operation is allowed. The destination file must not exist.
*/
NTSTATUS pvfs_copy_file(struct pvfs_state *pvfs,
			struct pvfs_filename *name1, 
			struct pvfs_filename *name2)
{
	int fd1, fd2;
	mode_t mode;
	NTSTATUS status;
	size_t buf_size = 0x10000;
	char *buf = talloc(name2, buf_size);

	if (buf == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	fd1 = open(name1->full_name, O_RDONLY);
	if (fd1 == -1) {
		talloc_free(buf);
		return pvfs_map_errno(pvfs, errno);
	}

	fd2 = open(name2->full_name, O_CREAT|O_EXCL|O_WRONLY, 0);
	if (fd2 == -1) {
		close(fd1);
		talloc_free(buf);
		return pvfs_map_errno(pvfs, errno);
	}

	while (1) {
		ssize_t ret2, ret = read(fd1, buf, buf_size);
		if (ret == -1 && 
		    (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		if (ret <= 0) break;

		ret2 = write(fd2, buf, ret);
		if (ret2 == -1 &&
		    (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		
		if (ret2 != ret) {
			close(fd1);
			close(fd2);
			talloc_free(buf);
			unlink(name2->full_name);
			if (ret2 == -1) {
				return pvfs_map_errno(pvfs, errno);
			}
			return NT_STATUS_DISK_FULL;
		}
	}

	close(fd1);

	mode = pvfs_fileperms(pvfs, name1->dos.attrib);
	if (fchmod(fd2, mode) == -1) {
		status = pvfs_map_errno(pvfs, errno);
		close(fd2);
		unlink(name2->full_name);
		return status;
	}
	
	name2->dos = name1->dos;

	status = pvfs_dosattrib_save(pvfs, name2, fd2);
	if (!NT_STATUS_IS_OK(status)) {
		close(fd2);
		unlink(name2->full_name);
		return status;
	}

	close(fd2);

	return NT_STATUS_OK;
}


/* 
   hash a string of the specified length. The string does not need to be
   null terminated 

   hash alghorithm changed to FNV1 by idra@samba.org (Simo Sorce).
   see http://www.isthe.com/chongo/tech/comp/fnv/index.html for a
   discussion on Fowler / Noll / Vo (FNV) Hash by one of it's authors
*/
uint32_t pvfs_name_hash(const char *key, size_t length)
{
	const uint32_t fnv1_prime = 0x01000193;
	const uint32_t fnv1_init = 0xa6b93095;
	uint32_t value = fnv1_init;

	while (*key && length--) {
		size_t c_size;
		codepoint_t c = next_codepoint(key, &c_size);
		c = toupper_w(c);
                value *= fnv1_prime;
                value ^= (uint32_t)c;
		key += c_size;
        }

	return value;
}


/*
  file allocation size rounding. This is required to pass ifstest
*/
uint64_t pvfs_round_alloc_size(struct pvfs_state *pvfs, uint64_t size)
{
	const uint64_t round_value = 511;
	if (size == 0) return 0;
	return (size + round_value) & ~round_value;
}
