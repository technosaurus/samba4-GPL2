/* 
   Unix SMB/CIFS implementation.

   NBT client - used to lookup netbios names

   Copyright (C) Andrew Tridgell 1994-2005
   Copyright (C) Jelmer Vernooij 2003 (Conversion to popt)
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
   
*/

#include "includes.h"
#include "dynconfig.h"
#include "libcli/nbt/libnbt.h"
#include "lib/cmdline/popt_common.h"
#include "system/iconv.h"
#include "lib/socket/socket.h"

/* command line options */
static struct {
	const char *broadcast_address;
	const char *unicast_address;
	BOOL find_master;
	BOOL wins_lookup;
	BOOL node_status;
	BOOL root_port;
	BOOL lookup_by_ip;
	BOOL case_sensitive;
} options;

/*
  clean any binary from a node name
*/
static const char *clean_name(TALLOC_CTX *mem_ctx, const char *name)
{
	char *ret = talloc_strdup(mem_ctx, name);
	int i;
	for (i=0;ret[i];i++) {
		if (!isprint((unsigned char)ret[i])) ret[i] = '.';
	}
	return ret;
}

/*
  turn a node status flags field into a string
*/
static char *node_status_flags(TALLOC_CTX *mem_ctx, uint16_t flags)
{
	char *ret;
	const char *group = "       ";
	const char *type = "B";

	if (flags & NBT_NM_GROUP) {
		group = "<GROUP>";
	}

	switch (flags & NBT_NM_OWNER_TYPE) {
	case NBT_NODE_B: 
		type = "B";
		break;
	case NBT_NODE_P: 
		type = "P";
		break;
	case NBT_NODE_M: 
		type = "M";
		break;
	case NBT_NODE_H: 
		type = "H";
		break;
	}

	ret = talloc_asprintf(mem_ctx, "%s %s", group, type);

	if (flags & NBT_NM_DEREGISTER) {
		ret = talloc_asprintf_append(ret, " <DEREGISTERING>");
	}
	if (flags & NBT_NM_CONFLICT) {
		ret = talloc_asprintf_append(ret, " <CONFLICT>");
	}
	if (flags & NBT_NM_ACTIVE) {
		ret = talloc_asprintf_append(ret, " <ACTIVE>");
	}
	if (flags & NBT_NM_PERMANENT) {
		ret = talloc_asprintf_append(ret, " <PERMANENT>");
	}
	
	return ret;
}

/* do a single node status */
static void do_node_status(struct nbt_name_socket *nbtsock,
			   const char *addr)
{
	struct nbt_name_status io;
	NTSTATUS status;

	io.in.name.name = "*";
	io.in.name.type = NBT_NAME_CLIENT;
	io.in.name.scope = NULL;
	io.in.dest_addr = addr;
	io.in.timeout = 1;
	io.in.retries = 2;

	status = nbt_name_status(nbtsock, nbtsock, &io);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		printf("Node status reply from %s\n",
		       io.out.reply_from);
		for (i=0;i<io.out.status.num_names;i++) {
			d_printf("\t%-16s <%02x>  %s\n", 
				 clean_name(nbtsock, io.out.status.names[i].name),
				 io.out.status.names[i].type,
				 node_status_flags(nbtsock, io.out.status.names[i].nb_flags));
		}
		printf("\n\tMAC Address = %02X-%02X-%02X-%02X-%02X-%02X\n",
		       io.out.status.statistics.unit_id[0],
		       io.out.status.statistics.unit_id[1],
		       io.out.status.statistics.unit_id[2],
		       io.out.status.statistics.unit_id[3],
		       io.out.status.statistics.unit_id[4],
		       io.out.status.statistics.unit_id[5]);
	}
}

/* do a single node query */
static NTSTATUS do_node_query(struct nbt_name_socket *nbtsock,
			      const char *addr, 
			      const char *node_name, 
			      enum nbt_name_type node_type,
			      BOOL broadcast)
{
	struct nbt_name_query io;
	NTSTATUS status;
	int i;

	io.in.name.name = node_name;
	io.in.name.type = node_type;
	io.in.name.scope = NULL;
	io.in.dest_addr = addr;
	io.in.broadcast = broadcast;
	io.in.wins_lookup = options.wins_lookup;
	io.in.timeout = 1;
	io.in.retries = 2;

	status = nbt_name_query(nbtsock, nbtsock, &io);
	NT_STATUS_NOT_OK_RETURN(status);

	for (i=0;i<io.out.num_addrs;i++) {
		printf("%s %s<%02x>\n",
		       io.out.reply_addrs[i],
		       io.out.name.name,
		       io.out.name.type);
	}
	if (options.node_status && io.out.num_addrs > 0) {
		do_node_status(nbtsock, io.out.reply_addrs[0]);
	}

	return status;
}


static void process_one(const char *name)
{
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	enum nbt_name_type node_type = NBT_NAME_CLIENT;
	char *node_name, *p;
	struct nbt_name_socket *nbtsock;
	NTSTATUS status = NT_STATUS_OK;

	if (!options.case_sensitive) {
		name = strupper_talloc(tmp_ctx, name);
	}
	
	if (options.find_master) {
		node_type = NBT_NAME_MASTER;
		if (*name == '-') {
			name = "\01\02__MSBROWSE__\02";
			node_type = NBT_NAME_MS;
		}
	}

	p = strchr(name, '#');
	if (p) {
		node_name = talloc_strndup(tmp_ctx, name, PTR_DIFF(p,name));
		node_type = (enum nbt_name_type)strtol(p+1, NULL, 16);
	} else {
		node_name = talloc_strdup(tmp_ctx, name);
	}

	nbtsock = nbt_name_socket_init(tmp_ctx, NULL);

	if (options.root_port) {
		status = socket_listen(nbtsock->sock, "0.0.0.0", NBT_NAME_SERVICE_PORT, 0, 0);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to bind to local port 137 - %s\n", nt_errstr(status));
			return;
		}
	}

	if (options.lookup_by_ip) {
		do_node_status(nbtsock, name);
		talloc_free(tmp_ctx);
		return;
	}

	if (options.broadcast_address) {
		status = do_node_query(nbtsock, options.broadcast_address, node_name, node_type, True);
	} else if (options.unicast_address) {
		status = do_node_query(nbtsock, options.unicast_address, node_name, node_type, False);
	} else {
		int i, num_interfaces = iface_count();
		for (i=0;i<num_interfaces;i++) {
			const char *bcast = iface_n_bcast(i);
			status = do_node_query(nbtsock, bcast, node_name, node_type, True);
			if (NT_STATUS_IS_OK(status)) break;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		printf("Lookup failed - %s\n", nt_errstr(status));
	}

	talloc_free(tmp_ctx);
 }

/*
  main program
*/
int main(int argc,char *argv[])
{
	poptContext pc;
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "broadcast", 'B', POPT_ARG_STRING, &options.broadcast_address, 
		  'B', "Specify address to use for broadcasts", "BROADCAST-ADDRESS" },

		{ "unicast", 'U', POPT_ARG_STRING, &options.unicast_address, 
		  'U', "Specify address to use for unicast" },

		{ "master-browser", 'M', POPT_ARG_VAL, &options.find_master, 
		  True, "Search for a master browser" },

		{ "wins", 'W', POPT_ARG_VAL, &options.wins_lookup, True, "Do a WINS lookup" },

		{ "status", 'S', POPT_ARG_VAL, &options.node_status, 
		  True, "Lookup node status as well" },

		{ "root-port", 'r', POPT_ARG_VAL, &options.root_port, 
		  True, "Use root port 137 (Win95 only replies to this)" },

		{ "lookup-by-ip", 'A', POPT_ARG_VAL, &options.lookup_by_ip, 
		  True, "Do a node status on <name> as an IP Address" },

		{ "case-sensitive", 0, POPT_ARG_VAL, &options.case_sensitive, 
		  True, "Don't uppercase the name before sending" },

		POPT_COMMON_SAMBA
		{ 0, 0, 0, 0 }
	};
	
	pc = poptGetContext("nmblookup", argc, (const char **)argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);
	
	poptSetOtherOptionHelp(pc, "<NODE> ...");

	while ((poptGetNextOpt(pc) != -1)) /* noop */ ;

	/* swallow argv[0] */
	poptGetArg(pc);

	if(!poptPeekArg(pc)) { 
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}
	
	while (poptPeekArg(pc)) {
		const char *name = poptGetArg(pc);

		process_one(name);
	}

	poptFreeContext(pc);

	return 0;
}
