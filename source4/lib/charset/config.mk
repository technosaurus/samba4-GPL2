################################################
# Start SUBSYSTEM CHARSET
[SUBSYSTEM::CHARSET]
OBJ_FILES = \
		iconv.o \
		charcnv.o
PUBLIC_HEADERS = charset.h
PUBLIC_PROTO_HEADER = charset_proto.h
PUBLIC_DEPENDENCIES = ICONV
# End SUBSYSTEM CHARSET
################################################
