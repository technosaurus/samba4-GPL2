#!/bin/sh
# install miscellaneous files

SRCDIR="$1"
LIBDIR="$2"
BINDIR="$3"

cd $SRCDIR || exit 1

echo "Installing js libs"
mkdir -p $LIBDIR/js || exit 1
cp scripting/libjs/*.js $LIBDIR/js || exit 1

echo "Installing setup templates"
mkdir -p $LIBDIR/setup || exit 1
cp setup/*.ldif $LIBDIR/setup || exit 1
cp setup/*.zone $LIBDIR/setup || exit 1
cp setup/*.conf $LIBDIR/setup || exit 1

echo "Installing script tools"
mkdir -p "$BINDIR"
rm -f scripting/bin/*~
cp scripting/bin/* $BINDIR/ || exit 1

exit 0
