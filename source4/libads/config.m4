########################################################
# Compile with LDAP support?

LDAP_LIBS=""
AC_SUBST(LDAP_LIBS)
with_ldap_support=auto
AC_MSG_CHECKING([for LDAP support])

AC_ARG_WITH(ldap,
[  --with-ldap             LDAP support (default yes)],
[ case "$withval" in
    yes|no)
	with_ldap_support=$withval
	;;
  esac ])

AC_MSG_RESULT($with_ldap_support)

if test x"$with_ldap_support" != x"no"; then

  ##################################################################
  # first test for ldap.h and lber.h
  # (ldap.h is required for this test)
  AC_CHECK_HEADERS(ldap.h lber.h)
  
  if test x"$ac_cv_header_ldap_h" != x"yes"; then
	if test x"$with_ldap_support" = x"yes"; then
	 AC_MSG_ERROR(ldap.h is needed for LDAP support)
	else
	 AC_MSG_WARN(ldap.h is needed for LDAP support)
	fi
	
	with_ldap_support=no
  fi
fi

if test x"$with_ldap_support" != x"no"; then
  ac_save_LIBS=$LIBS

  ##################################################################
  # we might need the lber lib on some systems. To avoid link errors
  # this test must be before the libldap test
  AC_CHECK_LIB_EXT(lber, LDAP_LIBS, ber_scanf)

  ########################################################
  # now see if we can find the ldap libs in standard paths
  AC_CHECK_LIB_EXT(ldap, LDAP_LIBS, ldap_init)

  AC_CHECK_FUNC_EXT(ldap_domain2hostlist,$LDAP_LIBS)
  
  ########################################################
  # If we have LDAP, does it's rebind procedure take 2 or 3 arguments?
  # Check found in pam_ldap 145.
  AC_CHECK_FUNC_EXT(ldap_set_rebind_proc,$LDAP_LIBS)

  LIBS="$LIBS $LDAP_LIBS"
  AC_CACHE_CHECK(whether ldap_set_rebind_proc takes 3 arguments, smb_ldap_cv_ldap_set_rebind_proc, [
    AC_TRY_COMPILE([
	#include <lber.h>
	#include <ldap.h>], 
	[ldap_set_rebind_proc(0, 0, 0);], 
	[smb_ldap_cv_ldap_set_rebind_proc=3], 
	[smb_ldap_cv_ldap_set_rebind_proc=2]
    ) 
  ])
  
  AC_DEFINE_UNQUOTED(LDAP_SET_REBIND_PROC_ARGS, $smb_ldap_cv_ldap_set_rebind_proc, [Number of arguments to ldap_set_rebind_proc])

  AC_CHECK_FUNC_EXT(ldap_initialize,$LDAP_LIBS)	
  
  if test x"$ac_cv_lib_ext_ldap_ldap_init" = x"yes" -a x"$ac_cv_func_ext_ldap_domain2hostlist" = x"yes"; then
    AC_DEFINE(HAVE_LDAP,1,[Whether ldap is available])
    SMB_MODULE_DEFAULT(STATIC,pdb_ldap)
    with_ldap_support=yes
    AC_MSG_CHECKING(whether LDAP support is used)
    AC_MSG_RESULT(yes)
  else
    if test x"$with_ldap_support" = x"yes"; then
	AC_MSG_ERROR(libldap is needed for LDAP support)
    else
	AC_MSG_WARN(libldap is needed for LDAP support)
    fi
    
    LDAP_LIBS=""
    with_ldap_support=no
  fi
  LIBS=$ac_save_LIBS
fi


#################################################
# active directory support

KRB5_LIBS=""
AC_SUBST(KRB5_LIBS)
with_ads_support=auto
AC_MSG_CHECKING([for Active Directory and krb5 support])

AC_ARG_WITH(ads,
[  --with-ads              Active Directory support (default auto)],
[ case "$withval" in
    yes|no)
	with_ads_support="$withval"
	;;
  esac ])

AC_MSG_RESULT($with_ads_support)

if test x"$with_ldap_support" != x"yes"; then
    if test x"$with_ads_support" = x"yes"; then
	AC_MSG_ERROR(Active Directory Support requires LDAP support)
    elif test x"$with_ads_support" != x"no"; then
	AC_MSG_WARN(Active Directory Support requires LDAP support)
    fi
    with_ads_support=no
fi

if test x"$with_ads_support" != x"no"; then
  FOUND_KRB5=no
  # Do no harm to the values of CFLAGS and LIBS while testing for
  # Kerberos support.

  #################################################
  # check for krb5-config from recent MIT and Heimdal kerberos 5
  AC_PATH_PROG(KRB5_CONFIG, krb5-config)
  AC_MSG_CHECKING(for working krb5-config)
  if test -x "$KRB5_CONFIG"; then
    ac_save_CFLAGS=$CFLAGS
    CFLAGS="";export CFLAGS
    ac_save_LDFLAGS=$LDFLAGS
    LDFLAGS="";export LDFLAGS
    KRB5_LIBS="`$KRB5_CONFIG --libs gssapi`"
    KRB5_CFLAGS="`$KRB5_CONFIG --cflags | sed s/@INCLUDE_des@//`" 
    KRB5_CPPFLAGS="`$KRB5_CONFIG --cflags | sed s/@INCLUDE_des@//`"
    CFLAGS=$ac_save_CFLAGS;export CFLAGS
    LDFLAGS=$ac_save_LDFLAGS;export LDFLAGS
    FOUND_KRB5=yes
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no. Fallback to previous krb5 detection strategy)
  fi
  
  if test x$FOUND_KRB5 = x"no"; then
    #################################################
    # check for location of Kerberos 5 install
    AC_MSG_CHECKING(for kerberos 5 install path)
    AC_ARG_WITH(krb5,
    [  --with-krb5=base-dir    Locate Kerberos 5 support (default=/usr)],
    [ case "$withval" in
      no)
        AC_MSG_RESULT(no krb5-path given)
        ;;
      yes)
        AC_MSG_RESULT(/usr)
        FOUND_KRB5=yes
        ;;
      *)
        AC_MSG_RESULT($withval)
        KRB5_CFLAGS="-I$withval/include"
        KRB5_CPPFLAGS="-I$withval/include"
        KRB5_LDFLAGS="-L$withval/lib"
        FOUND_KRB5=yes
        ;;
      esac ],
      AC_MSG_RESULT(no krb5-path given)
    )
  fi

  if test x$FOUND_KRB5 = x"no"; then
    #################################################
    # see if this box has the SuSE location for the heimdal krb implementation
    AC_MSG_CHECKING(for /usr/include/heimdal)
    if test -d /usr/include/heimdal; then
      if test -f /usr/lib/heimdal/lib/libkrb5.a; then
          KRB5_CFLAGS="-I/usr/include/heimdal"
          KRB5_CPPFLAGS="-I/usr/include/heimdal"
          KRB5_LDFLAGS="-L/usr/lib/heimdal/lib"
          AC_MSG_RESULT(yes)
      else
          KRB5_CFLAGS="-I/usr/include/heimdal"
          KRB5_CPPFLAGS="-I/usr/include/heimdal"
          AC_MSG_RESULT(yes)
      fi
    else
      AC_MSG_RESULT(no)
    fi
  fi

  if test x$FOUND_KRB5 = x"no"; then
    #################################################
    # see if this box has the RedHat location for kerberos
    AC_MSG_CHECKING(for /usr/kerberos)
    if test -d /usr/kerberos -a -f /usr/kerberos/lib/libkrb5.a; then
      KRB5_LDFLAGS="-L/usr/kerberos/lib"
      KRB5_CFLAGS="-I/usr/kerberos/include"
      KRB5_CPPFLAGS="-I/usr/kerberos/include"
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
    fi
  fi

  ac_save_CFLAGS=$CFLAGS
  ac_save_CPPFLAGS=$CPPFLAGS
  ac_save_LDFLAGS=$LDFLAGS

  CFLAGS="$CFLAGS $KRB5_CFLAGS"
  CPPFLAGS="$CPPFLAGS $KRB5_CPPFLAGS"
  LDFLAGS="$LDFLAGS $KRB5_LDFLAGS"

  KRB5_LIBS="$KRB5_LDFLAGS $KRB5_LIBS"

  # now check for krb5.h. Some systems have the libraries without the headers!
  # note that this check is done here to allow for different kerberos
  # include paths
  AC_CHECK_HEADERS(krb5.h)

  if test x"$ac_cv_header_krb5_h" = x"no"; then

    # Give a warning if AD support was not explicitly requested,
    # i.e with_ads_support = auto, otherwise die with an error.

    if test x"$with_ads_support" = x"yes"; then
      AC_MSG_ERROR([Active Directory cannot be supported without krb5.h])
    else
      AC_MSG_WARN([Active Directory cannot be supported without krb5.h])
    fi

    # Turn off AD support and restore CFLAGS and LIBS variables

    with_ads_support="no"
    
    CFLAGS=$ac_save_CFLAGS
    CPPFLAGS=$ac_save_CPPFLAGS
    LDFLAGS=$ac_save_LDFLAGS
  fi
fi

# Now we have determined whether we really want ADS support

if test x"$with_ads_support" != x"no"; then
  ac_save_LIBS=$LIBS

  # now check for gssapi headers.  This is also done here to allow for
  # different kerberos include paths
  AC_CHECK_HEADERS(gssapi.h gssapi/gssapi_generic.h gssapi/gssapi.h com_err.h)

  ##################################################################
  # we might need the k5crypto and com_err libraries on some systems
  AC_CHECK_LIB_EXT(com_err, KRB5_LIBS, _et_list)
  AC_CHECK_LIB_EXT(k5crypto, KRB5_LIBS, krb5_encrypt_data)

  # Heimdal checks.
  AC_CHECK_LIB_EXT(crypto, KRB5_LIBS, des_set_key)
  AC_CHECK_LIB_EXT(asn1, KRB5_LIBS, copy_Authenticator)
  AC_CHECK_LIB_EXT(roken, KRB5_LIBS, roken_getaddrinfo_hostspec)

  # Heimdal checks. On static Heimdal gssapi must be linked before krb5.
  AC_CHECK_LIB_EXT(gssapi, KRB5_LIBS, gss_display_status,[],[],
				AC_DEFINE(HAVE_GSSAPI,1,[Whether GSSAPI is available]))

  ########################################################
  # now see if we can find the krb5 libs in standard paths
  # or as specified above
  AC_CHECK_LIB_EXT(krb5, KRB5_LIBS, krb5_mk_req_extended)

  ########################################################
  # now see if we can find the gssapi libs in standard paths
  AC_CHECK_LIB_EXT(gssapi_krb5, KRB5_LIBS,gss_display_status,[],[],
	    AC_DEFINE(HAVE_GSSAPI,1,[Whether GSSAPI is available]))

  AC_CHECK_FUNC_EXT(krb5_set_real_time, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_set_default_in_tkt_etypes, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_set_default_tgs_ktypes, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_principal2salt, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_use_enctype, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_string_to_key, $KRB5_LIBS) 
  AC_CHECK_FUNC_EXT(krb5_get_pw_salt, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_string_to_key_salt, $KRB5_LIBS) 
  AC_CHECK_FUNC_EXT(krb5_auth_con_setkey, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_auth_con_setuseruserkey, $KRB5_LIBS) 
  AC_CHECK_FUNC_EXT(krb5_locate_kdc, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_get_permitted_enctypes, $KRB5_LIBS) 
  AC_CHECK_FUNC_EXT(krb5_get_default_in_tkt_etypes, $KRB5_LIBS) 
  AC_CHECK_FUNC_EXT(krb5_free_ktypes, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_free_data_contents, $KRB5_LIBS)
  AC_CHECK_FUNC_EXT(krb5_principal_get_comp_string, $KRB5_LIBS)

  LIBS="$LIBS $KRB5_LIBS"
  
  AC_CACHE_CHECK([for addrtype in krb5_address],
                samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_address kaddr; kaddr.addrtype = ADDRTYPE_INET;],
      samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS=yes,
      samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS=no)])

  if test x"$samba_cv_HAVE_ADDRTYPE_IN_KRB5_ADDRESS" = x"yes"; then
    AC_DEFINE(HAVE_ADDRTYPE_IN_KRB5_ADDRESS,1,
               [Whether the krb5_address struct has a addrtype property])
  fi

  AC_CACHE_CHECK([for addr_type in krb5_address],
                 samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_address kaddr; kaddr.addr_type = KRB5_ADDRESS_INET;],
      samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS=yes,
      samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS=no)])

  if test x"$samba_cv_HAVE_ADDR_TYPE_IN_KRB5_ADDRESS" = x"yes"; then
    AC_DEFINE(HAVE_ADDR_TYPE_IN_KRB5_ADDRESS,1,
              [Whether the krb5_address struct has a addr_type property])
  fi

  AC_CACHE_CHECK([for enc_part2 in krb5_ticket], 
		 samba_cv_HAVE_KRB5_TKT_ENC_PART2,
                 [AC_TRY_COMPILE([#include <krb5.h>],
    [krb5_ticket tkt; tkt.enc_part2->authorization_data[0]->contents = NULL;],
    samba_cv_HAVE_KRB5_TKT_ENC_PART2=yes,samba_cv_HAVE_KRB5_TKT_ENC_PART2=no)])

  if test x"$samba_cv_HAVE_KRB5_TKT_ENC_PART2" = x"yes"; then
    AC_DEFINE(HAVE_KRB5_TKT_ENC_PART2,1,
              [Whether the krb5_ticket struct has a enc_part2 property])
  fi

  AC_CACHE_CHECK([for keyvalue in krb5_keyblock],
                 samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_keyblock key; key.keyvalue.data = NULL;],
      samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE=yes,
      samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE=no)])

  if test x"$samba_cv_HAVE_KRB5_KEYBLOCK_KEYVALUE" = x"yes"; then
    AC_DEFINE(HAVE_KRB5_KEYBLOCK_KEYVALUE,1,
              [Whether the krb5_keyblock struct has a keyvalue property])
  fi

  AC_CACHE_CHECK([for ENCTYPE_ARCFOUR_HMAC_MD5],
                 samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_enctype enctype; enctype = ENCTYPE_ARCFOUR_HMAC_MD5;],
      samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5=yes,
      samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5=no)])
  AC_CACHE_CHECK([for KEYTYPE_ARCFOUR_56],
                 samba_cv_HAVE_KEYTYPE_ARCFOUR_56,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_keytype keytype; keytype = KEYTYPE_ARCFOUR_56;],
      samba_cv_HAVE_KEYTYPE_ARCFOUR_56=yes,
      samba_cv_HAVE_KEYTYPE_ARCFOUR_56=no)])
# Heimdals with KEYTYPE_ARCFOUR but not KEYTYPE_ARCFOUR_56 are broken
# w.r.t. arcfour and windows, so we must not enable it here
  if test x"$samba_cv_HAVE_ENCTYPE_ARCFOUR_HMAC_MD5" = x"yes" -a\
          x"$samba_cv_HAVE_KEYTYPE_ARCFOUR_56" = x"yes"; then
    AC_DEFINE(HAVE_ENCTYPE_ARCFOUR_HMAC_MD5,1,
              [Whether the ENCTYPE_ARCFOUR_HMAC_MD5 key type is available])
  fi

  AC_CACHE_CHECK([for AP_OPTS_USE_SUBKEY],
                 samba_cv_HAVE_AP_OPTS_USE_SUBKEY,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_flags ap_options; ap_options = AP_OPTS_USE_SUBKEY;],
      samba_cv_HAVE_AP_OPTS_USE_SUBKEY=yes,
      samba_cv_HAVE_AP_OPTS_USE_SUBKEY=no)])

  if test x"$samba_cv_HAVE_AP_OPTS_USE_SUBKEY" = x"yes"; then
    AC_DEFINE(HAVE_AP_OPTS_USE_SUBKEY,1,
              [Whether the AP_OPTS_USE_SUBKEY ap option is available])
  fi

  AC_CACHE_CHECK([for the krb5_princ_component macro],
                samba_cv_HAVE_KRB5_PRINC_COMPONENT,[
    AC_TRY_LINK([#include <krb5.h>],
      [const krb5_data *pkdata; krb5_context context; krb5_principal principal; pkdata = krb5_princ_component(context, principal, 0);],
      samba_cv_HAVE_KRB5_PRINC_COMPONENT=yes,
      samba_cv_HAVE_KRB5_PRINC_COMPONENT=no)])

  if test x"$samba_cv_HAVE_KRB5_PRINC_COMPONENT" = x"yes"; then
    AC_DEFINE(HAVE_KRB5_PRINC_COMPONENT,1,
               [Whether krb5_princ_component is available])
  fi

  AC_CACHE_CHECK([for memory keytab support],
		 samba_cv_HAVE_MEMORY_KEYTAB,[
    AC_TRY_RUN([
#include<krb5.h>
  main()
  {
    krb5_context context;
    krb5_keytab keytab;
    
    krb5_init_context(&context);
    if (krb5_kt_resolve(context, "MEMORY:", &keytab))
      exit(1);
    exit(0);
  }], 
  samba_cv_HAVE_MEMORY_KEYTAB=yes,
  samba_cv_HAVE_MEMORY_KEYTAB=no)])

  if test x"$samba_cv_HAVE_MEMORY_KEYTAB" = x"yes"; then
      AC_DEFINE(HAVE_MEMORY_KEYTAB,1,
               [Whether in-memory keytabs are supported])
  fi

  AC_CACHE_CHECK([for key in krb5_keytab_entry],
                 samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_keytab_entry entry; krb5_keyblock e; entry.key = e;],
      samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY=yes,
      samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY=no)])

  if test x"$samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEY" = x"yes"; then
    AC_DEFINE(HAVE_KRB5_KEYTAB_ENTRY_KEY,1,
              [Whether krb5_keytab_entry has key member])
  fi

  AC_CACHE_CHECK([for keyblock in krb5_keytab_entry],
                 samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK,[
    AC_TRY_COMPILE([#include <krb5.h>],
      [krb5_keytab_entry entry; entry.keyblock.keytype = 0;],
      samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK=yes,
      samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK=no)])

  if test x"$samba_cv_HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK" = x"yes"; then
    AC_DEFINE(HAVE_KRB5_KEYTAB_ENTRY_KEYBLOCK,1,
              [Whether krb5_keytab_entry has keyblock member])
  fi

  if test x"$ac_cv_lib_ext_krb5_krb5_mk_req_extended" = x"yes"; then
    AC_DEFINE(HAVE_KRB5,1,[Whether to have KRB5 support])
    AC_DEFINE(WITH_ADS,1,[Whether to include Active Directory support])
    AC_MSG_CHECKING(whether Active Directory and KRB5 support is used)
    AC_MSG_RESULT(yes)
  else
    if test x"$with_ads_support" = x"yes"; then
	AC_MSG_ERROR(libkrb5 is needed for Active Directory support)
    else
	AC_MSG_WARN(libkrb5 is needed for Active Directory support)
    fi
    KRB5_LIBS=""
    with_ads_support=no 
  fi
  LIBS="$ac_save_LIBS"
fi
