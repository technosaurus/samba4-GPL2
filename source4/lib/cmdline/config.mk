[SUBSYSTEM::LIBCMDLINE_CREDENTIALS]
PRIVATE_PROTO_HEADER = credentials.h
OBJ_FILES = credentials.o
REQUIRED_SUBSYSTEMS = CREDENTIALS

[SUBSYSTEM::POPT_SAMBA]
OBJ_FILES = popt_common.o

[SUBSYSTEM::POPT_CREDENTIALS]
PRIVATE_PROTO_HEADER = popt_credentials.h
OBJ_FILES = popt_credentials.o
REQUIRED_SUBSYSTEMS = CREDENTIALS LIBCMDLINE_CREDENTIALS
