/*
   Unix SMB/CIFS implementation.
   DCOM interface and class tables
   Copyright (C) 2004 Jelmer Vernooij <jelmer@samba.org>

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

static struct class_list {
	struct class_list *prev, *next;
	struct dcom_class class;
} *classes = NULL;

static struct interface_list {
	struct interface_list *prev, *next;
	struct dcom_interface interface;
} *interfaces = NULL;

const struct dcom_interface *dcom_interface_by_iid(const struct GUID *iid)
{
	struct interface_list *l = interfaces;

	while(l) {
		
		if (GUID_equal(iid, &l->interface.iid)) 
			return &l->interface;
		
		l = l->next;
	}
	
	return NULL;
}

const struct dcom_class *dcom_class_by_clsid(const struct GUID *clsid)
{
	struct class_list *c = classes;

	while(c) {

		if (GUID_equal(clsid, &c->class.clsid)) {
			return &c->class;
		}

		c = c->next;
	}
	
	return NULL;
}

const void *dcom_proxy_vtable_by_iid(const struct GUID *iid)
{
	const struct dcom_interface *iface = dcom_interface_by_iid(iid);

	if (!iface) { 
		return NULL;
	}

	return iface->proxy_vtable;
}

NTSTATUS dcom_register_interface(const void *_iface)
{
	const struct dcom_interface *iface = _iface;
	struct interface_list *l;
	TALLOC_CTX *lcl_ctx = talloc_init("dcom_register_interface");

	DEBUG(5, ("Adding DCOM interface %s\n", GUID_string(lcl_ctx, &iface->iid)));

	talloc_free(lcl_ctx);
	
	l = talloc_zero(interfaces?interfaces:talloc_autofree_context(), 
			  struct interface_list);

	l->interface = *iface;

	DLIST_ADD(interfaces, l);
	
	return NT_STATUS_OK;
}

NTSTATUS dcom_register_class(const void *_class)
{
	const struct dcom_class *class = _class;
	struct class_list *l = talloc_zero(classes?classes:talloc_autofree_context(), 
					     struct class_list);

	l->class = *class;
	
	DLIST_ADD(classes, l);
	
	return NT_STATUS_OK;
}
