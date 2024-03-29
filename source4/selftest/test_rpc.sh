#!/bin/sh

# add tests to this list as they start passing, so we test
# that they stay passing
ncacn_np_tests="RPC-SPOOLSS RPC-SRVSVC RPC-UNIXINFO RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-ECHO RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON RPC-MGMT RPC-HANDLES RPC-WINREG RPC-WKSSVC RPC-SVCCTL RPC-EPMAPPER RPC-INITSHUTDOWN RPC-EVENTLOG RPC-ATSVC RPC-SAMSYNC"
ncalrpc_tests="RPC-MGMT RPC-UNIXINFO RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-ECHO RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON RPC-WINREG RPC-WKSSVC RPC-SVCCTL RPC-EPMAPPER RPC-EVENTLOG RPC-ATSVC RPC-INITSHUTDOWN"
ncacn_ip_tcp_tests="RPC-UNIXINFO RPC-SCHANNEL RPC-JOIN RPC-LSA RPC-ECHO RPC-DSSETUP RPC-ALTERCONTEXT RPC-MULTIBIND RPC-NETLOGON RPC-MGMT RPC-HANDLES RPC-WINREG RPC-WKSSVC RPC-SVCCTL RPC-EPMAPPER RPC-ATSVC RPC-EVENTLOG RPC-DSSYNC"
slow_ncacn_np_tests="RPC-SAMLOGON RPC-SAMR RPC-SAMR-USERS RPC-SAMR-PASSWORDS RPC-COUNTCALLS"
slow_ncalrpc_tests="RPC-SAMR RPC-SAMR-PASSWORDS RPC-COUNTCALLS RPC-CRACKNAMES"
slow_ncacn_ip_tcp_tests="RPC-SAMR RPC-SAMR-PASSWORDS RPC-COUNTCALLS RPC-CRACKNAMES"

incdir=`dirname $0`
. $incdir/test_functions.sh

for bindoptions in seal,padcheck $VALIDATE bigendian; do
 for transport in ncalrpc ncacn_np ncacn_ip_tcp; do
     case $transport in
	 ncalrpc) tests=$ncalrpc_tests ;;
	 ncacn_np) tests=$ncacn_np_tests ;;
	 ncacn_ip_tcp) tests=$ncacn_ip_tcp_tests ;;
     esac
   for t in $tests; do
    name="$t on $transport with $bindoptions"
    plantest "$name" dc $VALGRIND bin/smbtorture $TORTURE_OPTIONS $transport:"\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN $t "$*"
   done
 done
done

for bindoptions in connect $VALIDATE ; do
 for transport in ncalrpc ncacn_np ncacn_ip_tcp; do
     case $transport in
	 ncalrpc) tests=$slow_ncalrpc_tests ;;
	 ncacn_np) tests=$slow_ncacn_np_tests ;;
	 ncacn_ip_tcp) tests=$slow_ncacn_ip_tcp_tests ;;
     esac
   for t in $tests; do
    name="$t on $transport with $bindoptions"
    plantest "$name" dc $VALGRIND bin/smbtorture $TORTURE_OPTIONS $transport:"\$SERVER[$bindoptions]" -U"\$USERNAME"%"\$PASSWORD" -W \$DOMAIN $t "$*"
   done
 done
done
