# LDAP server subsystem

#######################
# Start SUBSYSTEM LDAP
[SUBSYSTEM::LDAP]
INIT_OBJ_FILES = \
		ldap_server.o \
		ldap_backend.o \
		ldap_bind.o \
		ldap_simple_ldb.o 
REQUIRED_SUBSYSTEMS = \
		LIBCLI_LDAP SAMDB
# End SUBSYSTEM SMB
#######################
