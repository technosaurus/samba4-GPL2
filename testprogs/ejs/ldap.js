#!/bin/sh
exec smbscript "$0" ${1+"$@"}
/*
	test certin LDAP behaviours
*/

var ldb = ldb_init();

var options = GetOptions(ARGV, 
		"POPT_AUTOHELP",
		"POPT_COMMON_SAMBA",
		"POPT_COMMON_CREDENTIALS");
if (options == undefined) {
   println("Failed to parse options");
   return -1;
}

libinclude("base.js");

if (options.ARGV.length != 1) {
   println("Usage: ldap.js <HOST>");
   return -1;
}

var host = options.ARGV[0];

function basic_tests(ldb, base_dn)
{
	println("Running basic tests");

	ldb.del("cn=ldaptestuser,cn=users," + base_dn);

	var ok = ldb.add("
dn: cn=ldaptestuser,cn=users," + base_dn + "
objectClass: user
objectClass: person
cn: LDAPtestUSER
");
	if (!ok) {
		ok = ldb.del("cn=ldaptestuser,cn=users," + base_dn);
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
		ok = ldb.add("
dn: cn=ldaptestuser,cn=users," + base_dn + "
objectClass: user
objectClass: person
cn: LDAPtestUSER
");
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	}

	var ok = ldb.add("
dn: cn=ldaptestcomputer,cn=computers," + base_dn + "
objectClass: computer
cn: LDAPtestCOMPUTER
");
	if (!ok) {
		ok = ldb.del("cn=ldaptestcomputer,cn=computers," + base_dn);
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
		ok = ldb.add("
dn: cn=ldaptestcomputer,cn=computers," + base_dn + "
objectClass: computer
cn: LDAPtestCOMPUTER
");
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	}

	ok = ldb.add("
dn: cn=ldaptestuser2,cn=users," + base_dn + "
objectClass: person
objectClass: user
cn: LDAPtestUSER2
");
	if (!ok) {
		ok = ldb.del("cn=ldaptestuser2,cn=users," + base_dn);
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	        ok = ldb.add("
dn: cn=ldaptestuser2,cn=users," + base_dn + "
objectClass: person
objectClass: user
cn: LDAPtestUSER2
");
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	}

	ok = ldb.add("
dn: cn=ldaptestutf8user èùéìòà ,cn=users," + base_dn + "
objectClass: user
");
	if (!ok) {
		ok = ldb.del("cn=ldaptestutf8user èùéìòà ,cn=users," + base_dn);
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	ok = ldb.add("
dn: cn=ldaptestutf8user èùéìòà ,cn=users," + base_dn + "
objectClass: user
");
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	}

	ok = ldb.add("
dn: cn=ldaptestutf8user2  èùéìòà ,cn=users," + base_dn + "
objectClass: user
");
	if (!ok) {
		ok = ldb.del("cn=ldaptestutf8user2  èùéìòà ,cn=users," + base_dn);
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	ok = ldb.add("
dn: cn=ldaptestutf8user2  èùéìòà ,cn=users," + base_dn + "
objectClass: user
");
		if (!ok) {
			println(ldb.errstring());
			assert(ok);
		}
	}

	println("Testing ldb.search for (&(cn=ldaptestuser)(objectClass=user))");
	var res = ldb.search("(&(cn=ldaptestuser)(objectClass=user))");
	if (res.length != 1) {
		println("Could not find (&(cn=ldaptestuser)(objectClass=user))");
		assert(res.length == 1);
	}

	assert(res[0].dn == "cn=ldaptestuser,cn=users," + base_dn);
	assert(res[0].cn == "ldaptestuser");
	assert(res[0].name == "ldaptestuser");
	assert(res[0].objectClass[0] == "top");
	assert(res[0].objectClass[1] == "person");
	assert(res[0].objectClass[2] == "organizationalPerson");
	assert(res[0].objectClass[3] == "user");
	assert(res[0].objectGUID != undefined);
	assert(res[0].whenCreated != undefined);
	assert(res[0].objectCategory == "cn=Person,cn=Schema,cn=Configuration," + base_dn);

	println("Testing ldb.search for (&(cn=ldaptestuser)(objectCategory=cn=person,cn=schema,cn=configuration," + base_dn + "))");
	var res2 = ldb.search("(&(cn=ldaptestuser)(objectCategory=cn=person,cn=schema,cn=configuration," + base_dn + "))");
	if (res2.length != 1) {
		println("Could not find (&(cn=ldaptestuser)(objectCategory=cn=person,cn=schema,cn=configuration," + base_dn + "))");
		assert(res2.length == 1);
	}

	assert(res[0].dn == res2[0].dn);

	println("Testing ldb.search for (&(cn=ldaptestuser)(objectCategory=PerSon))");
	var res3 = ldb.search("(&(cn=ldaptestuser)(objectCategory=PerSon))");
	if (res.length != 1) {
		println("Could not find (&(cn=ldaptestuser)(objectCategory=PerSon))");
		assert(res.length == 1);
	}

	assert(res[0].dn == res3[0].dn);

	ok = ldb.del(res[0].dn);
	if (!ok) {
		println(ldb.errstring());
		assert(ok);
	}

	println("Testing ldb.search for (&(cn=ldaptestcomputer)(objectClass=user))");
	var res = ldb.search("(&(cn=ldaptestcomputer)(objectClass=user))");
	if (res.length != 1) {
		println("Could not find (&(cn=ldaptestuser)(objectClass=user))");
		assert(res.length == 1);
	}

	assert(res[0].dn == "cn=ldaptestcomputer,cn=computers," + base_dn);
	assert(res[0].cn == "ldaptestcomputer");
	assert(res[0].name == "ldaptestcomputer");
	assert(res[0].objectClass[0] == "top");
	assert(res[0].objectClass[1] == "person");
	assert(res[0].objectClass[2] == "organizationalPerson");
	assert(res[0].objectClass[3] == "user");
	assert(res[0].objectClass[4] == "computer");
	assert(res[0].objectGUID != undefined);
	assert(res[0].whenCreated != undefined);
	assert(res[0].objectCategory == "cn=Computer,cn=Schema,cn=Configuration," + base_dn);

	println("Testing ldb.search for (&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + base_dn + "))");
	var res2 = ldb.search("(&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + base_dn + "))");
	if (res2.length != 1) {
		println("Could not find (&(cn=ldaptestcomputer)(objectCategory=cn=computer,cn=schema,cn=configuration," + base_dn + "))");
		assert(res2.length == 1);
	}

	assert(res[0].dn == res2[0].dn);

	println("Testing ldb.search for (&(cn=ldaptestcomputer)(objectCategory=compuTER))");
	var res3 = ldb.search("(&(cn=ldaptestcomputer)(objectCategory=compuTER))");
	if (res3.length != 1) {
		println("Could not find (&(cn=ldaptestcomputer)(objectCategory=compuTER))");
		assert(res3.length == 1);
	}

	assert(res[0].dn == res3[0].dn);

	println("Testing ldb.search for (&(cn=ldaptest*computer)(objectCategory=compuTER))");
	var res4 = ldb.search("(&(cn=ldaptest*computer)(objectCategory=compuTER))");
	if (res4.length != 1) {
		println("Could not find (&(cn=ldaptest*computer)(objectCategory=compuTER))");
		assert(res4.length == 1);
	}

	assert(res[0].dn == res4[0].dn);

	println("Testing ldb.search for (&(cn=ldaptestcomput*)(objectCategory=compuTER))");
	var res5 = ldb.search("(&(cn=ldaptestcomput*)(objectCategory=compuTER))");
	if (res5.length != 1) {
		println("Could not find (&(cn=ldaptestcomput*)(objectCategory=compuTER))");
		assert(res5.length == 1);
	}

	assert(res[0].dn == res5[0].dn);

	println("Testing ldb.search for (&(cn=*daptestcomputer)(objectCategory=compuTER))");
	var res6 = ldb.search("(&(cn=*daptestcomputer)(objectCategory=compuTER))");
	if (res6.length != 1) {
		println("Could not find (&(cn=*daptestcomputer)(objectCategory=compuTER))");
		assert(res6.length == 1);
	}

	assert(res[0].dn == res6[0].dn);

	ok = ldb.del(res[0].dn);
	if (!ok) {
		println(ldb.errstring());
		assert(ok);
	}

	println("Testing ldb.search for (&(cn=ldaptestUSer2)(objectClass=user))");
	var res = ldb.search("(&(cn=ldaptestUSer2)(objectClass=user))");
	if (res.length != 1) {
		println("Could not find (&(cn=ldaptestUSer2)(objectClass=user))");
		assert(res.length == 1);
	}

	assert(res[0].dn == "cn=ldaptestuser2,cn=users," + base_dn);
	assert(res[0].cn == "ldaptestuser2");
	assert(res[0].name == "ldaptestuser2");
	assert(res[0].objectClass[0] == "top");
	assert(res[0].objectClass[1] == "person");
	assert(res[0].objectClass[2] == "organizationalPerson");
	assert(res[0].objectClass[3] == "user");
	assert(res[0].objectGUID != undefined);
	assert(res[0].whenCreated != undefined);

	ok = ldb.del(res[0].dn);
	if (!ok) {
		println(ldb.errstring());
		assert(ok);
	}

	println("Testing ldb.search for (&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))");
	var res = ldb.search("(&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))");

	if (res.length != 1) {
		println("Could not find (&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))");
		assert(res.length == 1);
	}

	assert(res[0].dn == "cn=ldaptestutf8user èùéìòà,cn=users," + base_dn);
	assert(res[0].cn == "ldaptestutf8user èùéìòà");
	assert(res[0].name == "ldaptestutf8user èùéìòà");
	assert(res[0].objectClass[0] == "top");
	assert(res[0].objectClass[1] == "person");
	assert(res[0].objectClass[2] == "organizationalPerson");
	assert(res[0].objectClass[3] == "user");
	assert(res[0].objectGUID != undefined);
	assert(res[0].whenCreated != undefined);

	ok = ldb.del(res[0].dn);
	if (!ok) {
		println(ldb.errstring());
		assert(ok);
	}

	println("Testing ldb.search for (&(cn=ldaptestutf8user2 ÈÙÉÌÒÀ)(objectClass=user))");
	var res = ldb.search("(&(cn=ldaptestutf8user ÈÙÉÌÒÀ)(objectClass=user))");

	if (res.length != 1) {
		println("Could not find (expect space collapse, win2k3 fails) (&(cn=ldaptestutf8user2 ÈÙÉÌÒÀ)(objectClass=user))");
	} else {
		assert(res[0].dn == "cn=ldaptestutf8user2 èùéìòà,cn=users," + base_dn);
		assert(res[0].cn == "ldaptestutf8user2 èùéìòà");
	}

}

function find_basedn(ldb)
{
    var attrs = new Array("defaultNamingContext");
    var res = ldb.search("", "", ldb.SCOPE_BASE, attrs);
    assert(res.length == 1);
    return res[0].defaultNamingContext;
}

/* use command line creds if available */
ldb.credentials = options.get_credentials();

var ok = ldb.connect("ldap://" + host);
var base_dn = find_basedn(ldb);

printf("baseDN: %s\n", base_dn);

basic_tests(ldb, base_dn)

return 0;
