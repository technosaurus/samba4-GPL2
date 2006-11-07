# These should also be autogenerated at some point
# perhaps by some perl scripts run from config.status ?
#
librpc/gen_ndr/misc.h: idl
librpc/ndr/libndr.h: librpc/ndr/libndr_proto.h librpc/gen_ndr/misc.h
librpc/rpc/dcerpc.h: librpc/rpc/dcerpc_proto.h
auth/credentials/credentials.h: auth/credentials/credentials_proto.h
libcli/nbt/libnbt.h: libcli/nbt/nbt_proto.h
lib/charset/charset.h: lib/charset/charset_proto.h

include/includes.h: \
		include/config.h \
		lib/util/util_proto.h \
		lib/charset/charset.h \
		param/proto.h \
		libcli/util/proto.h \
		librpc/gen_ndr/misc.h

heimdal_basics: \
       heimdal/lib/roken/vis.h \
       heimdal/lib/roken/err.h \
       heimdal/lib/hdb/hdb_asn1.h \
       heimdal/lib/gssapi/spnego/spnego_asn1.h \
       heimdal/lib/gssapi/mech/gssapi_asn1.h \
       heimdal/lib/asn1/krb5_asn1.h \
       heimdal/lib/asn1/asn1_err.h \
       heimdal/lib/asn1/digest_asn1.h \
       heimdal/lib/hdb/hdb_err.h \
       heimdal/lib/krb5/heim_err.h \
       heimdal/lib/krb5/k524_err.h \
       heimdal/lib/krb5/krb5_err.h \
       heimdal/lib/des/hcrypto

proto: basics
basics: include/includes.h \
	idl \
	$(PROTO_HEADERS) \
	heimdal_basics
