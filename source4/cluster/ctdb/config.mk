##################
[SUBSYSTEM::brlock_ctdb]
OBJ_FILES = brlock_ctdb.o

##################
[SUBSYSTEM::ctdb_tcp]
OBJ_FILES = \
		tcp/tcp_init.o \
		tcp/tcp_io.o \
		tcp/tcp_connect.o

##################
[SUBSYSTEM::ctdb]
OBJ_FILES = \
		ctdb_cluster.o \
		common/ctdb.o \
		common/ctdb_call.o \
		common/ctdb_message.o \
		common/ctdb_ltdb.o \
		common/ctdb_util.o
PUBLIC_DEPENDENCIES = LIBTDB LIBTALLOC
PRIVATE_DEPENDENCIES = ctdb_tcp
