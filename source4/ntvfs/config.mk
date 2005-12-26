# NTVFS Server subsystem
include posix/config.mk
include unixuid/config.mk

################################################
# Start MODULE ntvfs_cifs
[MODULE::ntvfs_cifs]
INIT_FUNCTION = ntvfs_cifs_init 
SUBSYSTEM = NTVFS
OBJ_FILES = \
		cifs/vfs_cifs.o
REQUIRED_SUBSYSTEMS = \
		LIBCLI
# End MODULE ntvfs_cifs
################################################

################################################
# Start MODULE ntvfs_simple
[MODULE::ntvfs_simple]
INIT_FUNCTION = ntvfs_simple_init 
SUBSYSTEM = NTVFS
OBJ_FILES = \
		simple/vfs_simple.o \
		simple/svfs_util.o
# End MODULE ntvfs_cifs
################################################

################################################
# Start MODULE ntvfs_print
[MODULE::ntvfs_print]
INIT_FUNCTION = ntvfs_print_init 
SUBSYSTEM = NTVFS
OBJ_FILES = \
		print/vfs_print.o
# End MODULE ntvfs_print
################################################

################################################
# Start MODULE ntvfs_ipc
[MODULE::ntvfs_ipc]
SUBSYSTEM = NTVFS
INIT_FUNCTION = ntvfs_ipc_init 
OBJ_FILES = \
		ipc/vfs_ipc.o \
		ipc/ipc_rap.o \
		ipc/rap_server.o
# End MODULE ntvfs_ipc
################################################



################################################
# Start MODULE ntvfs_nbench
[MODULE::ntvfs_nbench]
SUBSYSTEM = NTVFS
INIT_FUNCTION = ntvfs_nbench_init 
OBJ_FILES = \
		nbench/vfs_nbench.o
# End MODULE ntvfs_nbench
################################################

################################################
# Start SUBSYSTEM ntvfs_common
[SUBSYSTEM::ntvfs_common]
OBJ_FILES = \
		common/brlock.o \
		common/opendb.o \
		common/sidmap.o
# End SUBSYSTEM ntvfs_common
################################################


################################################
# Start SUBSYSTEM NTVFS
[LIBRARY::NTVFS]
PUBLIC_HEADERS = ntvfs.h
MAJOR_VERSION = 0
MINOR_VERSION = 0
DESCRIPTION = Virtual File System with NTFS semantics
RELEASE_VERSION = 1
OBJ_FILES = \
		ntvfs_base.o \
		ntvfs_generic.o \
		ntvfs_interface.o \
		ntvfs_util.o
#
# End SUBSYSTEM NTVFS
################################################
