dnl # LIBRPC subsystem

SMB_SUBSYSTEM(LIBNDR_RAW,[],
		[librpc/ndr/ndr.o
		librpc/ndr/ndr_basic.o
		librpc/ndr/ndr_sec.o
		librpc/ndr/ndr_spoolss_buf.o
		librpc/gen_ndr/tables.o
		librpc/gen_ndr/ndr_dcerpc.o
		librpc/gen_ndr/ndr_echo.o
		librpc/gen_ndr/ndr_misc.o
		librpc/gen_ndr/ndr_lsa.o
		librpc/gen_ndr/ndr_lsads.o
		librpc/gen_ndr/ndr_dfs.o
		librpc/gen_ndr/ndr_drsuapi.o
		librpc/gen_ndr/ndr_samr.o
		librpc/gen_ndr/ndr_spoolss.o
		librpc/gen_ndr/ndr_wkssvc.o
		librpc/gen_ndr/ndr_srvsvc.o
		librpc/gen_ndr/ndr_svcctl.o
		librpc/gen_ndr/ndr_atsvc.o
		librpc/gen_ndr/ndr_eventlog.o
		librpc/gen_ndr/ndr_epmapper.o
		librpc/gen_ndr/ndr_winreg.o
		librpc/gen_ndr/ndr_mgmt.o
		librpc/gen_ndr/ndr_protected_storage.o
		librpc/gen_ndr/ndr_dcom.o
		librpc/gen_ndr/ndr_wzcsvc.o
		librpc/gen_ndr/ndr_browser.o
		librpc/gen_ndr/ndr_w32time.o
		librpc/gen_ndr/ndr_scerpc.o
		librpc/gen_ndr/ndr_ntsvcs.o
		librpc/gen_ndr/ndr_netlogon.o
		librpc/gen_ndr/ndr_trkwks.o
		librpc/gen_ndr/ndr_keysvc.o
		librpc/gen_ndr/ndr_krb5pac.o
		librpc/gen_ndr/ndr_schannel.o])

SMB_SUBSYSTEM(LIBRPC_RAW,[],
		[librpc/rpc/dcerpc.o
		librpc/rpc/dcerpc_auth.o
		librpc/rpc/dcerpc_util.o
		librpc/rpc/dcerpc_schannel.o
		librpc/rpc/dcerpc_ntlm.o
		librpc/rpc/dcerpc_spnego.o
		librpc/rpc/dcerpc_smb.o
		librpc/rpc/dcerpc_tcp.o])

SMB_SUBSYSTEM(LIBRPC,[],[],[],
		[LIBNDR_RAW LIBRPC_RAW])
