# LIB GTK SMB subsystem

##############################
# Start SUBSYSTEM GTKSMB
[SUBSYSTEM::GTKSMB]
NOPROTO = YES
INIT_OBJ_FILES = gtk/common/gtk-smb.o 
ADD_OBJ_FILES = gtk/common/select.o
REQUIRED_SUBSYSTEMS = CHARSET LIBBASIC EXT_LIB_gtk
# End SUBSYSTEM GTKSMB
##############################

################################################
# Start BINARY gregedit
[BINARY::gregedit]
OBJ_FILES = gtk/tools/gregedit.o
REQUIRED_SUBSYSTEMS = CONFIG LIBCMDLINE REGISTRY GTKSMB
# End BINARY gregedit
################################################

################################################
# Start BINARY gepdump 
[BINARY::gepdump]
OBJ_FILES = gtk/tools/gepdump.o
REQUIRED_SUBSYSTEMS = CONFIG LIBCMDLINE GTKSMB LIBRPC LIBSMB
# End BINARY gepdump 
################################################

################################################
# Start BINARY gwcrontab
[BINARY::gwcrontab]
OBJ_FILES = gtk/tools/gwcrontab.o
REQUIRED_SUBSYSTEMS = CONFIG LIBCMDLINE LIBSMB GTKSMB LIBRPC
# End BINARY gwcrontab
################################################

################################################
# Start BINARY gwsam
[BINARY::gwsam]
OBJ_FILES = gtk/tools/gwsam.o gtk/tools/gwsam_user.o
REQUIRED_SUBSYSTEMS = CONFIG LIBCMDLINE LIBRPC LIBSMB GTKSMB
# End BINARY gwsam
################################################
