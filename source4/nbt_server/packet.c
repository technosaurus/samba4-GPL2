/* 
   Unix SMB/CIFS implementation.

   packet utility functions

   Copyright (C) Andrew Tridgell	2005
   
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
#include "dlinklist.h"
#include "nbt_server/nbt_server.h"

/*
  we received a badly formed packet - log it
*/
void nbtd_bad_packet(struct nbt_name_packet *packet, 
		     const char *src_address, const char *reason)
{
	DEBUG(2,("nbtd: bad packet '%s' from %s\n", reason, src_address));
	if (DEBUGLVL(5)) {
		NDR_PRINT_DEBUG(nbt_name_packet, packet);		
	}
}


/*
  see if an incoming packet is a broadcast packet from one of our own
  interfaces
*/
BOOL nbtd_self_packet(struct nbt_name_socket *nbtsock, 
		      struct nbt_name_packet *packet, 
		      const char *src_address, int src_port)
{
	struct nbtd_interface *iface = talloc_get_type(nbtsock->incoming.private, 
						       struct nbtd_interface);
	struct nbtd_server *nbtsrv = iface->nbtsrv;
	
	/* if its not a broadcast then its not considered a self packet */
	if (!(packet->operation & NBT_FLAG_BROADCAST)) {
		return False;
	}

	/* if its not from the nbt port, then it wasn't a broadcast from us */
	if (src_port != lp_nbt_port()) {
		return False;
	}

	/* this uses the fact that iface->nbtsock is our non-broadcast
	   listen address */
	if (iface->nbtsock == nbtsock &&
	    iface != iface->nbtsrv->bcast_interface) {
		return False;
	}

	/* we have to loop over our interface list, seeing if its from
	   one of our own interfaces */
	for (iface=nbtsrv->interfaces;iface;iface=iface->next) {
		if (strcmp(src_address, iface->ip_address) == 0) {
			return True;
		}
	}

	return False;
}


/*
  send a name query reply
*/
void nbtd_name_query_reply(struct nbt_name_socket *nbtsock, 
			   struct nbt_name_packet *request_packet, 
			   const char *src_address, int src_port,
			   struct nbt_name *name, uint32_t ttl,
			   uint16_t nb_flags, const char **addresses)
{
	struct nbt_name_packet *packet;
	size_t num_addresses = str_list_length(addresses);
	int i;

	if (num_addresses == 0) {
		DEBUG(3,("No addresses in name query reply - failing\n"));
		return;
	}

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return;

	packet->name_trn_id = request_packet->name_trn_id;
	packet->ancount = 1;
	packet->operation = 
		NBT_FLAG_REPLY | 
		NBT_OPCODE_QUERY | 
		NBT_FLAG_AUTHORITIVE |
		NBT_FLAG_RECURSION_DESIRED |
		NBT_FLAG_RECURSION_AVAIL;

	packet->answers = talloc_array(packet, struct nbt_res_rec, 1);
	if (packet->answers == NULL) goto failed;

	packet->answers[0].name     = *name;
	packet->answers[0].rr_type  = NBT_QTYPE_NETBIOS;
	packet->answers[0].rr_class = NBT_QCLASS_IP;
	packet->answers[0].ttl      = ttl;
	packet->answers[0].rdata.netbios.length = num_addresses*6;
	packet->answers[0].rdata.netbios.addresses = 
		talloc_array(packet->answers, struct nbt_rdata_address, num_addresses);
	if (packet->answers[0].rdata.netbios.addresses == NULL) goto failed;

	for (i=0;i<num_addresses;i++) {
		struct nbt_rdata_address *addr = 
			&packet->answers[0].rdata.netbios.addresses[i];
		addr->nb_flags = nb_flags;
		addr->ipaddr = talloc_strdup(packet->answers, addresses[i]);
		if (addr->ipaddr == NULL) goto failed;
	}

	DEBUG(7,("Sending name query reply for %s at %s to %s:%d\n", 
		 nbt_name_string(packet, name), addresses[0], src_address, src_port));
	
	nbt_name_reply_send(nbtsock, src_address, src_port, packet);

failed:
	talloc_free(packet);
}


/*
  send a negative name query reply
*/
void nbtd_negative_name_query_reply(struct nbt_name_socket *nbtsock, 
				    struct nbt_name_packet *request_packet, 
				    const char *src_address, int src_port)
{
	struct nbt_name_packet *packet;
	struct nbt_name *name = &request_packet->questions[0].name;

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return;

	packet->name_trn_id = request_packet->name_trn_id;
	packet->ancount = 1;
	packet->operation = 
		NBT_FLAG_REPLY | 
		NBT_OPCODE_QUERY | 
		NBT_FLAG_AUTHORITIVE |
		NBT_RCODE_NAM;

	packet->answers = talloc_array(packet, struct nbt_res_rec, 1);
	if (packet->answers == NULL) goto failed;

	packet->answers[0].name      = *name;
	packet->answers[0].rr_type   = NBT_QTYPE_NULL;
	packet->answers[0].rr_class  = NBT_QCLASS_IP;
	packet->answers[0].ttl       = 0;
	ZERO_STRUCT(packet->answers[0].rdata);

	DEBUG(7,("Sending negative name query reply for %s to %s:%d\n", 
		 nbt_name_string(packet, name), src_address, src_port));
	
	nbt_name_reply_send(nbtsock, src_address, src_port, packet);

failed:
	talloc_free(packet);
}

/*
  send a name defense reply
*/
void nbtd_negative_name_registration_reply(struct nbt_name_socket *nbtsock, 
					   struct nbt_name_packet *request_packet, 
					   const char *src_address, int src_port)
{
	struct nbt_name_packet *packet;
	struct nbt_name *name = &request_packet->questions[0].name;

	packet = talloc_zero(nbtsock, struct nbt_name_packet);
	if (packet == NULL) return;

	packet->name_trn_id = request_packet->name_trn_id;
	packet->ancount = 1;
	packet->operation = 
		NBT_FLAG_REPLY | 
		NBT_OPCODE_REGISTER |
		NBT_FLAG_AUTHORITIVE |
		NBT_FLAG_RECURSION_DESIRED |
		NBT_FLAG_RECURSION_AVAIL |
		NBT_RCODE_ACT;
	
	packet->answers = talloc_array(packet, struct nbt_res_rec, 1);
	if (packet->answers == NULL) goto failed;

	packet->answers[0].name     = *name;
	packet->answers[0].rr_type  = NBT_QTYPE_NETBIOS;
	packet->answers[0].rr_class = NBT_QCLASS_IP;
	packet->answers[0].ttl      = 0;
	packet->answers[0].rdata    = request_packet->additional[0].rdata;

	DEBUG(7,("Sending negative name registration reply for %s to %s:%d\n", 
		 nbt_name_string(packet, name), src_address, src_port));
	
	nbt_name_reply_send(nbtsock, src_address, src_port, packet);

failed:
	talloc_free(packet);
}
