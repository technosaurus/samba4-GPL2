$SRCDIR/script/tests/test_ejs.sh $SERVER $USERNAME $PASSWORD || failed=`expr $failed + $?`
$SRCDIR/script/tests/test_ldap.sh $SERVER $USERNAME $PASSWORD || failed=`expr $failed + $?`
$SRCDIR/script/tests/test_nbt.sh $SERVER || failed=`expr $failed + $?`
$SRCDIR/script/tests/test_quick.sh //$SERVER/cifs $USERNAME $PASSWORD "" || failed=`expr $failed + $?`

