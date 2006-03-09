#######################
# Start LIBRARY EJSRPC
[SUBSYSTEM::EJSRPC]
OBJ_FILES = \
		ejsrpc.o
NOPROTO = YES
# End SUBSYSTEM EJSRPC
#######################

[MODULE::smbcalls_config]
OBJ_FILES = smbcalls_config.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_config

[MODULE::smbcalls_ldb]
OBJ_FILES = smbcalls_ldb.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_ldb

[MODULE::smbcalls_nbt]
OBJ_FILES = smbcalls_nbt.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_nbt

[MODULE::smbcalls_samba3]
OBJ_FILES = smbcalls_samba3.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_samba3
REQUIRED_SUBSYSTEMS = LIBSAMBA3 


[MODULE::smbcalls_rand]
OBJ_FILES = smbcalls_rand.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_random

[MODULE::smbcalls_nss]
OBJ_FILES = smbcalls_nss.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_nss

[MODULE::smbcalls_data]
OBJ_FILES = smbcalls_data.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_datablob

[MODULE::smbcalls_auth]
OBJ_FILES = smbcalls_auth.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_auth
REQUIRED_SUBSYSTEMS = auth

[MODULE::smbcalls_string]
OBJ_FILES = smbcalls_string.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_string

[MODULE::smbcalls_sys]
OBJ_FILES = smbcalls_sys.o
SUBSYSTEM = smbcalls
INIT_FUNCTION = smb_setup_ejs_system

#######################
# Start LIBRARY smbcalls
[SUBSYSTEM::smbcalls]
PRIVATE_PROTO_HEADER = proto.h
OBJ_FILES = \
		smbcalls.o \
		smbcalls_cli.o \
		smbcalls_rpc.o \
		smbcalls_options.o \
		smbcalls_creds.o \
		smbcalls_param.o \
		ejsnet.o \
		mprutil.o
REQUIRED_SUBSYSTEMS = \
		EJS LIBBASIC \
		EJSRPC MESSAGING \
		LIBNET LIBSMB LIBPOPT \
		POPT_CREDENTIALS POPT_SAMBA \
		dcerpc \
		NDR_TABLE \
		RPC_EJS_SECURITY \
		RPC_EJS_LSA \
		RPC_EJS_ECHO \
		RPC_EJS_WINREG \
		RPC_EJS_DFS \
		RPC_EJS_MISC \
		RPC_EJS_EVENTLOG \
		RPC_EJS_SAMR \
		RPC_EJS_WKSSVC \
		RPC_EJS_SRVSVC \
		RPC_EJS_SVCCTL \
		RPC_EJS_INITSHUTDOWN \
		RPC_EJS_NETLOGON \
		RPC_EJS_DRSUAPI \
		RPC_EJS_IRPC
# End SUBSYSTEM smbcalls
#######################

#######################
# Start BINARY SMBSCRIPT
[BINARY::smbscript]
INSTALLDIR = BINDIR
OBJ_FILES = \
		smbscript.o
REQUIRED_SUBSYSTEMS = EJS LIBBASIC smbcalls CONFIG 
# End BINARY SMBSCRIPT
#######################
