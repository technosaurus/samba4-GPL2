#######################
# Start SUBSYSTEM HEIMDAL_KDC
[SUBSYSTEM::HEIMDAL_KDC]
ADD_OBJ_FILES = \
	heimdal/kdc/default_config.o \
	heimdal/kdc/kerberos5.o \
	heimdal/kdc/pkinit.o \
	heimdal/kdc/log.o \
	heimdal/kdc/misc.o \
	heimdal/kdc/524.o \
	heimdal/kdc/kerberos4.o \
	heimdal/kdc/kaserver.o \
	heimdal/kdc/process.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_KDC
#######################

#######################
# Start SUBSYSTEM HEIMDAL_HDB
[SUBSYSTEM::HEIMDAL_HDB]
ADD_OBJ_FILES = \
	heimdal/lib/hdb/db.o \
	heimdal/lib/hdb/hdb.o \
	heimdal/lib/hdb/keys.o \
	heimdal/lib/hdb/ndbm.o \
	heimdal/lib/hdb/asn1_Event.o \
	heimdal/lib/hdb/asn1_GENERATION.o \
	heimdal/lib/hdb/asn1_HDBFlags.o \
	heimdal/lib/hdb/asn1_Key.o \
	heimdal/lib/hdb/asn1_Salt.o \
	heimdal/lib/hdb/asn1_hdb_entry.o \
	heimdal/lib/hdb/hdb_err.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_HDB
#######################

#######################
# Start SUBSYSTEM HEIMDAL_GSSAPI
[SUBSYSTEM::HEIMDAL_GSSAPI]
ADD_OBJ_FILES = \
	heimdal/lib/gssapi/init_sec_context.o \
	heimdal/lib/gssapi/inquire_cred.o \
	heimdal/lib/gssapi/release_buffer.o \
	heimdal/lib/gssapi/release_cred.o \
	heimdal/lib/gssapi/release_name.o \
	heimdal/lib/gssapi/release_oid_set.o \
	heimdal/lib/gssapi/sequence.o \
	heimdal/lib/gssapi/test_oid_set_member.o \
	heimdal/lib/gssapi/unwrap.o \
	heimdal/lib/gssapi/verify_mic.o \
	heimdal/lib/gssapi/wrap.o \
	heimdal/lib/gssapi/address_to_krb5addr.o \
	heimdal/lib/gssapi/asn1_ContextFlags.o \
	heimdal/lib/gssapi/asn1_MechType.o \
	heimdal/lib/gssapi/asn1_MechTypeList.o \
	heimdal/lib/gssapi/asn1_NegTokenInit.o \
	heimdal/lib/gssapi/asn1_NegTokenTarg.o \
	heimdal/lib/gssapi/8003.o \
	heimdal/lib/gssapi/accept_sec_context.o \
	heimdal/lib/gssapi/acquire_cred.o \
	heimdal/lib/gssapi/add_oid_set_member.o \
	heimdal/lib/gssapi/arcfour.o \
	heimdal/lib/gssapi/ccache_name.o \
	heimdal/lib/gssapi/copy_ccache.o \
	heimdal/lib/gssapi/cfx.o \
	heimdal/lib/gssapi/compat.o \
	heimdal/lib/gssapi/context_time.o \
	heimdal/lib/gssapi/create_emtpy_oid_set.o \
	heimdal/lib/gssapi/decapsulate.o \
	heimdal/lib/gssapi/delete_sec_context.o \
	heimdal/lib/gssapi/display_name.o \
	heimdal/lib/gssapi/display_status.o \
	heimdal/lib/gssapi/duplicate_name.o \
	heimdal/lib/gssapi/encapsulate.o \
	heimdal/lib/gssapi/external.o \
	heimdal/lib/gssapi/get_mic.o \
	heimdal/lib/gssapi/import_name.o \
	heimdal/lib/gssapi/init.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_GSSAPI
#######################

#######################
# Start SUBSYSTEM HEIMDAL_KRB5
[SUBSYSTEM::HEIMDAL_KRB5]
ADD_OBJ_FILES = \
	heimdal/lib/krb5/acache.o \
	heimdal/lib/krb5/add_et_list.o \
	heimdal/lib/krb5/addr_families.o \
	heimdal/lib/krb5/appdefault.o \
	heimdal/lib/krb5/asn1_glue.o \
	heimdal/lib/krb5/auth_context.o \
	heimdal/lib/krb5/build_ap_req.o \
	heimdal/lib/krb5/build_auth.o \
	heimdal/lib/krb5/cache.o \
	heimdal/lib/krb5/changepw.o \
	heimdal/lib/krb5/codec.o \
	heimdal/lib/krb5/config_file.o \
	heimdal/lib/krb5/config_file_netinfo.o \
	heimdal/lib/krb5/constants.o \
	heimdal/lib/krb5/context.o \
	heimdal/lib/krb5/copy_host_realm.o \
	heimdal/lib/krb5/crc.o \
	heimdal/lib/krb5/creds.o \
	heimdal/lib/krb5/crypto.o \
	heimdal/lib/krb5/data.o \
	heimdal/lib/krb5/eai_to_heim_errno.o \
	heimdal/lib/krb5/error_string.o \
	heimdal/lib/krb5/expand_hostname.o \
	heimdal/lib/krb5/fcache.o \
	heimdal/lib/krb5/free.o \
	heimdal/lib/krb5/free_host_realm.o \
	heimdal/lib/krb5/generate_seq_number.o \
	heimdal/lib/krb5/generate_subkey.o \
	heimdal/lib/krb5/get_cred.o \
	heimdal/lib/krb5/get_default_principal.o \
	heimdal/lib/krb5/get_default_realm.o \
	heimdal/lib/krb5/get_for_creds.o \
	heimdal/lib/krb5/get_host_realm.o \
	heimdal/lib/krb5/get_in_tkt.o \
	heimdal/lib/krb5/get_in_tkt_with_keytab.o \
	heimdal/lib/krb5/get_port.o \
	heimdal/lib/krb5/init_creds.o \
	heimdal/lib/krb5/init_creds_pw.o \
	heimdal/lib/krb5/kcm.o \
	heimdal/lib/krb5/keyblock.o \
	heimdal/lib/krb5/keytab.o \
	heimdal/lib/krb5/keytab_any.o \
	heimdal/lib/krb5/keytab_file.o \
	heimdal/lib/krb5/keytab_memory.o \
	heimdal/lib/krb5/keytab_keyfile.o \
	heimdal/lib/krb5/keytab_krb4.o \
	heimdal/lib/krb5/krbhst.o \
	heimdal/lib/krb5/log.o \
	heimdal/lib/krb5/mcache.o \
	heimdal/lib/krb5/misc.o \
	heimdal/lib/krb5/mk_error.o \
	heimdal/lib/krb5/mk_priv.o \
	heimdal/lib/krb5/mk_rep.o \
	heimdal/lib/krb5/mk_req.o \
	heimdal/lib/krb5/mk_req_ext.o \
	heimdal/lib/krb5/mit_glue.o \
	heimdal/lib/krb5/n-fold.o \
	heimdal/lib/krb5/padata.o \
	heimdal/lib/krb5/pkinit.o \
	heimdal/lib/krb5/principal.o \
	heimdal/lib/krb5/rd_cred.o \
	heimdal/lib/krb5/rd_error.o \
	heimdal/lib/krb5/rd_priv.o \
	heimdal/lib/krb5/rd_rep.o \
	heimdal/lib/krb5/rd_req.o \
	heimdal/lib/krb5/replay.o \
	heimdal/lib/krb5/send_to_kdc.o \
	heimdal/lib/krb5/set_default_realm.o \
	heimdal/lib/krb5/store.o \
	heimdal/lib/krb5/store_emem.o \
	heimdal/lib/krb5/store_fd.o \
	heimdal/lib/krb5/store_mem.o \
	heimdal/lib/krb5/ticket.o \
	heimdal/lib/krb5/time.o \
	heimdal/lib/krb5/transited.o \
	heimdal/lib/krb5/v4_glue.o \
	heimdal/lib/krb5/version.o \
	heimdal/lib/krb5/warn.o \
	heimdal/lib/krb5/krb5_err.o \
	heimdal/lib/krb5/heim_err.o \
	heimdal/lib/krb5/k524_err.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_KRB5
#######################

#######################
# Start SUBSYSTEM HEIMDAL_ASN1
[SUBSYSTEM::HEIMDAL_ASN1]
ADD_OBJ_FILES = \
	heimdal/lib/asn1/der_get.o \
	heimdal/lib/asn1/der_put.o \
	heimdal/lib/asn1/der_free.o \
	heimdal/lib/asn1/der_length.o \
	heimdal/lib/asn1/der_copy.o \
	heimdal/lib/asn1/der_cmp.o \
	heimdal/lib/asn1/asn1_AD_IF_RELEVANT.o \
	heimdal/lib/asn1/asn1_APOptions.o \
	heimdal/lib/asn1/asn1_AP_REP.o \
	heimdal/lib/asn1/asn1_AP_REQ.o \
	heimdal/lib/asn1/asn1_AS_REP.o \
	heimdal/lib/asn1/asn1_AS_REQ.o \
	heimdal/lib/asn1/asn1_Authenticator.o \
	heimdal/lib/asn1/asn1_AuthorizationData.o \
	heimdal/lib/asn1/asn1_CBCParameter.o \
	heimdal/lib/asn1/asn1_CKSUMTYPE.o \
	heimdal/lib/asn1/asn1_ChangePasswdDataMS.o \
	heimdal/lib/asn1/asn1_Checksum.o \
	heimdal/lib/asn1/asn1_ENCTYPE.o \
	heimdal/lib/asn1/asn1_ETYPE_INFO.o \
	heimdal/lib/asn1/asn1_ETYPE_INFO2.o \
	heimdal/lib/asn1/asn1_ETYPE_INFO2_ENTRY.o \
	heimdal/lib/asn1/asn1_ETYPE_INFO_ENTRY.o \
	heimdal/lib/asn1/asn1_EncAPRepPart.o \
	heimdal/lib/asn1/asn1_EncASRepPart.o \
	heimdal/lib/asn1/asn1_EncKDCRepPart.o \
	heimdal/lib/asn1/asn1_EncKrbCredPart.o \
	heimdal/lib/asn1/asn1_EncKrbPrivPart.o \
	heimdal/lib/asn1/asn1_EncTGSRepPart.o \
	heimdal/lib/asn1/asn1_EncTicketPart.o \
	heimdal/lib/asn1/asn1_EncryptedData.o \
	heimdal/lib/asn1/asn1_EncryptionKey.o \
	heimdal/lib/asn1/asn1_EtypeList.o \
	heimdal/lib/asn1/asn1_HostAddress.o \
	heimdal/lib/asn1/asn1_HostAddresses.o \
	heimdal/lib/asn1/asn1_KDCOptions.o \
	heimdal/lib/asn1/asn1_KDC_REP.o \
	heimdal/lib/asn1/asn1_KDC_REQ.o \
	heimdal/lib/asn1/asn1_KDC_REQ_BODY.o \
	heimdal/lib/asn1/asn1_KRB_CRED.o \
	heimdal/lib/asn1/asn1_KRB_ERROR.o \
	heimdal/lib/asn1/asn1_KRB_PRIV.o \
	heimdal/lib/asn1/asn1_KerberosString.o \
	heimdal/lib/asn1/asn1_KerberosTime.o \
	heimdal/lib/asn1/asn1_KrbCredInfo.o \
	heimdal/lib/asn1/asn1_LR_TYPE.o \
	heimdal/lib/asn1/asn1_LastReq.o \
	heimdal/lib/asn1/asn1_MESSAGE_TYPE.o \
	heimdal/lib/asn1/asn1_METHOD_DATA.o \
	heimdal/lib/asn1/asn1_NAME_TYPE.o \
	heimdal/lib/asn1/asn1_PADATA_TYPE.o \
	heimdal/lib/asn1/asn1_PA_DATA.o \
	heimdal/lib/asn1/asn1_PA_ENC_TS_ENC.o \
	heimdal/lib/asn1/asn1_PA_PAC_REQUEST.o \
	heimdal/lib/asn1/asn1_Principal.o \
	heimdal/lib/asn1/asn1_PrincipalName.o \
	heimdal/lib/asn1/asn1_RC2CBCParameter.o \
	heimdal/lib/asn1/asn1_Realm.o \
	heimdal/lib/asn1/asn1_TGS_REP.o \
	heimdal/lib/asn1/asn1_TGS_REQ.o \
	heimdal/lib/asn1/asn1_Ticket.o \
	heimdal/lib/asn1/asn1_TicketFlags.o \
	heimdal/lib/asn1/asn1_TransitedEncoding.o \
	heimdal/lib/asn1/asn1_UNSIGNED.o \
	heimdal/lib/asn1/asn1_err.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_KRB5
#######################

#######################
# Start SUBSYSTEM HEIMDAL_DES
[SUBSYSTEM::HEIMDAL_DES]
ADD_OBJ_FILES = \
	heimdal/lib/des/aes.o \
	heimdal/lib/des/des.o \
	heimdal/lib/des/md4.o \
	heimdal/lib/des/md5.o \
	heimdal/lib/des/rc2.o \
	heimdal/lib/des/rc4.o \
	heimdal/lib/des/rijndael-alg-fst.o \
	heimdal/lib/des/rnd_keys.o \
	heimdal/lib/des/sha.o \
	heimdal/lib/des/ui.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_DES
#######################

#######################
# Start SUBSYSTEM HEIMDAL_ROKEN_ADDRINFO
[SUBSYSTEM::HEIMDAL_ROKEN_ADDRINFO]
ADD_OBJ_FILES = \
	heimdal/lib/roken/gai_strerror.o \
	heimdal/lib/roken/getaddrinfo.o \
	heimdal/lib/roken/freeaddrinfo.o \
	heimdal/lib/roken/getipnodebyaddr.o \
	heimdal/lib/roken/getipnodebyname.o \
	heimdal/lib/roken/freehostent.o \
	heimdal/lib/roken/copyhostent.o \
	heimdal/lib/roken/hostent_find_fqdn.o
NOPROTO = YES

#######################
# Start SUBSYSTEM HEIMDAL_ROKEN
[SUBSYSTEM::HEIMDAL_ROKEN]
ADD_OBJ_FILES = \
	heimdal/lib/roken/base64.o \
	heimdal/lib/roken/bswap.o \
	heimdal/lib/roken/get_window_size.o \
	heimdal/lib/roken/getprogname.o \
	heimdal/lib/roken/h_errno.o \
	heimdal/lib/roken/issuid.o \
	heimdal/lib/roken/net_read.o \
	heimdal/lib/roken/net_write.o \
	heimdal/lib/roken/parse_time.o \
	heimdal/lib/roken/parse_units.o \
	heimdal/lib/roken/resolve.o \
	heimdal/lib/roken/roken_gethostby.o \
	heimdal/lib/roken/signal.o \
	heimdal/lib/roken/vis.o \
	heimdal/lib/roken/strlwr.o \
	heimdal/lib/roken/strsep_copy.o \
	heimdal/lib/roken/strupr.o \
	heimdal/lib/roken/strpool.o \
	heimdal_build/replace.o
REQUIRED_SUBSYSTEMS = HEIMDAL_ROKEN_ADDRINFO EXT_LIB_SOCKET
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_ROKEN
#######################

#######################
# Start SUBSYSTEM HEIMDAL_GLUE
[SUBSYSTEM::HEIMDAL_GLUE]
ADD_OBJ_FILES = heimdal_build/glue.o
# End SUBSYSTEM HEIMDAL_GLUE
#######################

#######################
# Start SUBSYSTEM HEIMDAL_COM_ERR
[SUBSYSTEM::HEIMDAL_COM_ERR]
ADD_OBJ_FILES = \
	heimdal/lib/com_err/com_err.o \
	heimdal/lib/com_err/error.o
NOPROTO = YES
# End SUBSYSTEM HEIMDAL_COM_ERR
#######################

#######################
# Start SUBSYSTEM ASN1_COMPILER
[SUBSYSTEM::ASN1_COMPILER]
ADD_OBJ_FILES = \
	heimdal/lib/asn1/main.o \
	heimdal/lib/asn1/gen.o \
	heimdal/lib/asn1/gen_copy.o \
	heimdal/lib/asn1/gen_decode.o \
	heimdal/lib/asn1/gen_encode.o \
	heimdal/lib/asn1/gen_free.o \
	heimdal/lib/asn1/gen_glue.o \
	heimdal/lib/asn1/gen_length.o \
	heimdal/lib/asn1/hash.o \
	heimdal/lib/asn1/lex.o \
	heimdal/lib/asn1/parse.o \
	heimdal/lib/roken/emalloc.o \
	heimdal/lib/roken/getarg.o \
	heimdal/lib/roken/print_version.o \
	heimdal/lib/roken/setprogname.o \
	heimdal/lib/roken/strupr.o \
	heimdal/lib/roken/getprogname.o \
	heimdal/lib/roken/get_window_size.o \
	heimdal/lib/asn1/symbol.o \
	heimdal_build/replace.o
NOPROTO = YES
# End SUBSYSTEM ASN1_COMPILER
#######################

#######################
# Start BINARY asn1_compile
[BINARY::asn1_compile]
REQUIRED_SUBSYSTEMS = ASN1_COMPILER LIBREPLACE
# End BINARY asn1_compile
#######################

#######################
# Start SUBSYSTEM COMPILE_ET
[SUBSYSTEM::COMPILE_ET]
ADD_OBJ_FILES = \
	heimdal/lib/com_err/lex.o \
	heimdal/lib/com_err/parse.o \
	heimdal/lib/com_err/compile_et.o \
	heimdal/lib/roken/getarg.o \
	heimdal/lib/roken/get_window_size.o \
	heimdal/lib/roken/getprogname.o \
	heimdal/lib/roken/strupr.o \
	heimdal/lib/roken/print_version.o \
	heimdal/lib/roken/setprogname.o \
	heimdal_build/replace.o
NOPROTO = YES
# End SUBSYSTEM COMPILE_ET
#######################

#######################
# Start BINARY compile_et
[BINARY::compile_et]
REQUIRED_SUBSYSTEMS = COMPILE_ET LIBREPLACE
# End BINARY compile_et
#######################

heimdal/lib/roken/vis.h: heimdal/lib/roken/vis.hin
	@cp heimdal/lib/roken/vis.hin heimdal/lib/roken/vis.h

heimdal/lib/roken/err.h: heimdal/lib/roken/err.hin
	@cp heimdal/lib/roken/err.hin heimdal/lib/roken/err.h

include ./heimdal_build/asn1_deps.pl heimdal/lib/hdb/hdb.asn1 hdb_asn1|
include ./heimdal_build/asn1_deps.pl heimdal/lib/gssapi/spnego.asn1 spnego_asn1|
include ./heimdal_build/asn1_deps.pl heimdal/lib/asn1/k5.asn1 krb5_asn1|

include ./heimdal_build/et_deps.pl heimdal/lib/asn1/asn1_err.et|
include ./heimdal_build/et_deps.pl heimdal/lib/hdb/hdb_err.et|
include ./heimdal_build/et_deps.pl heimdal/lib/krb5/heim_err.et|
include ./heimdal_build/et_deps.pl heimdal/lib/krb5/k524_err.et|
include ./heimdal_build/et_deps.pl heimdal/lib/krb5/krb5_err.et|

heimdal_basics: \
	heimdal/lib/roken/vis.h \
	heimdal/lib/roken/err.h \
	heimdal/lib/hdb/hdb_asn1.h \
	heimdal/lib/gssapi/spnego_asn1.h \
	heimdal/lib/asn1/krb5_asn1.h \
	heimdal/lib/asn1/asn1_err.h \
	heimdal/lib/hdb/hdb_err.h \
	heimdal/lib/krb5/heim_err.h \
	heimdal/lib/krb5/k524_err.h \
	heimdal/lib/krb5/krb5_err.h

heimdal_clean:
	@echo Removing generated ASN1 files
	@-find heimdal/lib/asn1 -name 'asn1_*.[cx]' -exec rm -f '{}' \;
	@-find heimdal/lib/gssapi -name 'asn1_*.[c]' -exec rm -f '{}' \;
	@-find heimdal/lib/hdb -name 'asn1_*.[c]' -exec rm -f '{}' \;
	@-rm -f heimdal/lib/roken/vis.h heimdal/lib/roken/err.h
	@-rm -f heimdal/lib/hdb/hdb_asn1.h
	@-rm -f heimdal/lib/gssapi/spnego_asn1.h
	@-rm -f heimdal/lib/asn1/krb5_asn1.h
	@-rm -f heimdal/lib/asn1/asn1_err.{c,h}
	@-rm -f heimdal/lib/hdb/hdb_err.{c,h}
	@-rm -f heimdal/lib/krb5/heim_err.{c,h}
	@-rm -f heimdal/lib/krb5/k524_err.{c,h}
	@-rm -f heimdal/lib/krb5/krb5_err.{c,h}

#######################
# Start SUBSYSTEM HEIMDAL
[SUBSYSTEM::HEIMDAL]
NOPROTO = YES
REQUIRED_SUBSYSTEMS = \
		HEIMDAL_GSSAPI HEIMDAL_KRB5 \
		HEIMDAL_ASN1 HEIMDAL_DES HEIMDAL_ROKEN HEIMDAL_COM_ERR HEIMDAL_GLUE
# End SUBSYSTEM HEIMDAL
#######################

#######################
# Start SUBSYSTEM KERBEROS_LIB
[SUBSYSTEM::KERBEROS_LIB]
#REQUIRED_SUBSYSTEMS = EXT_LIB_KRB5
REQUIRED_SUBSYSTEMS = HEIMDAL LIBREPLACE
# End SUBSYSTEM KERBEROS_LIB
#######################
