FEDORA_DS_INF=$LDAPDIR/fedorads.inf
FEDORA_DS_INITIAL_LDIF=$LDAPDIR/fedorads-initial.ldif

echo "FEDORA_DS_LOGLEVEL=0"

#Make the subdirectory be as fedora DS would expect
FEDORA_DS_DIR=$LDAPDIR/slapd-samba4
echo FEDORA_DS_DIR=$FEDORA_DS_DIR

echo "FEDORA_DS_PIDFILE=$FEDORA_DS_DIR/logs/slapd-samba4.pid"

cat >$FEDORA_DS_INF <<EOF
[General]
SuiteSpotUserID = $ROOT
FullMachineName=   localhost
ServerRoot=   $LDAPDIR

[slapd]
ldapifilepath=$LDAPDIR/ldapi
Suffix= $BASEDN
RootDN= cn=Manager,$BASEDN
RootDNPwd= $PASSWORD
ServerIdentifier= samba4
InstallLdifFile=$FEDORA_DS_INITIAL_LDIF

inst_dir= $FEDORA_DS_DIR
config_dir= $FEDORA_DS_DIR
schema_dir= $FEDORA_DS_DIR/schema
lock_dir= $FEDORA_DS_DIR/lock
log_dir= $FEDORA_DS_DIR/logs
run_dir= $FEDORA_DS_DIR/logs
db_dir= $FEDORA_DS_DIR/db
bak_dir= $FEDORA_DS_DIR/bak
tmp_dir= $FEDORA_DS_DIR/tmp
ldif_dir= $FEDORA_DS_DIR/ldif
cert_dir= $FEDORA_DS_DIR

start_server= 0

EOF

cat >$FEDORA_DS_INITIAL_LDIF<<EOF
# These entries need to be added to get the container for the 
# provision to be aimed at.

dn: cn="dc=$BASEDN",cn=mapping tree,cn=config
objectclass: top
objectclass: extensibleObject
objectclass: nsMappingTree
nsslapd-state: backend
nsslapd-backend: userData
cn: $BASEDN

dn: cn=userData,cn=ldbm database,cn=plugins,cn=config
objectclass: extensibleObject
objectclass: nsBackendInstance
nsslapd-suffix: $BASEDN

EOF

perl $FEDORA_DS_PREFIX/bin/ds_newinst.pl $FEDORA_DS_INF >&2 || exit 1;

( 
     cd $FEDORA_DS_DIR/schema
     ls | grep -v ^00core | xargs rm
)

cat >>$FEDORA_DS_DIR/dse.ldif<<EOF
dn: cn=bitwise,cn=plugins,cn=config
objectClass: top
objectClass: nsSlapdPlugin
objectClass: extensibleObject
cn: bitwise
nsslapd-pluginPath: $FEDORA_DS_PREFIX/lib/fedora-ds/plugins/libbitwise-plugin.so
nsslapd-pluginInitfunc: bitwise_init
nsslapd-pluginType: matchingRule
nsslapd-pluginEnabled: on
nsslapd-pluginId: bitwise
nsslapd-pluginVersion: 1.1.0a3
nsslapd-pluginVendor: Fedora Project
nsslapd-pluginDescription: Allow bitwise matching rules

EOF

$srcdir/bin/ad2oLschema $CONFIGURATION -H $PRIVATEDIR/sam.ldb --option=convert:target=fedora-ds -I $srcdir/setup/schema-map-fedora-ds-1.0 -O $FEDORA_DS_DIR/schema/99_ad.ldif >&2

PROVISION_OPTIONS="$PROVISION_OPTIONS --ldap-module=nsuniqueid"
#it is easier to base64 encode this than correctly escape it:
# (targetattr = "*") (version 3.0;acl "full access to all by all";allow (all)(userdn = "ldap:///anyone");)
PROVISION_ACI="--aci=aci:: KHRhcmdldGF0dHIgPSAiKiIpICh2ZXJzaW9uIDMuMDthY2wgImZ1bGwgYWNjZXNzIHRvIGFsbCBieSBhbGwiO2FsbG93IChhbGwpKHVzZXJkbiA9ICJsZGFwOi8vL2FueW9uZSIpOykK"

