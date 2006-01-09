#!/bin/sh

# add tests to this list as they start passing, so we test
# that they stay passing
ncacn_np_tests="RPC-ALTERCONTEXT RPC-JOIN"
ncalrpc_tests="RPC-ALTERCONTEXT RPC-JOIN"
ncacn_ip_tcp_tests="RPC-ALTERCONTEXT RPC-JOIN"

# if we're not running under valgrind test some more tests
if [ -z "$VALGRIND" ]; then
	ncacn_np_tests="$ncacn_np_tests RPC-ECHO"
	ncalrpc_tests="$ncalrpc_tests RPC-ECHO"
	ncacn_ip_tcp_tests="$ncacn_ip_tcp_tests RPC-ECHO"
fi

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_rpc_quick.sh SERVER USERNAME PASSWORD DOMAIN
EOF
exit 1;
fi

server="$1"
username="$2"
password="$3"
domain="$4"
shift 4

incdir=`dirname $0`
. $incdir/test_functions.sh

failed=0
for bindoptions in seal,validate,padcheck bigendian; do
 for transport in ncalrpc ncacn_np ncacn_ip_tcp; do
   case $transport in
	 ncalrpc) tests=$ncalrpc_tests ;;
	 ncacn_np) tests=$ncacn_np_tests ;;
	 ncacn_ip_tcp) tests=$ncacn_ip_tcp_tests ;;
   esac
   for t in $tests; do
    name="$t on $transport with $bindoptions"
    testit "$name" $VALGRIND bin/smbtorture $TORTURE_OPTIONS $transport:"$server[$bindoptions]" -U"$username"%"$password" -W $domain $t "$*" || failed=`expr $failed + 1`
   done
 done
done

testok $0 $failed
