# auth server subsystem
include gensec/config.mk
include kerberos/config.mk
include ntlmssp/config.mk
include credentials/config.mk

#######################
# Start MODULE auth_sam
[MODULE::auth_sam]
# gensec_krb5 and gensec_gssapi depend on it
OUTPUT_TYPE = MERGEDOBJ
PRIVATE_PROTO_HEADER = auth_sam.h
INIT_FUNCTION = auth_sam_init
SUBSYSTEM = auth
OBJ_FILES = \
		auth_sam.o \
		auth_sam_reply.o
PUBLIC_DEPENDENCIES = \
		SAMDB
# End MODULE auth_sam
#######################

#######################
# Start MODULE auth_anonymous
[MODULE::auth_anonymous]
INIT_FUNCTION = auth_anonymous_init
SUBSYSTEM = auth
OBJ_FILES = auth_anonymous.o
# End MODULE auth_anonymous
#######################

#######################
# Start MODULE auth_winbind
[MODULE::auth_winbind]
INIT_FUNCTION = auth_winbind_init
SUBSYSTEM = auth
OBJ_FILES = auth_winbind.o
PUBLIC_DEPENDENCIES = \
		LIBWINBIND-CLIENT \
		NDR_NETLOGON LIBNDR
# End MODULE auth_winbind
#######################

#######################
# Start MODULE auth_developer
[MODULE::auth_developer]
INIT_FUNCTION = auth_developer_init
SUBSYSTEM = auth
OBJ_FILES = auth_developer.o
# End MODULE auth_developer
#######################

#######################
# Start MODULE auth_unix
[MODULE::auth_unix]
INIT_FUNCTION = auth_unix_init
SUBSYSTEM = auth
OBJ_FILES = auth_unix.o
PUBLIC_DEPENDENCIES = CRYPT PAM PAM_ERRORS
# End MODULE auth_unix
#######################

[SUBSYSTEM::PAM_ERRORS]
PRIVATE_PROTO_HEADER = pam_errors.h
OBJ_FILES = pam_errors.o

#######################
# Start SUBSYSTEM auth
[SUBSYSTEM::auth]
PUBLIC_HEADERS = auth.h
PUBLIC_PROTO_HEADER = auth_proto.h
OBJ_FILES = \
		auth.o \
		auth_util.o \
		ntlm_check.o \
		auth_simple.o
PUBLIC_DEPENDENCIES = LIBSECURITY process_model SAMDB
# End SUBSYSTEM auth
#######################
