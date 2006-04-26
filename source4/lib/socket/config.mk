
################################################
# Start MODULE socket_ipv4
[MODULE::socket_ipv4]
SUBSYSTEM = SOCKET
OUTPUT_TYPE = MERGEDOBJ
OBJ_FILES = \
		socket_ipv4.o
PUBLIC_DEPENDENCIES = EXT_LIB_SOCKET
# End MODULE socket_ipv4
################################################

################################################
# Start MODULE socket_ipv6
[MODULE::socket_ipv6]
SUBSYSTEM = SOCKET
OUTPUT_TYPE = MERGEDOBJ
OBJ_FILES = \
		socket_ipv6.o
PUBLIC_DEPENDENCIES = EXT_LIB_SOCKET
# End MODULE socket_ipv6
################################################

################################################
# Start MODULE socket_unix
[MODULE::socket_unix]
SUBSYSTEM = SOCKET
OUTPUT_TYPE = MERGEDOBJ
OBJ_FILES = \
		socket_unix.o
PUBLIC_DEPENDENCIES = EXT_LIB_SOCKET
# End MODULE socket_unix
################################################

################################################
# Start SUBSYSTEM SOCKET
[SUBSYSTEM::SOCKET]
OBJ_FILES = \
		socket.o \
		access.o \
		connect_multi.o \
		connect.o
PUBLIC_DEPENDENCIES = LIBTALLOC
PRIVATE_DEPENDENCIES = LIBCLI_RESOLVE SOCKET_WRAPPER 
# End SUBSYSTEM SOCKET
################################################
