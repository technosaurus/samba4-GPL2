# TORTURE subsystem

#################################
# Start SUBSYSTEM TORTURE_BASIC
[SUBSYSTEM::TORTURE_BASIC]
ADD_OBJ_FILES = \
		torture/basic/scanner.o \
		torture/basic/utable.o \
		torture/basic/charset.o \
		torture/basic/mangle_test.o \
		torture/basic/denytest.o \
		torture/basic/aliases.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB
# End SUBSYSTEM TORTURE_BASIC
#################################

#################################
# Start SUBSYSTEM TORTURE_BASIC
[SUBSYSTEM::TORTURE_RAW]
ADD_OBJ_FILES = \
		torture/raw/qfsinfo.o \
		torture/raw/qfileinfo.o \
		torture/raw/setfileinfo.o \
		torture/raw/search.o \
		torture/raw/close.o \
		torture/raw/open.o \
		torture/raw/mkdir.o \
		torture/raw/oplock.o \
		torture/raw/notify.o \
		torture/raw/mux.o \
		torture/raw/ioctl.o \
		torture/raw/chkpath.o \
		torture/raw/unlink.o \
		torture/raw/read.o \
		torture/raw/context.o \
		torture/raw/write.o \
		torture/raw/lock.o \
		torture/raw/rename.o \
		torture/raw/seek.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB
# End SUBSYSTEM TORTURE_RAW
#################################

#################################
# Start SUBSYSTEM TORTURE_RPC
[SUBSYSTEM::TORTURE_RPC]
ADD_OBJ_FILES = \
		torture/rpc/lsa.o \
		torture/rpc/echo.o \
		torture/rpc/dfs.o \
		torture/rpc/spoolss.o \
		torture/rpc/samr.o \
		torture/rpc/wkssvc.o \
		torture/rpc/srvsvc.o \
		torture/rpc/atsvc.o \
		torture/rpc/eventlog.o \
		torture/rpc/epmapper.o \
		torture/rpc/winreg.o \
		torture/rpc/mgmt.o \
		torture/rpc/scanner.o \
		torture/rpc/autoidl.o \
		torture/rpc/netlogon.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB
# End SUBSYSTEM TORTURE_RPC
#################################

#################################
# Start SUBSYSTEM TORTURE_NBENCH
[SUBSYSTEM::TORTURE_NBENCH]
ADD_OBJ_FILES = \
		torture/nbench/nbio.o \
		torture/nbench/nbench.o
# End SUBSYSTEM TORTURE_NBENCH
#################################

#################################
# Start BINARY smbtorture
[BINARY::smbtorture]
OBJ_FILES = \
		torture/torture.o \
		torture/torture_util.o
REQUIRED_SUBSYSTEMS = \
		TORTURE_BASIC \
		TORTURE_RAW \
		TORTURE_RPC \
		TORTURE_NBENCH \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC
# End BINARY smbtorture
#################################

#################################
# Start BINARY gentest
[BINARY::gentest]
OBJ_FILES = \
		torture/gentest.o \
		torture/torture_util.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB \
		CONFIG \
		LIBBASIC \
		LIBCMDLINE
# End BINARY gentest
#################################

#################################
# Start BINARY masktest
[BINARY::masktest]
OBJ_FILES = \
		torture/masktest.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB \
		CONFIG \
		LIBBASIC \
		LIBCMDLINE
# End BINARY masktest
#################################

#################################
# Start BINARY locktest
[BINARY::locktest]
OBJ_FILES = \
		torture/locktest.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB \
		CONFIG \
		LIBBASIC \
		LIBCMDLINE
# End BINARY locktest
#################################
