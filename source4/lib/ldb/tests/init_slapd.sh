#!/bin/sh 

if [ -z "$LDBDIR" ]; then
    LDBDIR=`dirname $0`/..
    export LDBDIR
fi

rm -rf tests/tmp/db
mkdir -p tests/tmp/db

if [ -f tests/tmp/slapd.pid ]; then
    kill `cat tests/tmp/slapd.pid`
    sleep 1
fi
if [ -f tests/tmp/slapd.pid ]; then
    kill -9 `cat tests/tmp/slapd.pid`
    rm -f tests/tmp/slapd.pid
fi

slapadd -f $LDBDIR/tests/slapd.conf < $LDBDIR/tests/init.ldif || exit 1
