
################################################
# Start MODULE socket_ipv4
[MODULE::socket_ipv4]
SUBSYSTEM = LIBSAMBA-SOCKET
OUTPUT_TYPE = INTEGRATED
OBJ_FILES = \
		socket_ipv4.o
PUBLIC_DEPENDENCIES = EXT_SOCKET EXT_NSL
PRIVATE_DEPENDENCIES = LIBSAMBA-ERRORS 
# End MODULE socket_ipv4
################################################

################################################
# Start MODULE socket_ipv6
[MODULE::socket_ipv6]
SUBSYSTEM = LIBSAMBA-SOCKET
OUTPUT_TYPE = INTEGRATED
OBJ_FILES = \
		socket_ipv6.o
PUBLIC_DEPENDENCIES = EXT_SOCKET EXT_NSL
# End MODULE socket_ipv6
################################################

################################################
# Start MODULE socket_unix
[MODULE::socket_unix]
SUBSYSTEM = LIBSAMBA-SOCKET
OUTPUT_TYPE = INTEGRATED
OBJ_FILES = \
		socket_unix.o
PUBLIC_DEPENDENCIES = EXT_SOCKET EXT_NSL
# End MODULE socket_unix
################################################

################################################
# Start SUBSYSTEM SOCKET
[SUBSYSTEM::LIBSAMBA-SOCKET]
OBJ_FILES = \
		socket.o \
		access.o \
		connect_multi.o \
		connect.o
LDFLAGS = $(SUBSYSTEM_LIBCLI_RESOLVE_OUTPUT) $(SUBSYSTEM_LIBCLI_NBT_OUTPUT) $(SUBSYSTEM_NDR_NBT_OUTPUT) $(LIBRARY_NDR_SVCCTL_OUTPUT)
PUBLIC_DEPENDENCIES = LIBTALLOC
PRIVATE_DEPENDENCIES = SOCKET_WRAPPER LIBCLI_COMPOSITE 
#LIBCLI_RESOLVE
# End SUBSYSTEM SOCKET
################################################
