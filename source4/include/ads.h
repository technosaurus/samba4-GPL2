/*
   Unix SMB/CIFS implementation.
   header for ads (active directory) library routines
   basically this is a wrapper around ldap

   Copyright (C) Andrew Tridgell 2001-2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* ldap control oids */
#define ADS_PAGE_CTL_OID "1.2.840.113556.1.4.319"
#define ADS_NO_REFERRALS_OID "1.2.840.113556.1.4.1339"
#define ADS_SERVER_SORT_OID "1.2.840.113556.1.4.473"
#define ADS_PERMIT_MODIFY_OID "1.2.840.113556.1.4.1413"
/*
1.2.840.113556.1.4.319;
1.2.840.113556.1.4.801;
1.2.840.113556.1.4.473;
1.2.840.113556.1.4.528;
1.2.840.113556.1.4.417;
1.2.840.113556.1.4.619;
1.2.840.113556.1.4.841;
1.2.840.113556.1.4.529;
1.2.840.113556.1.4.805;
1.2.840.113556.1.4.521;
1.2.840.113556.1.4.970;
1.2.840.113556.1.4.1338;
1.2.840.113556.1.4.474;
1.2.840.113556.1.4.1339;
1.2.840.113556.1.4.1340;
1.2.840.113556.1.4.1413;
2.16.840.1.113730.3.4.9;
2.16.840.1.113730.3.4.10;
1.2.840.113556.1.4.1504;
1.2.840.113556.1.4.1852;
1.2.840.113556.1.4.802; 
*/
/* UserFlags for userAccountControl */
#define UF_SCRIPT	 			0x00000001
#define UF_ACCOUNTDISABLE			0x00000002
#define UF_00000004	 			0x00000004
#define UF_HOMEDIR_REQUIRED			0x00000008

#define UF_LOCKOUT	 			0x00000010
#define UF_PASSWD_NOTREQD 			0x00000020
#define UF_PASSWD_CANT_CHANGE 			0x00000040
#define UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED	0x00000080

#define UF_TEMP_DUPLICATE_ACCOUNT       	0x00000100
#define UF_NORMAL_ACCOUNT               	0x00000200
#define UF_00000400	 			0x00000400
#define UF_INTERDOMAIN_TRUST_ACCOUNT    	0x00000800

#define UF_WORKSTATION_TRUST_ACCOUNT    	0x00001000
#define UF_SERVER_TRUST_ACCOUNT         	0x00002000
#define UF_00004000	 			0x00004000
#define UF_00008000	 			0x00008000

#define UF_DONT_EXPIRE_PASSWD			0x00010000
#define UF_MNS_LOGON_ACCOUNT			0x00020000
#define UF_SMARTCARD_REQUIRED			0x00040000
#define UF_TRUSTED_FOR_DELEGATION		0x00080000

#define UF_NOT_DELEGATED			0x00100000
#define UF_USE_DES_KEY_ONLY			0x00200000
#define UF_DONT_REQUIRE_PREAUTH			0x00400000
#define UF_PASSWORD_EXPIRED			0x00800000

#define UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION 0x01000000
#define UF_NO_AUTH_DATA_REQUIRED                0x02000000
#define UF_UNUSED_8				0x04000000
#define UF_UNUSED_9				0x08000000

#define UF_UNUSED_10				0x10000000
#define UF_UNUSED_11				0x20000000
#define UF_UNUSED_12				0x40000000
#define UF_UNUSED_13				0x80000000

#define UF_MACHINE_ACCOUNT_MASK (\
		UF_INTERDOMAIN_TRUST_ACCOUNT |\
		UF_WORKSTATION_TRUST_ACCOUNT |\
		UF_SERVER_TRUST_ACCOUNT \
		)

#define UF_ACCOUNT_TYPE_MASK (\
		UF_TEMP_DUPLICATE_ACCOUNT |\
		UF_NORMAL_ACCOUNT |\
		UF_INTERDOMAIN_TRUST_ACCOUNT |\
		UF_WORKSTATION_TRUST_ACCOUNT |\
		UF_SERVER_TRUST_ACCOUNT \
                )

#define UF_SETTABLE_BITS (\
		UF_SCRIPT |\
		UF_ACCOUNTDISABLE |\
		UF_HOMEDIR_REQUIRED  |\
		UF_LOCKOUT |\
		UF_PASSWD_NOTREQD |\
		UF_PASSWD_CANT_CHANGE |\
		UF_ACCOUNT_TYPE_MASK | \
		UF_DONT_EXPIRE_PASSWD | \
		UF_MNS_LOGON_ACCOUNT |\
		UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED |\
		UF_SMARTCARD_REQUIRED |\
		UF_TRUSTED_FOR_DELEGATION |\
		UF_NOT_DELEGATED |\
		UF_USE_DES_KEY_ONLY  |\
		UF_DONT_REQUIRE_PREAUTH \
		)

/* sAMAccountType */
#define ATYPE_NORMAL_ACCOUNT			0x30000000 /* 805306368 */
#define ATYPE_WORKSTATION_TRUST			0x30000001 /* 805306369 */
#define ATYPE_INTERDOMAIN_TRUST			0x30000002 /* 805306370 */ 
#define ATYPE_SECURITY_GLOBAL_GROUP		0x10000000 /* 268435456 */
#define ATYPE_DISTRIBUTION_GLOBAL_GROUP		0x10000001 /* 268435457 */
#define ATYPE_DISTRIBUTION_UNIVERSAL_GROUP 	ATYPE_DISTRIBUTION_GLOBAL_GROUP
#define ATYPE_SECURITY_LOCAL_GROUP		0x20000000 /* 536870912 */
#define ATYPE_DISTRIBUTION_LOCAL_GROUP		0x20000001 /* 536870913 */

#define ATYPE_ACCOUNT		ATYPE_NORMAL_ACCOUNT 		/* 0x30000000 805306368 */
#define ATYPE_GLOBAL_GROUP	ATYPE_SECURITY_GLOBAL_GROUP 	/* 0x10000000 268435456 */
#define ATYPE_LOCAL_GROUP	ATYPE_SECURITY_LOCAL_GROUP 	/* 0x20000000 536870912 */

/* groupType */
#define GROUP_TYPE_BUILTIN_LOCAL_GROUP		0x00000001
#define GROUP_TYPE_ACCOUNT_GROUP		0x00000002
#define GROUP_TYPE_RESOURCE_GROUP		0x00000004
#define GROUP_TYPE_UNIVERSAL_GROUP		0x00000008
#define GROUP_TYPE_APP_BASIC_GROUP		0x00000010
#define GROUP_TYPE_APP_QUERY_GROUP		0x00000020
#define GROUP_TYPE_SECURITY_ENABLED		0x80000000

#define GTYPE_SECURITY_BUILTIN_LOCAL_GROUP ( \
		/* 0x80000005 -2147483643 */ \
		GROUP_TYPE_BUILTIN_LOCAL_GROUP| \
		GROUP_TYPE_RESOURCE_GROUP| \
		GROUP_TYPE_SECURITY_ENABLED \
		)
#define GTYPE_SECURITY_DOMAIN_LOCAL_GROUP ( \
		/* 0x80000004 -2147483644 */ \
		GROUP_TYPE_RESOURCE_GROUP| \
		GROUP_TYPE_SECURITY_ENABLED \
		)
#define GTYPE_SECURITY_GLOBAL_GROUP ( \
		/* 0x80000002 -2147483646 */ \
		GROUP_TYPE_ACCOUNT_GROUP| \
		GROUP_TYPE_SECURITY_ENABLED \
		)
#define GTYPE_DISTRIBUTION_GLOBAL_GROUP		0x00000002	/* 2 */
#define GTYPE_DISTRIBUTION_DOMAIN_LOCAL_GROUP	0x00000004	/* 4 */
#define GTYPE_DISTRIBUTION_UNIVERSAL_GROUP	0x00000008	/* 8 */

/* Mailslot or cldap getdcname response flags */
#define ADS_PDC            0x00000001  /* DC is PDC */
#define ADS_GC             0x00000004  /* DC is a GC of forest */
#define ADS_LDAP           0x00000008  /* DC is an LDAP server */
#define ADS_DS             0x00000010  /* DC supports DS */
#define ADS_KDC            0x00000020  /* DC is running KDC */
#define ADS_TIMESERV       0x00000040  /* DC is running time services */
#define ADS_CLOSEST        0x00000080  /* DC is closest to client */
#define ADS_WRITABLE       0x00000100  /* DC has writable DS */
#define ADS_GOOD_TIMESERV  0x00000200  /* DC has hardware clock
	  				 (and running time) */
#define ADS_NDNC           0x00000400  /* DomainName is non-domain NC serviced
	  				 by LDAP server */
#define ADS_PINGS          0x0000FFFF  /* Ping response */
#define ADS_DNS_CONTROLLER 0x20000000  /* DomainControllerName is a DNS name*/
#define ADS_DNS_DOMAIN     0x40000000  /* DomainName is a DNS name */
#define ADS_DNS_FOREST     0x80000000  /* DnsForestName is a DNS name */

/* DomainCntrollerAddressType */
#define ADS_INET_ADDRESS      0x00000001
#define ADS_NETBIOS_ADDRESS   0x00000002


/* ads auth control flags */
#define ADS_AUTH_DISABLE_KERBEROS 0x01
#define ADS_AUTH_NO_BIND          0x02
#define ADS_AUTH_ANON_BIND        0x04
#define ADS_AUTH_SIMPLE_BIND      0x08
#define ADS_AUTH_ALLOW_NTLMSSP    0x10

/* Kerberos environment variable names */
#define KRB5_ENV_CCNAME "KRB5CCNAME"

/* Heimdal uses a slightly different name */
#if defined(HAVE_ENCTYPE_ARCFOUR_HMAC_MD5)
#define ENCTYPE_ARCFOUR_HMAC ENCTYPE_ARCFOUR_HMAC_MD5
#endif

#define INSTANCE_TYPE_IS_NC_HEAD	0x00000001
#define INSTANCE_TYPE_UNINSTANT		0x00000002
#define INSTANCE_TYPE_WRITE		0x00000004
#define INSTANCE_TYPE_NC_ABOVE		0x00000008
#define INSTANCE_TYPE_NC_COMING		0x00000010
#define INSTANCE_TYPE_NC_GOING		0x00000020

#define SYSTEM_FLAG_CR_NTDS_NC			0x00000001
#define SYSTEM_FLAG_CR_NTDS_DOMAIN		0x00000002
#define SYSTEM_FLAG_CR_NTDS_NOT_GC_REPLICATED	0x00000004
#define SYSTEM_FLAG_SCHEMA_BASE_OBJECT		0x00000010
#define SYSTEM_FLAG_DISALLOW_MOVE_ON_DELETE	0x02000000
#define SYSTEM_FLAG_DOMAIN_DISALLOW_MOVE	0x04000000
#define SYSTEM_FLAG_DOMAIN_DISALLOW_RENAME	0x08000000
#define SYSTEM_FLAG_CONFIG_ALLOW_LIMITED_MOVE	0x10000000
#define SYSTEM_FLAG_CONFIG_ALLOW_MOVE		0x20000000
#define SYSTEM_FLAG_CONFIG_ALLOW_ERNAME		0x20000000
#define SYSTEM_FLAG_DISALLOW_DELTE		0x80000000

#define DS_BEHAVIOR_WIN2000	0
#define DS_BEHAVIOR_WIN2003	2
