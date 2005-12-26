[SUBSYSTEM::COM]
INIT_FUNCTION = com_init
OBJ_FILES = \
		tables.o \
		rot.o \
		main.o

[SUBSYSTEM::DCOM]
OBJ_FILES = \
		dcom/main.o \
		dcom/tables.o
REQUIRED_SUBSYSTEMS = COM DCOM_PROXY_DCOM RPC_NDR_REMACT \
					  RPC_NDR_OXIDRESOLVER

[MODULE::com_simple]
SUBSYSTEM = COM
OBJ_FILES = classes/simple.o
INIT_FUNCTION = com_simple_init
