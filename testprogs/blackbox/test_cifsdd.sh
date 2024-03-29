#!/bin/sh

# Basic script to make sure that cifsdd can do both local and remote I/O.

if [ $# -lt 4 ]; then
cat <<EOF
Usage: test_cifsdd.sh SERVER USERNAME PASSWORD DOMAIN
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
PASSWORD=$3
DOMAIN=$4

DD=bin/cifsdd

SHARE=tmp
DEBUGLEVEL=1

failed=0

testit() {
	name="$1"
	shift
	cmdline="$*"
	echo "test: $name"
	$cmdline
	status=$?
	if [ x$status = x0 ]; then
		echo "success: $name"
	else
		echo "failure: $name"
		failed=`expr $failed + 1`
	fi
	return $status
}

runcopy() {
	message="$1"
	shift
	
	testit "$message" $VALGRIND $DD $CONFIGURATION --debuglevel=$DEBUGLEVEL -W "$DOMAIN" -U "$USERNAME"%"$PASSWORD" \
	    "$@"
}

compare() {
    testit "$1" cmp "$2" "$3"
}

sourcepath=tempfile.src.$$
destpath=tempfile.dst.$$

# Create a source file with arbitrary contents
dd if=$DD of=$sourcepath bs=1024 count=50 > /dev/null

ls -l $sourcepath

for bs in 512 4k 48k ; do

echo "Testing $bs block size ..."

# Check whether we can do local IO
runcopy "Testing local -> local copy" if=$sourcepath of=$destpath bs=$bs
compare "Checking local differences" $sourcepath $destpath

# Check whether we can do a round trip
runcopy "Testing local -> remote copy" \
	    if=$sourcepath of=//$SERVER/$SHARE/$sourcepath bs=$bs 
runcopy "Testing remote -> local copy" \
	    if=//$SERVER/$SHARE/$sourcepath of=$destpath bs=$bs 
compare "Checking differences" $sourcepath $destpath 

# Check that copying within the remote server works
runcopy "Testing local -> remote copy" \
	    if=//$SERVER/$SHARE/$sourcepath of=//$SERVER/$SHARE/$sourcepath bs=$bs
runcopy "Testing remote -> remote copy" \
	    if=//$SERVER/$SHARE/$sourcepath of=//$SERVER/$SHARE/$destpath bs=$bs 
runcopy "Testing remote -> local copy" \
	    if=//$SERVER/$SHARE/$destpath of=$destpath bs=$bs
compare "Checking differences" $sourcepath $destpath

done

rm -f $sourcepath $destpath

exit $failed
