################################################
# Start SUBSYSTEM NDR_RAW
[SUBSYSTEM::NDR_RAW]
INIT_OBJ_FILES = \
		ndr/ndr.o
ADD_OBJ_FILES = \
		ndr/ndr_basic.o \
		ndr/ndr_string.o \
		ndr/ndr_obfuscate.o \
		ndr/ndr_misc.o
REQUIRED_SUBSYSTEMS = LIBCLI_UTILS LIBTALLOC
# End SUBSYSTEM NDR_RAW
################################################

################################################
# Start SUBSYSTEM NDR_COMPRESSION
[SUBSYSTEM::NDR_COMPRESSION]
ADD_OBJ_FILES = \
		ndr/ndr_compression.o
REQUIRED_SUBSYSTEMS = LIBCOMPRESSION
# End SUBSYSTEM NDR_COMPRESSION
################################################

################################################
# Start SUBSYSTEM RPC_RAW
[SUBSYSTEM::RPC_RAW]
INIT_OBJ_FILES = \
		rpc/dcerpc.o
ADD_OBJ_FILES = \
		rpc/dcerpc_auth.o \
		rpc/dcerpc_schannel.o \
		rpc/dcerpc_util.o \
		rpc/dcerpc_error.o \
		rpc/dcerpc_smb.o \
		rpc/dcerpc_sock.o
REQUIRED_SUBSYSTEMS = SOCKET
# End SUBSYSTEM RPC_RAW
################################################

[SUBSYSTEM::NDR_AUDIOSRV]
INIT_FUNCTION = dcerpc_audiosrv_init
INIT_OBJ_FILES = gen_ndr/ndr_audiosrv.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DCERPC]
INIT_FUNCTION = dcerpc_dcerpc_init
INIT_OBJ_FILES = gen_ndr/ndr_dcerpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_ECHO]
INIT_FUNCTION = dcerpc_echo_init
INIT_OBJ_FILES = gen_ndr/ndr_echo.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_IRPC]
INIT_FUNCTION = dcerpc_irpc_init
INIT_OBJ_FILES = gen_ndr/ndr_irpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_EXCHANGE]
INIT_FUNCTION = dcerpc_exchange_init
INIT_OBJ_FILES = gen_ndr/ndr_exchange.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DSBACKUP]
INIT_FUNCTION = dcerpc_dsbackup_init
INIT_OBJ_FILES = gen_ndr/ndr_dsbackup.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_EFS]
INIT_FUNCTION = dcerpc_efs_init
INIT_OBJ_FILES = gen_ndr/ndr_efs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_MISC]
INIT_FUNCTION = dcerpc_misc_init
INIT_OBJ_FILES = gen_ndr/ndr_misc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_ROT]
INIT_FUNCTION = dcerpc_rot_init
INIT_OBJ_FILES = gen_ndr/ndr_rot.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_LSA]
INIT_FUNCTION = dcerpc_lsa_init
INIT_OBJ_FILES = gen_ndr/ndr_lsa.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DFS]
INIT_FUNCTION = dcerpc_dfs_init
INIT_OBJ_FILES = gen_ndr/ndr_dfs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DRSUAPI]
INIT_FUNCTION = dcerpc_drsuapi_init
INIT_OBJ_FILES = gen_ndr/ndr_drsuapi.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_DRSUAPI_PRINT NDR_COMPRESSION

[SUBSYSTEM::NDR_DRSUAPI_PRINT]
INIT_OBJ_FILES = ndr/ndr_drsuapi.o

[SUBSYSTEM::NDR_DRSBLOBS]
INIT_FUNCTION = dcerpc_drsblobs_init
INIT_OBJ_FILES = gen_ndr/ndr_drsblobs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_POLICYAGENT]
INIT_FUNCTION = dcerpc_policyagent_init
INIT_OBJ_FILES = gen_ndr/ndr_policyagent.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_UNIXINFO]
INIT_FUNCTION = dcerpc_unixinfo_init
INIT_OBJ_FILES = gen_ndr/ndr_unixinfo.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_SAMR]
INIT_FUNCTION = dcerpc_samr_init
INIT_OBJ_FILES = gen_ndr/ndr_samr.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_SPOOLSS]
INIT_FUNCTION = dcerpc_spoolss_init
INIT_OBJ_FILES = gen_ndr/ndr_spoolss.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_SPOOLSS_BUF

[SUBSYSTEM::NDR_SPOOLSS_BUF]
INIT_OBJ_FILES = ndr/ndr_spoolss_buf.o

[SUBSYSTEM::NDR_WKSSVC]
INIT_FUNCTION = dcerpc_wkssvc_init
INIT_OBJ_FILES = gen_ndr/ndr_wkssvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_SRVSVC]
INIT_FUNCTION = dcerpc_srvsvc_init
INIT_OBJ_FILES = gen_ndr/ndr_srvsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_SVCCTL

[SUBSYSTEM::NDR_SVCCTL]
INIT_FUNCTION = dcerpc_svcctl_init
INIT_OBJ_FILES = gen_ndr/ndr_svcctl.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_ATSVC]
INIT_FUNCTION = dcerpc_atsvc_init
INIT_OBJ_FILES = gen_ndr/ndr_atsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_EVENTLOG]
INIT_FUNCTION = dcerpc_eventlog_init
INIT_OBJ_FILES = gen_ndr/ndr_eventlog.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_EPMAPPER]
INIT_FUNCTION = dcerpc_epmapper_init
INIT_OBJ_FILES = gen_ndr/ndr_epmapper.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DBGIDL]
INIT_FUNCTION = dcerpc_dbgidl_init
INIT_OBJ_FILES = gen_ndr/ndr_dbgidl.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DSSETUP]
INIT_FUNCTION = dcerpc_dssetup_init
INIT_OBJ_FILES = gen_ndr/ndr_dssetup.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_MSGSVC]
INIT_FUNCTION = dcerpc_msgsvc_init
INIT_OBJ_FILES = gen_ndr/ndr_msgsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_WINS]
INIT_FUNCTION = dcerpc_wins_init
INIT_OBJ_FILES = gen_ndr/ndr_wins.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_WINREG]
INIT_FUNCTION = dcerpc_winreg_init
INIT_OBJ_FILES = gen_ndr/ndr_winreg.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_INITSHUTDOWN

[SUBSYSTEM::NDR_INITSHUTDOWN]
INIT_FUNCTION = dcerpc_initshutdown_init
INIT_OBJ_FILES = gen_ndr/ndr_initshutdown.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_MGMT]
INIT_FUNCTION = dcerpc_mgmt_init
INIT_OBJ_FILES = gen_ndr/ndr_mgmt.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_PROTECTED_STORAGE]
INIT_FUNCTION = dcerpc_protected_storage_init
INIT_OBJ_FILES = gen_ndr/ndr_protected_storage.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_DCOM]
INIT_FUNCTION = dcerpc_dcom_init
INIT_OBJ_FILES = gen_ndr/ndr_dcom.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_ORPC

[SUBSYSTEM::NDR_ORPC_MANUAL]
INIT_OBJ_FILES = ndr/ndr_orpc.o 

[SUBSYSTEM::NDR_ORPC]
INIT_FUNCTION = dcerpc_orpc_init
INIT_OBJ_FILES = gen_ndr/ndr_orpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_ORPC_MANUAL

[SUBSYSTEM::NDR_OXIDRESOLVER]
INIT_FUNCTION = dcerpc_oxidresolver_init
INIT_OBJ_FILES = gen_ndr/ndr_oxidresolver.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_ORPC

[SUBSYSTEM::NDR_REMACT]
INIT_FUNCTION = dcerpc_remact_init
INIT_OBJ_FILES = gen_ndr/ndr_remact.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_ORPC

[SUBSYSTEM::NDR_WZCSVC]
INIT_FUNCTION = dcerpc_wzcsvc_init
INIT_OBJ_FILES = gen_ndr/ndr_wzcsvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_BROWSER]
INIT_FUNCTION = dcerpc_browser_init
INIT_OBJ_FILES = gen_ndr/ndr_browser.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_W32TIME]
INIT_FUNCTION = dcerpc_w32time_init
INIT_OBJ_FILES = gen_ndr/ndr_w32time.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_SCERPC]
INIT_FUNCTION = dcerpc_scerpc_init
INIT_OBJ_FILES = gen_ndr/ndr_scerpc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_NTSVCS]
INIT_FUNCTION = dcerpc_ntsvcs_init
INIT_OBJ_FILES = gen_ndr/ndr_ntsvcs.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_NETLOGON]
INIT_FUNCTION = dcerpc_netlogon_init
INIT_OBJ_FILES = gen_ndr/ndr_netlogon.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_SAMR

[SUBSYSTEM::NDR_TRKWKS]
INIT_FUNCTION = dcerpc_trkwks_init
INIT_OBJ_FILES = gen_ndr/ndr_trkwks.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_KEYSVC]
INIT_FUNCTION = dcerpc_keysvc_init
INIT_OBJ_FILES = gen_ndr/ndr_keysvc.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_KRB5PAC]
INIT_FUNCTION = dcerpc_krb5pac_init
INIT_OBJ_FILES = gen_ndr/ndr_krb5pac.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_KRB5PAC_UTIL

[SUBSYSTEM::NDR_KRB5PAC_UTIL]
INIT_OBJ_FILES = ndr/ndr_krb5pac.o

[SUBSYSTEM::NDR_XATTR]
INIT_FUNCTION = dcerpc_xattr_init
INIT_OBJ_FILES = gen_ndr/ndr_xattr.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_SCHANNEL]
INIT_FUNCTION = dcerpc_schannel_init
INIT_OBJ_FILES = gen_ndr/ndr_schannel.o
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW

[SUBSYSTEM::NDR_NBT]
INIT_OBJ_FILES = gen_ndr/ndr_nbt.o
INIT_FUNCTION = dcerpc_nbt_init
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_MISC

[SUBSYSTEM::NDR_WINSREPL]
INIT_OBJ_FILES = gen_ndr/ndr_winsrepl.o
INIT_FUNCTION = dcerpc_winsrepl_init
NOPROTO = YES
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_NBT


[SUBSYSTEM::NDR_ALL]
REQUIRED_SUBSYSTEMS = NDR_AUDIOSRV NDR_ECHO NDR_DCERPC NDR_EXCHANGE \
	NDR_DSBACKUP NDR_EFS NDR_MISC NDR_LSA NDR_DFS NDR_DRSUAPI \
	NDR_POLICYAGENT NDR_UNIXINFO NDR_SAMR NDR_SPOOLSS NDR_WKSSVC NDR_SRVSVC NDR_ATSVC \
	NDR_EVENTLOG NDR_EPMAPPER NDR_DBGIDL NDR_DSSETUP NDR_MSGSVC NDR_WINS \
	NDR_WINREG NDR_MGMT NDR_PROTECTED_STORAGE NDR_OXIDRESOLVER \
	NDR_REMACT NDR_WZCSVC NDR_BROWSER NDR_W32TIME NDR_SCERPC NDR_NTSVCS \
	NDR_NETLOGON NDR_TRKWKS NDR_KEYSVC NDR_KRB5PAC NDR_XATTR NDR_SCHANNEL \
	NDR_ROT NDR_DRSBLOBS NDR_SVCCTL NDR_NBT NDR_WINSREPL LIB_SECURITY_NDR \
	NDR_INITSHUTDOWN

[SUBSYSTEM::RPC_NDR_ROT]
ADD_OBJ_FILES = gen_ndr/ndr_rot_c.o
REQUIRED_SUBSYSTEMS = NDR_ROT RPC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_AUDIOSRV]
ADD_OBJ_FILES = gen_ndr/ndr_audiosrv_c.o
REQUIRED_SUBSYSTEMS = NDR_AUDIOSRV RPC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_ECHO]
ADD_OBJ_FILES = gen_ndr/ndr_echo_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_ECHO
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EXCHANGE]
ADD_OBJ_FILES = gen_ndr/ndr_exchange_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_EXCHANGE
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DSBACKUP]
ADD_OBJ_FILES = gen_ndr/ndr_dsbackup_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DSBACKUP
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EFS]
ADD_OBJ_FILES = gen_ndr/ndr_efs_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_EFS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_LSA]
ADD_OBJ_FILES = gen_ndr/ndr_lsa_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_LSA
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DFS]
ADD_OBJ_FILES = gen_ndr/ndr_dfs_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DFS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DRSUAPI]
ADD_OBJ_FILES = gen_ndr/ndr_drsuapi_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DRSUAPI
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DRSBLOBS]
ADD_OBJ_FILES = gen_ndr/ndr_drsblobs_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DRSBLOBS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_POLICYAGENT]
ADD_OBJ_FILES = gen_ndr/ndr_policyagent_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_POLICYAGENT
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_UNIXINFO]
ADD_OBJ_FILES = gen_ndr/ndr_unixinfo_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_UNIXINFO
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SAMR]
ADD_OBJ_FILES = gen_ndr/ndr_samr_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_SAMR
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SPOOLSS]
ADD_OBJ_FILES = gen_ndr/ndr_spoolss_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_SPOOLSS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WKSSVC]
ADD_OBJ_FILES = gen_ndr/ndr_wkssvc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_WKSSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SRVSVC]
ADD_OBJ_FILES = gen_ndr/ndr_srvsvc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_SRVSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SVCCTL]
ADD_OBJ_FILES = gen_ndr/ndr_svcctl_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_SVCCTL
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_ATSVC]
ADD_OBJ_FILES = gen_ndr/ndr_atsvc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_ATSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EVENTLOG]
ADD_OBJ_FILES = gen_ndr/ndr_eventlog_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_EVENTLOG
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_EPMAPPER]
ADD_OBJ_FILES = gen_ndr/ndr_epmapper_c.o
REQUIRED_SUBSYSTEMS = NDR_EPMAPPER
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DBGIDL]
ADD_OBJ_FILES = gen_ndr/ndr_dbgidl_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DBGIDL
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_DSSETUP]
ADD_OBJ_FILES = gen_ndr/ndr_dssetup_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DSSETUP
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_MSGSVC]
ADD_OBJ_FILES = gen_ndr/ndr_msgsvc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_MSGSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WINS]
ADD_OBJ_FILES = gen_ndr/ndr_wins_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_WINS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WINREG]
ADD_OBJ_FILES = gen_ndr/ndr_winreg_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_WINREG
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_INITSHUTDOWN]
ADD_OBJ_FILES = gen_ndr/ndr_initshutdown_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_INITSHUTDOWN
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_MGMT]
ADD_OBJ_FILES = gen_ndr/ndr_mgmt_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_MGMT
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_PROTECTED_STORAGE]
ADD_OBJ_FILES = gen_ndr/ndr_protected_storage_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_PROTECTED_STORAGE
NOPROTO = YES

[SUBSYSTEM::DCOM_PROXY_DCOM]
ADD_OBJ_FILES = gen_ndr/ndr_dcom_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_DCOM
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_OXIDRESOLVER]
ADD_OBJ_FILES = gen_ndr/ndr_oxidresolver_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_OXIDRESOLVER
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_REMACT]
ADD_OBJ_FILES = gen_ndr/ndr_remact_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_REMACT
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_WZCSVC]
ADD_OBJ_FILES = gen_ndr/ndr_wzcsvc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_WZCSVC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_BROWSER]
ADD_OBJ_FILES = gen_ndr/ndr_browser_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_BROWSER
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_W32TIME]
ADD_OBJ_FILES = gen_ndr/ndr_w32time_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_W32TIME
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_SCERPC]
ADD_OBJ_FILES = gen_ndr/ndr_scerpc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_SCERPC
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_NTSVCS]
ADD_OBJ_FILES = gen_ndr/ndr_ntsvcs_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_NTSVCS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_NETLOGON]
ADD_OBJ_FILES = gen_ndr/ndr_netlogon_c.o
REQUIRED_SUBSYSTEMS = NDR_NETLOGON
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_TRKWKS]
ADD_OBJ_FILES = gen_ndr/ndr_trkwks_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_TRKWKS
NOPROTO = YES

[SUBSYSTEM::RPC_NDR_KEYSVC]
ADD_OBJ_FILES = gen_ndr/ndr_keysvc_c.o
REQUIRED_SUBSYSTEMS = RPC NDR_KEYSVC
NOPROTO = YES

################################################
# Start SUBSYSTEM RPC
[SUBSYSTEM::RPC]
REQUIRED_SUBSYSTEMS = NDR_RAW RPC_RAW LIBSMB NDR_MISC NDR_DCERPC NDR_SCHANNEL NDR_LSA NDR_NETLOGON NDR_SAMR NDR_UNIXINFO RPC_NDR_NETLOGON RPC_NDR_EPMAPPER
# End SUBSYSTEM RPC
################################################

[SUBSYSTEM::RPC_EJS_ECHO]
INIT_FUNCTION = ejs_init_rpcecho
OBJ_FILES = gen_ndr/ndr_echo_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_ECHO
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_MISC]
INIT_FUNCTION = ejs_init_misc
OBJ_FILES = gen_ndr/ndr_misc_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_MISC
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_SAMR]
INIT_FUNCTION = ejs_init_samr
OBJ_FILES = gen_ndr/ndr_samr_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_SAMR
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_SECURITY]
INIT_FUNCTION = ejs_init_security
OBJ_FILES = gen_ndr/ndr_security_ejs.o
REQUIRED_SUBSYSTEMS = RPC LIB_SECURITY_NDR
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_LSA]
INIT_FUNCTION = ejs_init_lsarpc
OBJ_FILES = gen_ndr/ndr_lsa_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_LSA
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_DFS]
INIT_FUNCTION = ejs_init_dfs
OBJ_FILES = gen_ndr/ndr_dfs_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_DFS
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_DRSUAPI]
INIT_FUNCTION = ejs_init_drsuapi
OBJ_FILES = gen_ndr/ndr_drsuapi_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_DRSUAPI
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_SPOOLSS]
INIT_FUNCTION = ejs_init_spoolss
OBJ_FILES = gen_ndr/ndr_spoolss_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_SPOOLSS
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_WKSSVC]
INIT_FUNCTION = ejs_init_wkssvc
OBJ_FILES = gen_ndr/ndr_wkssvc_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_WKSSVC
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_SRVSVC]
INIT_FUNCTION = ejs_init_srvsvc
OBJ_FILES = gen_ndr/ndr_srvsvc_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_SRVSVC
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_EVENTLOG]
INIT_FUNCTION = ejs_init_eventlog
OBJ_FILES = gen_ndr/ndr_eventlog_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_EVENTLOG
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_WINREG]
INIT_FUNCTION = ejs_init_winreg
OBJ_FILES = gen_ndr/ndr_winreg_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_WINREG
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_INITSHUTDOWN]
INIT_FUNCTION = ejs_init_initshutdown
OBJ_FILES = gen_ndr/ndr_initshutdown_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_INITSHUTDOWN
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_NETLOGON]
INIT_FUNCTION = ejs_init_netlogon
OBJ_FILES = gen_ndr/ndr_netlogon_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_NETLOGON
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_SVCCTL]
INIT_FUNCTION = ejs_init_svcctl
OBJ_FILES = gen_ndr/ndr_svcctl_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_SVCCTL
NOPROTO = YES

[SUBSYSTEM::RPC_EJS_IRPC]
INIT_FUNCTION = ejs_init_irpc
OBJ_FILES = gen_ndr/ndr_irpc_ejs.o
REQUIRED_SUBSYSTEMS = RPC NDR_IRPC
NOPROTO = YES

################################################
# Start SUBSYSTEM RPC_EJS
[SUBSYSTEM::RPC_EJS]
REQUIRED_SUBSYSTEMS = RPC_EJS_ECHO RPC_EJS_MISC RPC_EJS_SAMR RPC_EJS_SECURITY \
	RPC_EJS_LSA RPC_EJS_SRVSVC RPC_EJS_SVCCTL RPC_EJS_IRPC RPC_EJS_WINREG \
	RPC_EJS_INITSHUTDOWN
# End SUBSYSTEM RPC_EJS
################################################
