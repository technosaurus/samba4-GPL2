# server subsystem

################################################
# Start MODULE server_service_auth
[MODULE::server_service_auth]
REQUIRED_SUBSYSTEMS = \
		AUTH
# End MODULE server_auth
################################################

################################################
# Start MODULE server_service_smb
[MODULE::server_service_smb]
REQUIRED_SUBSYSTEMS = \
		SMB
# End MODULE server_smb
################################################

################################################
# Start MODULE server_service_rpc
[MODULE::server_service_rpc]
REQUIRED_SUBSYSTEMS = \
		DCERPC
# End MODULE server_rpc
################################################

################################################
# Start MODULE server_service_ldap
[MODULE::server_service_ldap]
REQUIRED_SUBSYSTEMS = \
		LDAP
# End MODULE server_ldap
################################################

#######################
# Start SUBSYSTEM SERVICE
[SUBSYSTEM::SERVER_SERVICE]
INIT_OBJ_FILES = \
		smbd/service.o
# End SUBSYSTEM SERVER
#######################

#######################
# Start SUBSYSTEM SERVER
[SUBSYSTEM::SERVER]
INIT_OBJ_FILES = \
		smbd/server.o
ADD_OBJ_FILES = \
		smbd/rewrite.o
REQUIRED_SUBSYSTEMS = \
		PROCESS_MODEL \
		SERVER_SERVICE
# End SUBSYSTEM SERVER
#######################

#################################
# Start BINARY smbd
[BINARY::smbd]
REQUIRED_SUBSYSTEMS = \
		SERVER \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC
# End BINARY smbd
#################################
