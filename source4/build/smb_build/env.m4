dnl SMB Build Environment Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

SMB_VERSION_STRING=`cat include/version.h | grep 'SAMBA_VERSION_OFFICIAL_STRING' | cut -d '"' -f2`
echo "SAMBA VERSION: ${SMB_VERSION_STRING}"

AC_VALIDATE_CACHE_SYSTEM_TYPE

sinclude(build/smb_build/check_path.m4)
sinclude(build/smb_build/check_perl.m4)
sinclude(build/smb_build/check_cc.m4)
sinclude(build/smb_build/check_ld.m4)
sinclude(build/smb_build/check_shld.m4)
sinclude(build/smb_build/check_types.m4)
