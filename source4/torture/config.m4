dnl # TORTURE subsystem

SMB_SUBSYSTEM_MK(TORTURE_BASIC,torture/config.mk)

SMB_SUBSYSTEM_MK(TORTURE_RAW,torture/config.mk)

SMB_SUBSYSTEM_MK(TORTURE_RPC,torture/config.mk)

SMB_SUBSYSTEM_MK(TORTURE_NBENCH,torture/config.mk)

SMB_BINARY_MK(smbtorture,torture/config.mk)
