# server subsystem

################################################
# Start MODULE service_auth
[MODULE::service_auth]
INIT_FUNCTION = server_service_auth_init
SUBSYSTEM = service
PUBLIC_DEPENDENCIES = \
		auth
# End MODULE server_auth
################################################

#######################
# Start SUBSERVICE
[SUBSYSTEM::service]
PRIVATE_PROTO_HEADER = service_proto.h
OBJ_FILES = \
		service.o \
		service_stream.o \
		service_task.o
PUBLIC_DEPENDENCIES = \
		MESSAGING
# End SUBSYSTEM SERVER
#######################

#################################
# Start BINARY smbd
[BINARY::smbd]
INSTALLDIR = SBINDIR
MANPAGE = smbd.8
OBJ_FILES = \
		server.o
PRIVATE_DEPENDENCIES = \
		process_model \
		service \
		LIBSAMBA-CONFIG \
		LIBSAMBA-UTIL \
		PIDFILE \
		POPT_SAMBA \
		POPT_EXT \
		gensec \
		registry \
		ntptr \
		ntvfs \
		share
# End BINARY smbd
#################################
