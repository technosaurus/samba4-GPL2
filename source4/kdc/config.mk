# KDC server subsystem

#######################
# Start SUBSYSTEM KDC
[SUBSYSTEM::KDC]
INIT_OBJ_FILES = \
		kdc/kdc.o \
		kdc/hdb-ldb.o
REQUIRED_SUBSYSTEMS = \
		SOCKET
# End SUBSYSTEM KDC
#######################
