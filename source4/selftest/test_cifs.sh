#!/bin/sh

# this runs the file serving tests that are expected to pass with the
# current posix ntvfs backend, via the ntvfs cifs proxy

ADDARGS="$*"

incdir=`dirname $0`
. $incdir/test_functions.sh

raw=`bin/smbtorture --list | grep "^RAW-" | xargs`
base=`bin/smbtorture --list | grep "^BASE-" | xargs`
tests="$base $raw $smb2"

for t in $tests; do
    if [ ! -z "$start" -a "$start" != $t ]; then
	continue;
    fi
    start=""
    plantest "ntvfs/cifs $t" dc $VALGRIND bin/smbtorture $TORTURE_OPTIONS $ADDARGS //\$NETBIOSNAME/cifs -U"\$USERNAME"%"\$PASSWORD" $t
done
