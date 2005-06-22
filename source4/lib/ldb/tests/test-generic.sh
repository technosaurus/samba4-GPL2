#!/bin/sh

echo "LDB_URL: $LDB_URL"

echo "Adding base elements"
$VALGRIND bin/ldbadd tests/test.ldif || exit 1

echo "Modifying elements"
$VALGRIND bin/ldbmodify tests/test-modify.ldif || exit 1

echo "Showing modified record"
$VALGRIND bin/ldbsearch '(uid=uham)'  || exit 1

echo "Rename entry"
OLDDN="cn=Ursula Hampster,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST"
NEWDN="cn=Hampster Ursula,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST"
$VALGRIND bin/ldbrename "$OLDDN" "$NEWDN"  || exit 1

echo "Showing renamed record"
$VALGRIND bin/ldbsearch '(uid=uham)' || exit 1

echo "Starting ldbtest"
time $VALGRIND bin/ldbtest --num-records 1000 --num-searches 10  || exit 1

echo "Adding index"
$VALGRIND bin/ldbadd tests/test-index.ldif  || exit 1

echo "Adding attributes"
$VALGRIND bin/ldbadd tests/test-wrong_attributes.ldif  || exit 1

echo "testing indexed search"
$VALGRIND bin/ldbsearch '(uid=uham)'  || exit 1
$VALGRIND bin/ldbsearch '(&(objectclass=person)(objectclass=person)(objectclass=top))' || exit 1
$VALGRIND bin/ldbsearch '(&(uid=uham)(uid=uham))'  || exit 1
$VALGRIND bin/ldbsearch '(|(uid=uham)(uid=uham))'  || exit 1
$VALGRIND bin/ldbsearch '(|(uid=uham)(uid=uham)(objectclass=OpenLDAPperson))'  || exit 1
$VALGRIND bin/ldbsearch '(&(uid=uham)(uid=uham)(!(objectclass=xxx)))'  || exit 1
$VALGRIND bin/ldbsearch '(&(objectclass=person)(uid=uham)(!(uid=uhamxx)))' uid \* \+ dn  || exit 1
$VALGRIND bin/ldbsearch '(&(uid=uham)(uid=uha*)(title=*))' uid || exit 1
$VALGRIND bin/ldbsearch '((' uid && exit 1
$VALGRIND bin/ldbsearch '(objectclass=)' uid || exit 1
$VALGRIND bin/ldbsearch -b 'cn=Hampster Ursula,ou=Alumni Association,ou=People,o=University of Michigan,c=TEST' -s base "" sn || exit 1

echo "Starting ldbtest indexed"
time $VALGRIND bin/ldbtest --num-records 1000 --num-searches 5000  || exit 1

echo "Testing one level search"
count=`$VALGRIND bin/ldbsearch -b 'ou=Groups,o=University of Michigan,c=TEST' -s one 'objectclass=*' none |grep ^dn | wc -l`
if [ "$count" != 3 ]; then
    echo returned $count records - expected 3
    exit 1
fi

echo "Testing binary file attribute value"
$VALGRIND bin/ldbmodify tests/photo.ldif || exit 1
