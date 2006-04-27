################################################
# Start SUBSYSTEM LIBNDR
[LIBRARY::LIBNDR]
VERSION = 0.0.1
SO_VERSION = 0
DESCRIPTION = Network Data Representation Core Library
PUBLIC_HEADERS = ndr/libndr.h
PUBLIC_PROTO_HEADER = ndr/libndr_proto.h
OBJ_FILES = \
		ndr/ndr.o \
		ndr/ndr_basic.o \
		ndr/ndr_string.o \
		ndr/ndr_misc.o
PUBLIC_DEPENDENCIES = LIBSAMBA-ERRORS LIBTALLOC LIBSAMBA-UTIL CHARSET
# End SUBSYSTEM LIBNDR
################################################

################################################
# Start SUBSYSTEM NDR_COMPRESSION
[LIBRARY::NDR_COMPRESSION]
VERSION = 0.0.1
SO_VERSION = 0
DESCRIPTION = NDR support for compressed subcontexts
PRIVATE_PROTO_HEADER = ndr/ndr_compression.h
OBJ_FILES = \
		ndr/ndr_compression.o
PUBLIC_DEPENDENCIES = LIBCOMPRESSION
# End SUBSYSTEM NDR_COMPRESSION
################################################

[SUBSYSTEM::NDR_SECURITY_HELPER]
PRIVATE_PROTO_HEADER = ndr/ndr_sec.h
OBJ_FILES = ndr/ndr_sec_helper.o ndr/ndr_sec.o

[LIBRARY::NDR_SECURITY]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_security.o
PUBLIC_HEADERS = gen_ndr/security.h
PUBLIC_DEPENDENCIES = NDR_MISC NDR_SECURITY_HELPER

[LIBRARY::NDR_AUDIOSRV]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_audiosrv.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_DNSSERVER]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dnsserver.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_WINSTATION]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_winstation.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_ECHO]
OBJ_FILES = gen_ndr/ndr_echo.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_IRPC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_irpc.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_DSBACKUP]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dsbackup.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_EFS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_efs.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_MISC]
OBJ_FILES = gen_ndr/ndr_misc.o
PUBLIC_HEADERS = gen_ndr/misc.h gen_ndr/ndr_misc.h
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_ROT]
OBJ_FILES = gen_ndr/ndr_rot.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_LSA]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_lsa.o
PUBLIC_HEADERS = gen_ndr/lsa.h
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_DFS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dfs.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_DRSUAPI]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_drsuapi.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_DRSUAPI_PRINT NDR_COMPRESSION NDR_SECURITY

[SUBSYSTEM::NDR_DRSUAPI_PRINT]
PRIVATE_PROTO_HEADER = ndr/ndr_drsuapi.h
OBJ_FILES = ndr/ndr_drsuapi.o

[LIBRARY::NDR_DRSBLOBS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_drsblobs.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_SASL_HELPERS]
OBJ_FILES = gen_ndr/ndr_sasl_helpers.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_POLICYAGENT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_policyagent.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_UNIXINFO]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_unixinfo.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_SAMR]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_samr.o
PUBLIC_HEADERS = gen_ndr/samr.h
PUBLIC_DEPENDENCIES = LIBNDR NDR_MISC NDR_LSA NDR_SECURITY

[LIBRARY::NDR_SPOOLSS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_spoolss.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_SPOOLSS_BUF

[SUBSYSTEM::NDR_SPOOLSS_BUF]
PRIVATE_PROTO_HEADER = ndr/ndr_spoolss_buf.h
OBJ_FILES = ndr/ndr_spoolss_buf.o

[LIBRARY::NDR_WKSSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_wkssvc.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_SRVSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_srvsvc.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_SVCCTL

[LIBRARY::NDR_SVCCTL]
VERSION = 0.0.1
PUBLIC_HEADERS = gen_ndr/svcctl.h
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_svcctl.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_ATSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_atsvc.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_EVENTLOG]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_eventlog.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_EPMAPPER]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_epmapper.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_DBGIDL]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dbgidl.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_DSSETUP]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dssetup.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_MSGSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_msgsvc.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_WINS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_wins.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_WINREG]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_winreg.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_INITSHUTDOWN

[LIBRARY::NDR_INITSHUTDOWN]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_initshutdown.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_MGMT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_mgmt.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_PROTECTED_STORAGE]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_protected_storage.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_DCOM]
OBJ_FILES = gen_ndr/ndr_dcom.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC

[SUBSYSTEM::NDR_ORPC_MANUAL]
PRIVATE_PROTO_HEADER = ndr/ndr_orpc.h
OBJ_FILES = ndr/ndr_orpc.o 

[SUBSYSTEM::NDR_ORPC]
OBJ_FILES = gen_ndr/ndr_orpc.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC_MANUAL

[LIBRARY::NDR_OXIDRESOLVER]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_oxidresolver.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC

[LIBRARY::NDR_REMACT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_remact.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_ORPC

[LIBRARY::NDR_WZCSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_wzcsvc.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_BROWSER]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_browser.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_W32TIME]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_w32time.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_SCERPC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_scerpc.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_NTSVCS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_ntsvcs.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_NETLOGON]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_netlogon.o
PUBLIC_HEADERS = gen_ndr/netlogon.h
PUBLIC_DEPENDENCIES = LIBNDR NDR_SAMR NDR_LSA

[LIBRARY::NDR_TRKWKS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_trkwks.o
PUBLIC_DEPENDENCIES = LIBNDR

[LIBRARY::NDR_KEYSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_keysvc.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_KRB5PAC]
OBJ_FILES = gen_ndr/ndr_krb5pac.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_KRB5PAC_UTIL NDR_NETLOGON

[SUBSYSTEM::NDR_KRB5PAC_UTIL]
PRIVATE_PROTO_HEADER = ndr/ndr_krb5pac.h
OBJ_FILES = ndr/ndr_krb5pac.o

[LIBRARY::NDR_XATTR]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_xattr.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_SECURITY

[SUBSYSTEM::NDR_OPENDB]
OBJ_FILES = gen_ndr/ndr_opendb.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_NOTIFY]
OBJ_FILES = gen_ndr/ndr_notify.o
PUBLIC_DEPENDENCIES = LIBNDR

[SUBSYSTEM::NDR_SCHANNEL]
OBJ_FILES = gen_ndr/ndr_schannel.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_NBT

[SUBSYSTEM::NDR_NBT]
OBJ_FILES = gen_ndr/ndr_nbt.o
PUBLIC_HEADERS = gen_ndr/nbt.h
PUBLIC_DEPENDENCIES = LIBNDR NDR_MISC NDR_NBT_BUF NDR_SVCCTL NDR_SECURITY

[LIBRARY::NDR_WINSREPL]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_winsrepl.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_NBT

librpc/gen_ndr/tables.c: $(IDL_NDR_PARSE_H_FILES)
	@echo Generating librpc/gen_ndr/tables.c
	@$(PERL) $(srcdir)/librpc/tables.pl --output=librpc/gen_ndr/tables.c $(IDL_NDR_PARSE_H_FILES) > librpc/gen_ndr/tables.x
	mv librpc/gen_ndr/tables.x librpc/gen_ndr/tables.c

[SUBSYSTEM::NDR_IFACE_TABLE]
OBJ_FILES = gen_ndr/tables.o

[LIBRARY::NDR_TABLE]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = rpc/table.o 
PRIVATE_PROTO_HEADER = rpc/dcerpc_table.h
PUBLIC_DEPENDENCIES = \
	NDR_IFACE_TABLE \
	NDR_AUDIOSRV NDR_ECHO NDR_DCERPC \
	NDR_DSBACKUP NDR_EFS NDR_MISC NDR_LSA NDR_DFS NDR_DRSUAPI \
	NDR_POLICYAGENT NDR_UNIXINFO NDR_SAMR NDR_SPOOLSS NDR_WKSSVC NDR_SRVSVC NDR_ATSVC \
	NDR_EVENTLOG NDR_EPMAPPER NDR_DBGIDL NDR_DSSETUP NDR_MSGSVC NDR_WINS \
	NDR_WINREG NDR_MGMT NDR_PROTECTED_STORAGE NDR_OXIDRESOLVER \
	NDR_REMACT NDR_WZCSVC NDR_BROWSER NDR_W32TIME NDR_SCERPC NDR_NTSVCS \
	NDR_NETLOGON NDR_TRKWKS NDR_KEYSVC NDR_KRB5PAC NDR_XATTR NDR_SCHANNEL \
	NDR_ROT NDR_DRSBLOBS NDR_SVCCTL NDR_NBT NDR_WINSREPL NDR_SECURITY \
	NDR_INITSHUTDOWN NDR_DNSSERVER NDR_WINSTATION NDR_IRPC NDR_DCOM NDR_OPENDB \
	NDR_SASL_HELPERS NDR_NOTIFY

[LIBRARY::RPC_NDR_ROT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_rot_c.o
PUBLIC_DEPENDENCIES = NDR_ROT dcerpc

[LIBRARY::RPC_NDR_AUDIOSRV]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_audiosrv_c.o
PUBLIC_DEPENDENCIES = NDR_AUDIOSRV dcerpc

[LIBRARY::RPC_NDR_ECHO]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_echo_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_ECHO

[LIBRARY::RPC_NDR_DSBACKUP]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dsbackup_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_DSBACKUP

[LIBRARY::RPC_NDR_EFS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_efs_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_EFS

[LIBRARY::RPC_NDR_LSA]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_lsa_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_LSA

[LIBRARY::RPC_NDR_DFS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dfs_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_DFS

[LIBRARY::RPC_NDR_DRSUAPI]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_drsuapi_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_DRSUAPI

[LIBRARY::RPC_NDR_POLICYAGENT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_policyagent_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_POLICYAGENT

[LIBRARY::RPC_NDR_UNIXINFO]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_unixinfo_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_UNIXINFO

[LIBRARY::RPC_NDR_SAMR]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_samr_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_SAMR

[LIBRARY::RPC_NDR_SPOOLSS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_spoolss_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_SPOOLSS

[LIBRARY::RPC_NDR_WKSSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_wkssvc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_WKSSVC

[LIBRARY::RPC_NDR_SRVSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_srvsvc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_SRVSVC

[LIBRARY::RPC_NDR_SVCCTL]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_svcctl_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_SVCCTL

[LIBRARY::RPC_NDR_ATSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_atsvc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_ATSVC

[LIBRARY::RPC_NDR_EVENTLOG]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_eventlog_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_EVENTLOG

[LIBRARY::RPC_NDR_EPMAPPER]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_epmapper_c.o
PUBLIC_DEPENDENCIES = NDR_EPMAPPER

[LIBRARY::RPC_NDR_DBGIDL]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dbgidl_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_DBGIDL

[LIBRARY::RPC_NDR_DSSETUP]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_dssetup_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_DSSETUP

[LIBRARY::RPC_NDR_MSGSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_msgsvc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_MSGSVC

[LIBRARY::RPC_NDR_WINS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_wins_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_WINS

[LIBRARY::RPC_NDR_WINREG]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_winreg_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_WINREG

[LIBRARY::RPC_NDR_INITSHUTDOWN]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_initshutdown_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_INITSHUTDOWN

[LIBRARY::RPC_NDR_MGMT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_mgmt_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_MGMT

[LIBRARY::RPC_NDR_PROTECTED_STORAGE]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_protected_storage_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_PROTECTED_STORAGE

[SUBSYSTEM::DCOM_PROXY_DCOM]
OBJ_FILES = gen_ndr/ndr_dcom_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_DCOM

[LIBRARY::RPC_NDR_OXIDRESOLVER]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_oxidresolver_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_OXIDRESOLVER

[LIBRARY::RPC_NDR_REMACT]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_remact_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_REMACT

[LIBRARY::RPC_NDR_WZCSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_wzcsvc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_WZCSVC

[LIBRARY::RPC_NDR_W32TIME]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_w32time_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_W32TIME

[LIBRARY::RPC_NDR_SCERPC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_scerpc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_SCERPC

[LIBRARY::RPC_NDR_NTSVCS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_ntsvcs_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_NTSVCS

[LIBRARY::RPC_NDR_NETLOGON]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_netlogon_c.o
PUBLIC_DEPENDENCIES = NDR_NETLOGON

[LIBRARY::RPC_NDR_TRKWKS]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_trkwks_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_TRKWKS

[LIBRARY::RPC_NDR_KEYSVC]
VERSION = 0.0.1
SO_VERSION = 0
OBJ_FILES = gen_ndr/ndr_keysvc_c.o
PUBLIC_DEPENDENCIES = dcerpc NDR_KEYSVC

[SUBSYSTEM::NDR_DCERPC]
OBJ_FILES = gen_ndr/ndr_dcerpc.o
PUBLIC_DEPENDENCIES = LIBNDR NDR_MISC
PUBLIC_HEADERS = gen_ndr/dcerpc.h gen_ndr/ndr_dcerpc.h

################################################
# Start SUBSYSTEM dcerpc
[LIBRARY::dcerpc]
VERSION = 0.0.1
SO_VERSION = 0
DESCRIPTION = DCE/RPC client library
PUBLIC_HEADERS = rpc/dcerpc.h
PUBLIC_PROTO_HEADER = rpc/dcerpc_proto.h
OBJ_FILES = \
		rpc/dcerpc.o \
		rpc/dcerpc_auth.o \
		rpc/dcerpc_schannel.o \
		rpc/dcerpc_util.o \
		rpc/dcerpc_error.o \
		rpc/dcerpc_smb.o \
		rpc/dcerpc_smb2.o \
		rpc/dcerpc_sock.o \
		rpc/dcerpc_connect.o
PUBLIC_DEPENDENCIES = \
		SOCKET LIBSMB \
		LIBNDR NDR_DCERPC \
		RPC_NDR_EPMAPPER \
		NDR_SCHANNEL RPC_NDR_NETLOGON \
		gensec
# End SUBSYSTEM dcerpc
################################################

[MODULE::RPC_EJS_ECHO]
INIT_FUNCTION = ejs_init_rpcecho
OBJ_FILES = gen_ndr/ndr_echo_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_ECHO EJSRPC

[MODULE::RPC_EJS_MISC]
INIT_FUNCTION = ejs_init_misc
OBJ_FILES = gen_ndr/ndr_misc_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_MISC EJSRPC

[MODULE::RPC_EJS_SAMR]
INIT_FUNCTION = ejs_init_samr
OBJ_FILES = gen_ndr/ndr_samr_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_SAMR EJSRPC RPC_EJS_LSA RPC_EJS_SECURITY RPC_EJS_MISC

[MODULE::RPC_EJS_SECURITY]
INIT_FUNCTION = ejs_init_security
OBJ_FILES = gen_ndr/ndr_security_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_SECURITY EJSRPC

[MODULE::RPC_EJS_LSA]
INIT_FUNCTION = ejs_init_lsarpc
OBJ_FILES = gen_ndr/ndr_lsa_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_LSA EJSRPC RPC_EJS_SECURITY RPC_EJS_MISC

[MODULE::RPC_EJS_DFS]
INIT_FUNCTION = ejs_init_netdfs
OBJ_FILES = gen_ndr/ndr_dfs_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_DFS EJSRPC

[MODULE::RPC_EJS_DRSUAPI]
INIT_FUNCTION = ejs_init_drsuapi
OBJ_FILES = gen_ndr/ndr_drsuapi_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_DRSUAPI EJSRPC RPC_EJS_MISC

[MODULE::RPC_EJS_SPOOLSS]
INIT_FUNCTION = ejs_init_spoolss
OBJ_FILES = gen_ndr/ndr_spoolss_ejs.o
SUBSYSTEM = smbcalls
ENABLE = NO
PUBLIC_DEPENDENCIES = dcerpc NDR_SPOOLSS EJSRPC

[MODULE::RPC_EJS_WKSSVC]
INIT_FUNCTION = ejs_init_wkssvc
OBJ_FILES = gen_ndr/ndr_wkssvc_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_WKSSVC EJSRPC RPC_EJS_SRVSVC RPC_EJS_MISC

[MODULE::RPC_EJS_SRVSVC]
INIT_FUNCTION = ejs_init_srvsvc
OBJ_FILES = gen_ndr/ndr_srvsvc_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_SRVSVC EJSRPC RPC_EJS_MISC RPC_EJS_SVCCTL

[MODULE::RPC_EJS_EVENTLOG]
INIT_FUNCTION = ejs_init_eventlog
OBJ_FILES = gen_ndr/ndr_eventlog_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_EVENTLOG EJSRPC RPC_EJS_MISC

[MODULE::RPC_EJS_WINREG]
INIT_FUNCTION = ejs_init_winreg
OBJ_FILES = gen_ndr/ndr_winreg_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_WINREG EJSRPC RPC_EJS_INITSHUTDOWN \
					  RPC_EJS_MISC RPC_EJS_SECURITY

[MODULE::RPC_EJS_INITSHUTDOWN]
INIT_FUNCTION = ejs_init_initshutdown
OBJ_FILES = gen_ndr/ndr_initshutdown_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_INITSHUTDOWN EJSRPC

[MODULE::RPC_EJS_NETLOGON]
INIT_FUNCTION = ejs_init_netlogon
OBJ_FILES = gen_ndr/ndr_netlogon_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_NETLOGON EJSRPC RPC_EJS_SAMR RPC_EJS_SECURITY RPC_EJS_MISC

[MODULE::RPC_EJS_SVCCTL]
INIT_FUNCTION = ejs_init_svcctl
OBJ_FILES = gen_ndr/ndr_svcctl_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_SVCCTL EJSRPC RPC_EJS_MISC

[MODULE::RPC_EJS_IRPC]
INIT_FUNCTION = ejs_init_irpc
OBJ_FILES = gen_ndr/ndr_irpc_ejs.o
SUBSYSTEM = smbcalls
PUBLIC_DEPENDENCIES = dcerpc NDR_IRPC EJSRPC
