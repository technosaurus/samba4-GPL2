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
       heimdal/lib/hdb/hdb_asn1.h \
       heimdal/lib/gssapi/spnego_asn1.h \
       heimdal/lib/gssapi/gssapi_asn1.h \
       heimdal/lib/asn1/krb5_asn1.h \
       heimdal/lib/asn1/asn1_err.h \
       heimdal/lib/asn1/digest_asn1.h \
       heimdal/lib/asn1/pkcs8_asn1.h \
       heimdal/lib/asn1/pkcs9_asn1.h \
       heimdal/lib/asn1/pkcs12_asn1.h \
       heimdal/lib/asn1/cms_asn1.h \
       heimdal/lib/asn1/rfc2459_asn1.h \
       heimdal/lib/asn1/pkinit_asn1.h \
       heimdal/lib/asn1/kx509_asn1.h \
       heimdal/lib/hx509/ocsp_asn1.h \
       heimdal/lib/hx509/pkcs10_asn1.h \
       heimdal/lib/hdb/hdb_err.h \
       heimdal/lib/krb5/heim_err.h \
       heimdal/lib/krb5/k524_err.h \
       heimdal/lib/krb5/krb5_err.h \
       heimdal/lib/gssapi/gkrb5_err.h \
       heimdal/lib/hx509/hx509_err.h \
       heimdal/lib/des/hcrypto

proto: basics
basics: include/includes.h \
	idl \
	$(PROTO_HEADERS) \
	heimdal_basics
