[SUBSYSTEM::LIBCLI_UTILS]
ADD_OBJ_FILES = libcli/util/asn1.o \
		libcli/util/doserr.o \
		libcli/util/errormap.o \
		libcli/util/clierror.o \
		libcli/util/nterr.o \
		libcli/util/smbdes.o

[SUBSYSTEM::LIBCLI_LSA]
ADD_OBJ_FILES = libcli/util/clilsa.o
REQUIRED_SUBSYSTEMS = RPC_NDR_LSA

[SUBSYSTEM::LIBCLI_COMPOSITE_BASE]
ADD_OBJ_FILES = \
	libcli/composite/composite.o
REQUIRED_SUBSYSTEMS = LIBEVENTS

[SUBSYSTEM::LIBCLI_COMPOSITE]
ADD_OBJ_FILES = \
	libcli/composite/loadfile.o \
	libcli/composite/savefile.o \
	libcli/composite/connect.o \
	libcli/composite/sesssetup.o \
	libcli/composite/fetchfile.o \
	libcli/composite/appendacl.o \
	libcli/composite/fsinfo.o 
REQUIRED_SUBSYSTEMS = LIBCLI_COMPOSITE_BASE

[SUBSYSTEM::LIBCLI_NBT]
ADD_OBJ_FILES = \
	libcli/nbt/nbtname.o \
	libcli/nbt/nbtsocket.o \
	libcli/nbt/namequery.o \
	libcli/nbt/nameregister.o \
	libcli/nbt/namerefresh.o \
	libcli/nbt/namerelease.o
REQUIRED_SUBSYSTEMS = NDR_RAW NDR_NBT SOCKET LIBCLI_COMPOSITE_BASE LIBEVENTS \
	LIB_SECURITY_NDR

[SUBSYSTEM::LIBCLI_DGRAM]
ADD_OBJ_FILES = \
	libcli/dgram/dgramsocket.o \
	libcli/dgram/mailslot.o \
	libcli/dgram/netlogon.o \
	libcli/dgram/ntlogon.o \
	libcli/dgram/browse.o
NOPROTO=YES
REQUIRED_SUBSYSTEMS = LIBCLI_NBT

[SUBSYSTEM::LIBCLI_CLDAP]
ADD_OBJ_FILES = \
	libcli/cldap/cldap.o
NOPROTO=YES
REQUIRED_SUBSYSTEMS = LIBCLI_LDAP

[SUBSYSTEM::LIBCLI_WINS]
ADD_OBJ_FILES = \
	libcli/wins/winsrepl.o
REQUIRED_SUBSYSTEMS = LIBCLI_NBT NDR_WINS SOCKET LIBEVENTS

[SUBSYSTEM::LIBCLI_RESOLVE]
ADD_OBJ_FILES = \
	libcli/resolve/resolve.o \
	libcli/resolve/nbtlist.o \
	libcli/resolve/bcast.o \
	libcli/resolve/wins.o \
	libcli/resolve/host.o
REQUIRED_SUBSYSTEMS = LIBCLI_NBT

[SUBSYSTEM::LIBCLI]
REQUIRED_SUBSYSTEMS = LIBCLI_RAW LIBCLI_UTILS LIBCLI_AUTH \
	LIBCLI_COMPOSITE LIBCLI_NBT LIB_SECURITY LIBCLI_RESOLVE \
	LIBCLI_DGRAM

[SUBSYSTEM::LIBSMB]
REQUIRED_SUBSYSTEMS = LIBCLI SOCKET
ADD_OBJ_FILES = libcli/clireadwrite.o \
		libcli/cliconnect.o \
		libcli/clifile.o \
		libcli/clilist.o \
		libcli/clitrans2.o \
		libcli/climessage.o \
		libcli/clideltree.o
