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
		torture/basic/aliases.o \
		torture/basic/locking.o \
		torture/basic/secleak.o \
		torture/basic/rename.o \
		torture/basic/dir.o \
		torture/basic/delete.o \
		torture/basic/unlink.o \
		torture/basic/disconnect.o \
		torture/basic/delaywrite.o \
		torture/basic/attr.o \
		torture/basic/properties.o 
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
		torture/raw/eas.o \
		torture/raw/streams.o \
		torture/raw/acls.o \
		torture/raw/seek.o \
		torture/raw/composite.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB LIBCLI_LSA
# End SUBSYSTEM TORTURE_RAW
#################################

#################################
# Start SUBSYSTEM TORTURE_DCOM
[SUBSYSTEM::TORTURE_DCOM]
ADD_OBJ_FILES = \
		torture/dcom/simple.o
REQUIRED_SUBSYSTEMS = \
		LIBDCOM
# End SUBSYSTEM TORTURE_DCOM
#################################

#################################
# Start SUBSYSTEM TORTURE_RPC
[SUBSYSTEM::TORTURE_RPC]
ADD_OBJ_FILES = \
		torture/rpc/lsa.o \
		torture/rpc/echo.o \
		torture/rpc/dcom.o \
		torture/rpc/dfs.o \
		torture/rpc/drsuapi.o \
		torture/rpc/spoolss.o \
		torture/rpc/samr.o \
		torture/rpc/wkssvc.o \
		torture/rpc/srvsvc.o \
		torture/rpc/svcctl.o \
		torture/rpc/atsvc.o \
		torture/rpc/eventlog.o \
		torture/rpc/epmapper.o \
		torture/rpc/winreg.o \
		torture/rpc/oxidresolve.o \
		torture/rpc/remact.o \
		torture/rpc/mgmt.o \
		torture/rpc/scanner.o \
		torture/rpc/autoidl.o \
		torture/rpc/countcalls.o \
		torture/rpc/testjoin.o \
		torture/rpc/xplogin.o \
		torture/rpc/schannel.o \
		torture/rpc/netlogon.o \
		torture/rpc/samlogon.o \
		torture/rpc/samsync.o \
		torture/rpc/rot.o \
		torture/rpc/bind.o \
		torture/rpc/dssetup.o \
		torture/rpc/alter_context.o
REQUIRED_SUBSYSTEMS = \
		NDR_ALL RPC_NDR_SAMR RPC_NDR_WINREG RPC_NDR_OXIDRESOLVER \
		RPC_NDR_EVENTLOG RPC_NDR_ECHO RPC_NDR_SVCCTL \
		RPC_NDR_MGMT RPC_NDR_NETLOGON RPC_NDR_ATSVC RPC_NDR_DRSUAPI \
		RPC_NDR_LSA RPC_NDR_EPMAPPER RPC_NDR_DFS RPC_NDR_SPOOLSS \
		RPC_NDR_SRVSVC RPC_NDR_WKSSVC RPC_NDR_ROT RPC_NDR_DSSETUP
# End SUBSYSTEM TORTURE_RPC
#################################

#################################
# Start SUBSYSTEM TORTURE_RAP
[SUBSYSTEM::TORTURE_RAP]
ADD_OBJ_FILES = \
		torture/rap/rap.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB
# End SUBSYSTEM TORTURE_RAP
#################################

#################################
# Start SUBSYSTEM TORTURE_AUTH
[SUBSYSTEM::TORTURE_AUTH]
ADD_OBJ_FILES = \
		torture/auth/ntlmssp.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB
# End SUBSYSTEM TORTURE_AUTH
#################################

#################################
# Start SUBSYSTEM TORTURE_LOCAL
[SUBSYSTEM::TORTURE_LOCAL]
ADD_OBJ_FILES = \
		torture/local/iconv.o \
		lib/talloc/testsuite.o \
		torture/local/messaging.o \
		torture/local/binding_string.o \
		torture/local/idtree.o \
		torture/local/socket.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB \
		MESSAGING
# End SUBSYSTEM TORTURE_LOCAL
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
# Start SUBSYSTEM TORTURE_LDAP
[SUBSYSTEM::TORTURE_LDAP]
ADD_OBJ_FILES = \
		torture/ldap/common.o \
		torture/ldap/basic.o
REQUIRED_SUBSYSTEMS = \
		LIBCLI_LDAP
# End SUBSYSTEM TORTURE_LDAP
#################################

#################################
# Start SUBSYSTEM TORTURE_NBT
[SUBSYSTEM::TORTURE_NBT]
ADD_OBJ_FILES = \
		torture/nbt/query.o \
		torture/nbt/register.o \
		torture/nbt/wins.o \
		torture/nbt/winsbench.o \
		torture/nbt/winsreplication.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB LIBCLI_WINS
# End SUBSYSTEM TORTURE_NBT
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
		TORTURE_DCOM \
		TORTURE_RAP \
		TORTURE_AUTH \
		TORTURE_LOCAL \
		TORTURE_NBENCH \
		TORTURE_LDAP \
		TORTURE_NBT \
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
		LIBCMDLINE \
		LIBRPC
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
		LIBCMDLINE \
		LIBRPC
# End BINARY masktest
#################################

#################################
# Start BINARY locktest
[BINARY::locktest]
OBJ_FILES = \
		torture/locktest.o \
		torture/torture_util.o
REQUIRED_SUBSYSTEMS = \
		LIBSMB \
		CONFIG \
		LIBBASIC \
		LIBCMDLINE \
		LIBRPC
# End BINARY locktest
#################################
