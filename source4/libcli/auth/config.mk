#################################
# Start SUBSYSTEM LIBCLI_AUTH
[SUBSYSTEM::LIBCLI_AUTH]
OBJ_FILES = credentials.o \
		session.o \
		smbencrypt.o 
REQUIRED_SUBSYSTEMS = \
		AUTH SCHANNELDB
# End SUBSYSTEM LIBCLI_AUTH
#################################
