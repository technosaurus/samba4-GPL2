#################################
# Start SUBSYSTEM KERBEROS
[SUBSYSTEM::KERBEROS]
INIT_OBJ_FILES = auth/kerberos/kerberos.o 
ADD_OBJ_FILES = \
		auth/kerberos/clikrb5.o \
		auth/kerberos/kerberos_verify.o \
		auth/kerberos/kerberos_util.o \
		auth/kerberos/gssapi_parse.o
# End SUBSYSTEM KERBEROS
#################################
