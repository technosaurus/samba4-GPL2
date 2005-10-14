# WREPL server subsystem

#######################
# Start SUBSYSTEM WREPL_SRV
[SUBSYSTEM::WREPL_SRV]
INIT_OBJ_FILES = \
		wrepl_server/wrepl_server.o \
		wrepl_server/wrepl_in_connection.o \
		wrepl_server/wrepl_in_call.o \
		wrepl_server/wrepl_out_connection.o \
		wrepl_server/wrepl_out_helpers.o \
		wrepl_server/wrepl_apply_records.o
REQUIRED_SUBSYSTEMS = \
		LIBCLI_WREPL WINSDB
# End SUBSYSTEM WREPL_SRV
#######################
