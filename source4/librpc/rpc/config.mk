################################################
# Start SUBSYSTEM RPC_RAW
[SUBSYSTEM::RPC_RAW]
OBJ_FILES = \
		dcerpc.o \
		dcerpc_auth.o \
		dcerpc_schannel.o \
		dcerpc_util.o \
		dcerpc_error.o \
		dcerpc_smb.o \
		dcerpc_smb2.o \
		dcerpc_sock.o \
		dcerpc_connect.o
REQUIRED_SUBSYSTEMS = SOCKET
# End SUBSYSTEM RPC_RAW
################################################

