[LIBRARY::LIBSAMBA-CONFIG]
DESCRIPTION = Reading Samba configuration files
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = loadparm.o \
			params.o \
			generic.o \
			util.o \
			../lib/version.o
PUBLIC_DEPENDENCIES = LIBSAMBA-UTIL DYNCONFIG
PUBLIC_PROTO_HEADER = proto.h
PUBLIC_HEADERS = param.h

#################################
# Start SUBSYSTEM share
[LIBRARY::share]
VERSION = 0.0.1
SO_VERSION = 0
DESCRIPTION = Services Configuration Library
PUBLIC_HEADERS = share.h
PUBLIC_PROTO_HEADER = share_proto.h
OBJ_FILES = share.o
# End SUBSYSTEM share
#################################

################################################
# Start MODULE share_classic
[MODULE::share_classic]
SUBSYSTEM = share
INIT_FUNCTION = share_classic_init
OBJ_FILES = share_classic.o 
PUBLIC_DEPENDENCIES = LIBSAMBA-UTIL
# End MODULE share_classic
################################################

