#!/bin/sh

VERSION_FILE=$1
OUTPUT_FILE=$2

if test -z "$VERSION_FILE";then
	VERSION_FILE="VERSION"
fi

if test -z "$OUTPUT_FILE";then
	OUTPUT_FILE="include/version.h"
fi

SOURCE_DIR=$3

SAMBA_VERSION_MAJOR=`sed -n 's/^SAMBA_VERSION_MAJOR=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_MINOR=`sed -n 's/^SAMBA_VERSION_MINOR=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_RELEASE=`sed -n 's/^SAMBA_VERSION_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_REVISION=`sed -n 's/^SAMBA_VERSION_REVISION=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_TP_RELEASE=`sed -n 's/^SAMBA_VERSION_TP_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_PRE_RELEASE=`sed -n 's/^SAMBA_VERSION_PRE_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_RC_RELEASE=`sed -n 's/^SAMBA_VERSION_RC_RELEASE=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_IS_SVN_SNAPSHOT=`sed -n 's/^SAMBA_VERSION_IS_SVN_SNAPSHOT=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_RELEASE_NICKNAME=`sed -n 's/^SAMBA_VERSION_RELEASE_NICKNAME=//p' $SOURCE_DIR$VERSION_FILE`

SAMBA_VERSION_VENDOR_SUFFIX=`sed -n 's/^SAMBA_VERSION_VENDOR_SUFFIX=//p' $SOURCE_DIR$VERSION_FILE`
SAMBA_VERSION_VENDOR_PATCH=`sed -n 's/^SAMBA_VERSION_VENDOR_PATCH=//p' $SOURCE_DIR$VERSION_FILE`

echo "/* Autogenerated by script/mkversion.sh */" > $OUTPUT_FILE

echo "#define SAMBA_VERSION_MAJOR ${SAMBA_VERSION_MAJOR}" >> $OUTPUT_FILE
echo "#define SAMBA_VERSION_MINOR ${SAMBA_VERSION_MINOR}" >> $OUTPUT_FILE
echo "#define SAMBA_VERSION_RELEASE ${SAMBA_VERSION_RELEASE}" >> $OUTPUT_FILE


##
## start with "3.0.22"
##
SAMBA_VERSION_STRING="${SAMBA_VERSION_MAJOR}.${SAMBA_VERSION_MINOR}.${SAMBA_VERSION_RELEASE}"


##
## maybe add "3.0.22a" or "4.0.0tp11" or "3.0.22pre1" or "3.0.22rc1"
## We do not do pre or rc version on patch/letter releases
##
if test -n "${SAMBA_VERSION_REVISION}";then
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}${SAMBA_VERSION_REVISION}"
    echo "#define SAMBA_VERSION_REVISION \"${SAMBA_VERSION_REVISION}\"" >> $OUTPUT_FILE
elif test -n "${SAMBA_VERSION_TP_RELEASE}";then
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}tp${SAMBA_VERSION_TP_RELEASE}"
    echo "#define SAMBA_VERSION_TP_RELEASE ${SAMBA_VERSION_TP_RELEASE}" >> $OUTPUT_FILE
elif test -n "${SAMBA_VERSION_PRE_RELEASE}";then
    ## maybe add "3.0.22pre2"
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}pre${SAMBA_VERSION_PRE_RELEASE}"
    echo "#define SAMBA_VERSION_PRE_RELEASE ${SAMBA_VERSION_PRE_RELEASE}" >> $OUTPUT_FILE
elif test -n "${SAMBA_VERSION_RC_RELEASE}";then
    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}rc${SAMBA_VERSION_RC_RELEASE}"
    echo "#define SAMBA_VERSION_RC_RELEASE ${SAMBA_VERSION_RC_RELEASE}" >> $OUTPUT_FILE
fi

##
## SVN revision number? 
##
if test x"${SAMBA_VERSION_IS_SVN_SNAPSHOT}" = x"yes";then
    _SAVE_LANG=${LANG}
    LANG=""
    HAVESVN=no
    svn info ${SOURCE_DIR} >/dev/null 2>&1 && HAVESVN=yes
    TMP_REVISION=`(svn info ${SOURCE_DIR} 2>/dev/null || svk info ${SOURCE_DIR} 2>/dev/null) |grep 'Last Changed Rev.*:' |sed -e 's/Last Changed Rev.*: \([0-9]*\).*/\1/'`
    if test x"${HAVESVN}" = x"no";then
	HAVESVK=no
	svk info ${SOURCE_DIR} >/dev/null 2>&1 && HAVESVK=yes
	TMP_MIRRORED_REVISION=`(svk info ${SOURCE_DIR} 2>/dev/null) |grep 'Mirrored From:.*samba\.org.*' |sed -e 's/Mirrored From: .* Rev\..* \([0-9]*\).*/\1/'`
	if test -n "$TMP_MIRRORED_REVISION"; then
	    TMP_SVK_REVISION_STR="${TMP_REVISION}-${USER}@${HOSTNAME}-[SVN-${TMP_MIRRORED_REVISION}]"
	else
	    TMP_SVK_REVISION_STR="${TMP_REVISION}-${USER}@${HOSTNAME}"
	fi
    fi

    if test x"${HAVESVN}" = x"yes";then
	    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-SVN-build-${TMP_REVISION}"
	    echo "#define SAMBA_VERSION_SVN_REVISION ${TMP_REVISION}" >> $OUTPUT_FILE
    elif test x"${HAVESVK}" = x"yes";then
	    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-SVK-build-${TMP_SVK_REVISION_STR}"
    else
	    SAMBA_VERSION_STRING="${SAMBA_VERSION_STRING}-SVN-build-UNKNOWN"
    fi
    LANG=${_SAVE_LANG}
fi

##
## Add a release nickname
##
if test -n "${SAMBA_VERSION_RELEASE_NICKNAME}";then
    echo "#define SAMBA_VERSION_RELEASE_NICKNAME ${SAMBA_VERSION_RELEASE_NICKNAME}" >> $OUTPUT_FILE
fi

##
## Add the vendor string if present
##
if test -n "${SAMBA_VERSION_VENDOR_SUFFIX}";then
    echo "#define SAMBA_VERSION_VENDOR_SUFFIX ${SAMBA_VERSION_VENDOR_SUFFIX}" >> $OUTPUT_FILE
    if test -n "${SAMBA_VERSION_VENDOR_PATCH}";then
        echo "#define SAMBA_VERSION_VENDOR_PATCH ${SAMBA_VERSION_VENDOR_PATCH}" >> $OUTPUT_FILE
    fi
fi

echo "#define SAMBA_VERSION_OFFICIAL_STRING \"${SAMBA_VERSION_STRING}\"" >> $OUTPUT_FILE

echo "#define SAMBA_VERSION_STRING samba_version_string()" >> $OUTPUT_FILE

echo "$0: 'include/version.h' created for Samba(\"${SAMBA_VERSION_STRING}\")"

if test -n "${SAMBA_VERSION_RELEASE_NICKNAME}";then
    echo "$0: with RELEASE_NICKNAME = ${SAMBA_VERSION_RELEASE_NICKNAME}"
fi

if test -n "${SAMBA_VERSION_VENDOR_SUFFIX}";then
    echo "$0: with VENDOR_SUFFIX = ${SAMBA_VERSION_VENDOR_SUFFIX}"
    if test -n "${SAMBA_VERSION_VENDOR_PATCH}";then
        echo "$0: with VENDOR_PATCH = ${SAMBA_VERSION_VENDOR_PATCH}"
    fi
fi

exit 0
