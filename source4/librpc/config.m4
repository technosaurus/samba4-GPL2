dnl # LIBRPC subsystem

SMB_SUBSYSTEM_MK(LIBNDR_RAW,librpc/config.mk)

SMB_SUBSYSTEM_NOPROTO(LIBNDR_GEN)
SMB_MODULE_MK(ndr_echo, LIBNDR_GEN_ECHO, STATIC, librpc/config.m4)
SMB_SUBSYSTEM(LIBNDR_GEN,[],
		[librpc/gen_ndr/tables.o
		librpc/gen_ndr/ndr_audiosrv.o
		librpc/gen_ndr/ndr_dcerpc.o
		librpc/gen_ndr/ndr_echo.o
		librpc/gen_ndr/ndr_exchange.o
		librpc/gen_ndr/ndr_dsbackup.o
		librpc/gen_ndr/ndr_efs.o
		librpc/gen_ndr/ndr_misc.o
		librpc/gen_ndr/ndr_lsa.o
		librpc/gen_ndr/ndr_lsads.o
		librpc/gen_ndr/ndr_dfs.o
		librpc/gen_ndr/ndr_drsuapi.o
		librpc/gen_ndr/ndr_policyagent.o
		librpc/gen_ndr/ndr_samr.o
		librpc/gen_ndr/ndr_spoolss.o
		librpc/gen_ndr/ndr_wkssvc.o
		librpc/gen_ndr/ndr_srvsvc.o
		librpc/gen_ndr/ndr_svcctl.o
		librpc/gen_ndr/ndr_atsvc.o
		librpc/gen_ndr/ndr_eventlog.o
		librpc/gen_ndr/ndr_epmapper.o
		librpc/gen_ndr/ndr_dbgidl.o
		librpc/gen_ndr/ndr_dssetup.o
		librpc/gen_ndr/ndr_msgsvc.o
		librpc/gen_ndr/ndr_wins.o
		librpc/gen_ndr/ndr_winreg.o
		librpc/gen_ndr/ndr_mgmt.o
		librpc/gen_ndr/ndr_protected_storage.o
		librpc/gen_ndr/ndr_dcom.o
		librpc/gen_ndr/ndr_oxidresolver.o
		librpc/gen_ndr/ndr_remact.o
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

SMB_SUBSYSTEM_MK(LIBRPC_RAW,librpc/config.mk)
SMB_SUBSYSTEM_MK(LIBRPC,librpc/config.mk)
