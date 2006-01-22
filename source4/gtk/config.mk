# LIB GTK SMB subsystem

[LIBRARY::GTK_SAMBA]
MAJOR_VERSION = 0
DESCRIPTION = Common Samba-related widgets for GTK+ applications
MINOR_VERSION = 0
RELEASE_VERSION = 1
NOPROTO = YES
PUBLIC_HEADERS = common/gtk-smb.h common/select.h
OBJ_FILES = common/gtk-smb.o \
		common/select.o \
		common/gtk_events.o \
		common/credentials.o
REQUIRED_SUBSYSTEMS = CHARSET LIBBASIC EXT_LIB_gtk RPC_NDR_SAMR

[BINARY::gregedit]
INSTALLDIR = BINDIR
OBJ_FILES = tools/gregedit.o
REQUIRED_SUBSYSTEMS = CONFIG REGISTRY GTK_SAMBA
MANPAGE = man/gregedit.1

[BINARY::gepdump]
INSTALLDIR = BINDIR
MANPAGE = man/gepdump.1
OBJ_FILES = tools/gepdump.o
REQUIRED_SUBSYSTEMS = CONFIG GTK_SAMBA RPC_NDR_EPMAPPER RPC_NDR_MGMT

[BINARY::gwcrontab]
INSTALLDIR = BINDIR
MANPAGE = man/gwcrontab.1
OBJ_FILES = tools/gwcrontab.o
REQUIRED_SUBSYSTEMS = CONFIG GTK_SAMBA RPC_NDR_ATSVC

# This binary is disabled for now as it doesn't do anything useful yet...
[BINARY::gwsam]
#INSTALLDIR = BINDIR
OBJ_FILES = tools/gwsam.o tools/gwsam_user.o
REQUIRED_SUBSYSTEMS = CONFIG RPC_NDR_SAMR GTK_SAMBA
