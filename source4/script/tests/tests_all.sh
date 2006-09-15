#!/bin/sh
 $SRCDIR/script/tests/test_ejs.sh $DOMAIN $USERNAME $PASSWORD || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_ldap.sh $SERVER $USERNAME $PASSWORD || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_nbt.sh $SERVER || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_quick.sh //$SERVER/cifs $USERNAME $PASSWORD "" || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_rpc.sh $SERVER $USERNAME $PASSWORD $DOMAIN || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_net.sh $SERVER $USERNAME $PASSWORD $DOMAIN || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_session_key.sh $SERVER $USERNAME $PASSWORD $DOMAIN $NETBIOSNAME || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_binding_string.sh $SERVER $USERNAME $PASSWORD $DOMAIN || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_echo.sh $SERVER $USERNAME $PASSWORD $DOMAIN || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_posix.sh //$SERVER/tmp $USERNAME $PASSWORD "" || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_local.sh || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_pidl.sh || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_smbclient.sh $SERVER $USERNAME $PASSWORD $DOMAIN $PREFIX || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_cifsdd.sh $SERVER $USERNAME $PASSWORD $DOMAIN || failed=`expr $failed + $?`
 $SRCDIR/script/tests/test_simple.sh //$SERVER/simple $USERNAME $PASSWORD "" || failed=`expr $failed + $?`
