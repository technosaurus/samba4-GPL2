[SUBSYSTEM::LIBSMB]
REQUIRED_SUBSYSTEMS = LIBCLI SOCKET
ADD_OBJ_FILES = libcli/clireadwrite.o \
		libcli/cliconnect.o \
		libcli/clifile.o \
		libcli/clilist.o \
		libcli/clitrans2.o \
		libcli/climessage.o \
		libcli/clideltree.o
