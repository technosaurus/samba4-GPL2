################################################
# Start SUBSYSTEM LIBNDR
[LIBRARY::LIBNDR]
MAJOR_VERSION = 0
MINOR_VERSION = 0
RELEASE_VERSION = 1
DESCRIPTION = Network Data Representation Core Library
PUBLIC_HEADERS = ndr/libndr.h
PRIVATE_PROTO_HEADER = ndr/libndr_proto.h
OBJ_FILES = \
		ndr/ndr.o \
		ndr/ndr_basic.o \
		ndr/ndr_string.o \
		ndr/ndr_obfuscate.o \
		ndr/ndr_misc.o
REQUIRED_SUBSYSTEMS = LIBCLI_UTILS LIBTALLOC
# End SUBSYSTEM LIBNDR
################################################

################################################
# Start SUBSYSTEM NDR_COMPRESSION
[SUBSYSTEM::NDR_COMPRESSION]
OBJ_FILES = \
		ndr/ndr_compression.o
REQUIRED_SUBSYSTEMS = LIBCOMPRESSION
# End SUBSYSTEM NDR_COMPRESSION
################################################

include rpc/config.mk

[SUBSYSTEM::NDR_SECURITY_HELPER]
OBJ_FILES = ndr/ndr_sec_helper.o ndr/ndr_sec.o

[SUBSYSTEM::NDR_SECURITY]
OBJ_FILES = gen_ndr/ndr_security.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_SECURITY_HELPER

[SUBSYSTEM::NDR_AUDIOSRV]
OBJ_FILES = gen_ndr/ndr_audiosrv.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DNSSERVER]
OBJ_FILES = gen_ndr/ndr_dnsserver.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_WINSTATION]
OBJ_FILES = gen_ndr/ndr_winstation.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DCERPC]
OBJ_FILES = gen_ndr/ndr_dcerpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_ECHO]
OBJ_FILES = gen_ndr/ndr_echo.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_IRPC]
OBJ_FILES = gen_ndr/ndr_irpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_EXCHANGE]
OBJ_FILES = gen_ndr/ndr_exchange.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DSBACKUP]
OBJ_FILES = gen_ndr/ndr_dsbackup.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_EFS]
OBJ_FILES = gen_ndr/ndr_efs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_MISC]
OBJ_FILES = gen_ndr/ndr_misc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_ROT]
OBJ_FILES = gen_ndr/ndr_rot.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_LSA]
OBJ_FILES = gen_ndr/ndr_lsa.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DFS]
OBJ_FILES = gen_ndr/ndr_dfs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DRSUAPI]
OBJ_FILES = gen_ndr/ndr_drsuapi.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_DRSUAPI_PRINT NDR_COMPRESSION NDR_SECURITY

[SUBSYSTEM::NDR_DRSUAPI_PRINT]
OBJ_FILES = ndr/ndr_drsuapi.o

[SUBSYSTEM::NDR_DRSBLOBS]
OBJ_FILES = gen_ndr/ndr_drsblobs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_POLICYAGENT]
OBJ_FILES = gen_ndr/ndr_policyagent.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_UNIXINFO]
OBJ_FILES = gen_ndr/ndr_unixinfo.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_SAMR]
OBJ_FILES = gen_ndr/ndr_samr.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_SPOOLSS]
OBJ_FILES = gen_ndr/ndr_spoolss.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_SPOOLSS_BUF

[SUBSYSTEM::NDR_SPOOLSS_BUF]
OBJ_FILES = ndr/ndr_spoolss_buf.o

[SUBSYSTEM::NDR_WKSSVC]
OBJ_FILES = gen_ndr/ndr_wkssvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_SRVSVC]
OBJ_FILES = gen_ndr/ndr_srvsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_SVCCTL

[SUBSYSTEM::NDR_SVCCTL]
OBJ_FILES = gen_ndr/ndr_svcctl.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_ATSVC]
OBJ_FILES = gen_ndr/ndr_atsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_EVENTLOG]
OBJ_FILES = gen_ndr/ndr_eventlog.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_EPMAPPER]
OBJ_FILES = gen_ndr/ndr_epmapper.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DBGIDL]
OBJ_FILES = gen_ndr/ndr_dbgidl.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DSSETUP]
OBJ_FILES = gen_ndr/ndr_dssetup.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_MSGSVC]
OBJ_FILES = gen_ndr/ndr_msgsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_WINS]
OBJ_FILES = gen_ndr/ndr_wins.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_WINREG]
OBJ_FILES = gen_ndr/ndr_winreg.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_INITSHUTDOWN

[SUBSYSTEM::NDR_INITSHUTDOWN]
OBJ_FILES = gen_ndr/ndr_initshutdown.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_MGMT]
OBJ_FILES = gen_ndr/ndr_mgmt.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_PROTECTED_STORAGE]
OBJ_FILES = gen_ndr/ndr_protected_storage.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_DCOM]
OBJ_FILES = gen_ndr/ndr_dcom.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_ORPC

[SUBSYSTEM::NDR_ORPC_MANUAL]
OBJ_FILES = ndr/ndr_orpc.o 

[SUBSYSTEM::NDR_ORPC]
OBJ_FILES = gen_ndr/ndr_orpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_ORPC_MANUAL

[SUBSYSTEM::NDR_OXIDRESOLVER]
OBJ_FILES = gen_ndr/ndr_oxidresolver.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_ORPC

[SUBSYSTEM::NDR_REMACT]
OBJ_FILES = gen_ndr/ndr_remact.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_ORPC

[SUBSYSTEM::NDR_WZCSVC]
OBJ_FILES = gen_ndr/ndr_wzcsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_BROWSER]
OBJ_FILES = gen_ndr/ndr_browser.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_W32TIME]
OBJ_FILES = gen_ndr/ndr_w32time.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_SCERPC]
OBJ_FILES = gen_ndr/ndr_scerpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_NTSVCS]
OBJ_FILES = gen_ndr/ndr_ntsvcs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_NETLOGON]
OBJ_FILES = gen_ndr/ndr_netlogon.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_SAMR NDR_LSA

[SUBSYSTEM::NDR_TRKWKS]
OBJ_FILES = gen_ndr/ndr_trkwks.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_KEYSVC]
OBJ_FILES = gen_ndr/ndr_keysvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_KRB5PAC]
OBJ_FILES = gen_ndr/ndr_krb5pac.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_KRB5PAC_UTIL

[SUBSYSTEM::NDR_KRB5PAC_UTIL]
OBJ_FILES = ndr/ndr_krb5pac.o

[SUBSYSTEM::NDR_XATTR]
OBJ_FILES = gen_ndr/ndr_xattr.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_SCHANNEL]
OBJ_FILES = gen_ndr/ndr_schannel.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR

[SUBSYSTEM::NDR_NBT]
OBJ_FILES = gen_ndr/ndr_nbt.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_MISC NDR_NBT_BUF

[SUBSYSTEM::NDR_WINSREPL]
OBJ_FILES = gen_ndr/ndr_winsrepl.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = LIBNDR NDR_NBT

librpc/gen_ndr/tables.c: librpc/gen_ndr/ndr_*.h
	@$(PERL) librpc/tables.pl --output=librpc/gen_ndr/tables.c \
									librpc/gen_ndr/ndr_*.h

[SUBSYSTEM::NDR_IFACE_TABLE]
OBJ_FILES = gen_ndr/tables.o
NOPROTO = YES

[SUBSYSTEM::NDR_ALL]
OBJ_FILES = rpc/table.o 
PRIVATE_PROTO_HEADER = rpc/dcerpc_table.h
REQUIRED_SUBSYSTEMS = NDR_IFACE_TABLE NDR_AUDIOSRV NDR_ECHO NDR_DCERPC NDR_EXCHANGE \
	NDR_DSBACKUP NDR_EFS NDR_MISC NDR_LSA NDR_DFS NDR_DRSUAPI \
	NDR_POLICYAGENT NDR_UNIXINFO NDR_SAMR NDR_SPOOLSS NDR_WKSSVC NDR_SRVSVC NDR_ATSVC \
	NDR_EVENTLOG NDR_EPMAPPER NDR_DBGIDL NDR_DSSETUP NDR_MSGSVC NDR_WINS \
	NDR_WINREG NDR_MGMT NDR_PROTECTED_STORAGE NDR_OXIDRESOLVER \
	NDR_REMACT NDR_WZCSVC NDR_BROWSER NDR_W32TIME NDR_SCERPC NDR_NTSVCS \
	NDR_NETLOGON NDR_TRKWKS NDR_KEYSVC NDR_KRB5PAC NDR_XATTR NDR_SCHANNEL \
	NDR_ROT NDR_DRSBLOBS NDR_SVCCTL NDR_NBT NDR_WINSREPL NDR_SECURITY \
	NDR_INITSHUTDOWN NDR_DNSSERVER NDR_WINSTATION NDR_IRPC NDR_DCOM

[SUBSYSTEM::RPC_NDR_ROT]
OBJ_FILES = gen_ndr/ndr_rot_c.o
REQUIRED_SUBSYSTEMS = NDR_ROT LIBRPC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_AUDIOSRV]
OBJ_FILES = gen_ndr/ndr_audiosrv_c.o
REQUIRED_SUBSYSTEMS = NDR_AUDIOSRV LIBRPC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_ECHO]
OBJ_FILES = gen_ndr/ndr_echo_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_ECHO
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EXCHANGE]
OBJ_FILES = gen_ndr/ndr_exchange_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_EXCHANGE
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DSBACKUP]
OBJ_FILES = gen_ndr/ndr_dsbackup_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DSBACKUP
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EFS]
OBJ_FILES = gen_ndr/ndr_efs_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_EFS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_LSA]
OBJ_FILES = gen_ndr/ndr_lsa_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_LSA
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DFS]
OBJ_FILES = gen_ndr/ndr_dfs_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DFS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DRSUAPI]
OBJ_FILES = gen_ndr/ndr_drsuapi_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DRSUAPI
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DRSBLOBS]
OBJ_FILES = gen_ndr/ndr_drsblobs_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DRSBLOBS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_POLICYAGENT]
OBJ_FILES = gen_ndr/ndr_policyagent_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_POLICYAGENT
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_UNIXINFO]
OBJ_FILES = gen_ndr/ndr_unixinfo_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_UNIXINFO
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SAMR]
OBJ_FILES = gen_ndr/ndr_samr_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SAMR
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SPOOLSS]
OBJ_FILES = gen_ndr/ndr_spoolss_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SPOOLSS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WKSSVC]
OBJ_FILES = gen_ndr/ndr_wkssvc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_WKSSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SRVSVC]
OBJ_FILES = gen_ndr/ndr_srvsvc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SRVSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SVCCTL]
OBJ_FILES = gen_ndr/ndr_svcctl_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SVCCTL
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_ATSVC]
OBJ_FILES = gen_ndr/ndr_atsvc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_ATSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EVENTLOG]
OBJ_FILES = gen_ndr/ndr_eventlog_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_EVENTLOG
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EPMAPPER]
OBJ_FILES = gen_ndr/ndr_epmapper_c.o
REQUIRED_SUBSYSTEMS = NDR_EPMAPPER
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DBGIDL]
OBJ_FILES = gen_ndr/ndr_dbgidl_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DBGIDL
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DSSETUP]
OBJ_FILES = gen_ndr/ndr_dssetup_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DSSETUP
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_MSGSVC]
OBJ_FILES = gen_ndr/ndr_msgsvc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_MSGSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WINS]
OBJ_FILES = gen_ndr/ndr_wins_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_WINS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WINREG]
OBJ_FILES = gen_ndr/ndr_winreg_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_WINREG
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_INITSHUTDOWN]
OBJ_FILES = gen_ndr/ndr_initshutdown_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_INITSHUTDOWN
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_MGMT]
OBJ_FILES = gen_ndr/ndr_mgmt_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_MGMT
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_PROTECTED_STORAGE]
OBJ_FILES = gen_ndr/ndr_protected_storage_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_PROTECTED_STORAGE
NOPROTO = YES

[SUBSYSTEM::DCOM_PROXY_DCOM]
OBJ_FILES = gen_ndr/ndr_dcom_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DCOM
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_OXIDRESOLVER]
OBJ_FILES = gen_ndr/ndr_oxidresolver_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_OXIDRESOLVER
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_REMACT]
OBJ_FILES = gen_ndr/ndr_remact_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_REMACT
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WZCSVC]
OBJ_FILES = gen_ndr/ndr_wzcsvc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_WZCSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_BROWSER]
OBJ_FILES = gen_ndr/ndr_browser_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_BROWSER
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_W32TIME]
OBJ_FILES = gen_ndr/ndr_w32time_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_W32TIME
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SCERPC]
OBJ_FILES = gen_ndr/ndr_scerpc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SCERPC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_NTSVCS]
OBJ_FILES = gen_ndr/ndr_ntsvcs_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_NTSVCS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_NETLOGON]
OBJ_FILES = gen_ndr/ndr_netlogon_c.o
REQUIRED_SUBSYSTEMS = NDR_NETLOGON
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_TRKWKS]
OBJ_FILES = gen_ndr/ndr_trkwks_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_TRKWKS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_KEYSVC]
OBJ_FILES = gen_ndr/ndr_keysvc_c.o
REQUIRED_SUBSYSTEMS = LIBRPC NDR_KEYSVC
NOPROTO = YES

################################################
# Start SUBSYSTEM LIBRPC
[LIBRARY::LIBRPC]
MAJOR_VERSION = 0
MINOR_VERSION = 0
DESCRIPTION = DCE/RPC client library
RELEASE_VERSION = 1
PUBLIC_HEADERS = rpc/dcerpc.h
REQUIRED_SUBSYSTEMS = LIBNDR RPC_RAW LIBSMB NDR_MISC NDR_DCERPC NDR_SCHANNEL NDR_LSA NDR_NETLOGON NDR_SAMR NDR_UNIXINFO RPC_NDR_NETLOGON RPC_NDR_EPMAPPER
# End SUBSYSTEM LIBRPC
################################################

[MODULE::RPC_EJS_ECHO]
INIT_FUNCTION = ejs_init_rpcecho
OBJ_FILES = gen_ndr/ndr_echo_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_ECHO
NOPROTO = YES

[MODULE::RPC_EJS_MISC]
INIT_FUNCTION = ejs_init_misc
OBJ_FILES = gen_ndr/ndr_misc_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_MISC
NOPROTO = YES

[MODULE::RPC_EJS_SAMR]
INIT_FUNCTION = ejs_init_samr
OBJ_FILES = gen_ndr/ndr_samr_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SAMR
NOPROTO = YES

[MODULE::RPC_EJS_SECURITY]
INIT_FUNCTION = ejs_init_security
OBJ_FILES = gen_ndr/ndr_security_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SECURITY
NOPROTO = YES

[MODULE::RPC_EJS_LSA]
INIT_FUNCTION = ejs_init_lsarpc
OBJ_FILES = gen_ndr/ndr_lsa_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_LSA
NOPROTO = YES

[MODULE::RPC_EJS_DFS]
INIT_FUNCTION = ejs_init_netdfs
OBJ_FILES = gen_ndr/ndr_dfs_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DFS
NOPROTO = YES

[MODULE::RPC_EJS_DRSUAPI]
INIT_FUNCTION = ejs_init_drsuapi
OBJ_FILES = gen_ndr/ndr_drsuapi_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_DRSUAPI
NOPROTO = YES

[MODULE::RPC_EJS_SPOOLSS]
INIT_FUNCTION = ejs_init_spoolss
OBJ_FILES = gen_ndr/ndr_spoolss_ejs.o
SUBSYSTEM = SMBCALLS
ENABLE = NO
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SPOOLSS
NOPROTO = YES

[MODULE::RPC_EJS_WKSSVC]
INIT_FUNCTION = ejs_init_wkssvc
OBJ_FILES = gen_ndr/ndr_wkssvc_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_WKSSVC
NOPROTO = YES

[MODULE::RPC_EJS_SRVSVC]
INIT_FUNCTION = ejs_init_srvsvc
OBJ_FILES = gen_ndr/ndr_srvsvc_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SRVSVC
NOPROTO = YES

[MODULE::RPC_EJS_EVENTLOG]
INIT_FUNCTION = ejs_init_eventlog
OBJ_FILES = gen_ndr/ndr_eventlog_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_EVENTLOG
NOPROTO = YES

[MODULE::RPC_EJS_WINREG]
INIT_FUNCTION = ejs_init_winreg
OBJ_FILES = gen_ndr/ndr_winreg_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_WINREG
NOPROTO = YES

[MODULE::RPC_EJS_INITSHUTDOWN]
INIT_FUNCTION = ejs_init_initshutdown
OBJ_FILES = gen_ndr/ndr_initshutdown_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_INITSHUTDOWN
NOPROTO = YES

[MODULE::RPC_EJS_NETLOGON]
INIT_FUNCTION = ejs_init_netlogon
OBJ_FILES = gen_ndr/ndr_netlogon_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_NETLOGON
NOPROTO = YES

[MODULE::RPC_EJS_SVCCTL]
INIT_FUNCTION = ejs_init_svcctl
OBJ_FILES = gen_ndr/ndr_svcctl_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_SVCCTL
NOPROTO = YES

[MODULE::RPC_EJS_IRPC]
INIT_FUNCTION = ejs_init_irpc
OBJ_FILES = gen_ndr/ndr_irpc_ejs.o
SUBSYSTEM = SMBCALLS
REQUIRED_SUBSYSTEMS = LIBRPC NDR_IRPC
NOPROTO = YES
