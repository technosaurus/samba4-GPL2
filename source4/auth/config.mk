# AUTH Server subsystem

#######################
# Start MODULE auth_sam
[MODULE::auth_sam]
INIT_FUNCTION = auth_sam_init
INIT_OBJ_FILES = \
		auth/auth_sam.o
REQUIRED_SUBSYSTEMS = \
		SAMDB
# End MODULE auth_sam
#######################

#######################
# Start MODULE auth_builtin
[MODULE::auth_builtin]
INIT_FUNCTION = auth_builtin_init
INIT_OBJ_FILES = \
		auth/auth_builtin.o
# End MODULE auth_builtin
#######################

#######################
# Start MODULE auth_winbind
[MODULE::auth_winbind]
INIT_FUNCTION = auth_winbind_init
INIT_OBJ_FILES = \
		auth/auth_winbind.o
REQUIRED_SUBSYSTEMS = \
		LIB_WINBIND_CLIENT
# End MODULE auth_builtin
#######################

#######################
# Start SUBSYSTEM AUTH
[SUBSYSTEM::AUTH]
INIT_FUNCTION = auth_init
INIT_OBJ_FILES = \
		auth/auth.o
ADD_OBJ_FILES = \
		auth/auth_util.o \
		auth/pampass.o \
		auth/pass_check.o \
		auth/ntlm_check.o
# End SUBSYSTEM AUTH
#######################
