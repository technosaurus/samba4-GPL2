# utils subsystem

#################################
# Start BINARY ndrdump
[BINARY::ndrdump]
OBJ_FILES = \
		utils/ndrdump.o
REQUIRED_SUBSYSTEMS = \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC \
		LIBSMB
# End BINARY ndrdump
#################################

#################################
# Start BINARY lookupuuid
[BINARY::lookupuuid]
OBJ_FILES = \
		utils/lookupuuid.o
REQUIRED_SUBSYSTEMS = \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC \
		LIBSMB
# End BINARY lookupuuid
#################################

#################################
# Start BINARY ntlm_auth
[BINARY::ntlm_auth]
OBJ_FILES = \
		utils/ntlm_auth.o
REQUIRED_SUBSYSTEMS = \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC \
		LIBSMB \
		LIBRPC
# End BINARY ntlm_auth
#################################

#################################
# Start BINARY getntacl
[BINARY::getntacl]
OBJ_FILES = \
		utils/getntacl.o
REQUIRED_SUBSYSTEMS = \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC \
		LIBSMB \
		LIBRPC
# End BINARY getntacl
#################################

#################################
# Start BINARY setntacl
[BINARY::setntacl]
OBJ_FILES = \
		utils/setntacl.o
REQUIRED_SUBSYSTEMS = \
		CONFIG \
		LIBCMDLINE \
		LIBBASIC \
		LIBSMB \
		LIBRPC
# End BINARY setntacl
#################################
