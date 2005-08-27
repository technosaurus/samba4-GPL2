/* 
   ldb database library - map backend

   Copyright (C) Jelmer Vernooij 2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "includes.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_private.h"
#include "lib/ldb/ldb_map/ldb_map.h"

/* TODO:
 *  - objectclass hint in ldb_map_attribute 
 *     for use when multiple remote attributes (independant of each other)
 *     map to one local attribute. E.g.: (uid, gidNumber) -> unixName
 */

struct map_private {
	const struct ldb_map_attribute *attribute_maps;
	const struct ldb_map_objectclass *objectclass_maps;
	const char *last_err_string;
};

/* find an attribute by the local name */
static const struct ldb_map_attribute *map_find_attr_local(struct ldb_module *module, const char *attr)
{
	struct map_private *privdat = module->private_data;
	int i;
	for (i = 0; privdat->attribute_maps[i].local_name; i++) {
		if (!strcmp(privdat->attribute_maps[i].local_name, attr)) 
			return &privdat->attribute_maps[i];
	}

	return NULL;
}

/* find an attribute by the remote name */
static const struct ldb_map_attribute *map_find_attr_remote(struct ldb_module *module, const char *attr)
{
	struct map_private *privdat = module->private_data;
	int i;
	for (i = 0; privdat->attribute_maps[i].local_name; i++) {
		if (privdat->attribute_maps[i].type != MAP_RENAME &&
			privdat->attribute_maps[i].type != MAP_CONVERT) 
			continue;

		if (!strcmp(privdat->attribute_maps[i].u.rename.remote_name, attr)) 
			return &privdat->attribute_maps[i];
	}

	return NULL;
}

static struct ldb_parse_tree *ldb_map_parse_tree(struct ldb_module *module, TALLOC_CTX *ctx, const struct ldb_parse_tree *tree)
{
	int i;
	const struct ldb_map_attribute *attr;
	struct ldb_parse_tree *new_tree;

	if (tree == NULL)
		return NULL;
	
	new_tree = talloc_memdup(ctx, tree, sizeof(*tree));

	/* Find attr in question and:
	 *  - if it has a convert_operator function, run that
	 *  - otherwise, replace attr name with required[0] */

	if (tree->operation == LDB_OP_AND || 
		tree->operation == LDB_OP_OR) {
		new_tree->u.list.elements = talloc_array(new_tree, struct ldb_parse_tree *, tree->u.list.num_elements);
		for (i = 0; i < new_tree->u.list.num_elements; i++) {
			new_tree->u.list.elements[i] = ldb_map_parse_tree(module, new_tree, tree->u.list.elements[i]);
		}

		return new_tree;
	}
		
	if (tree->operation == LDB_OP_NOT) {
		new_tree->u.isnot.child = ldb_map_parse_tree(module, new_tree, tree->u.isnot.child);
		return new_tree;
	}

	/* tree->operation is LDB_OP_EQUALITY, LDB_OP_SUBSTRING, LDB_OP_GREATER,
	 * LDB_OP_LESS, LDB_OP_APPROX, LDB_OP_PRESENT or LDB_OP_EXTENDED
	 *
	 * (all have attr as the first element)
	 */

	if (new_tree->operation == LDB_OP_EQUALITY) {
		new_tree->u.equality.value = ldb_val_dup(new_tree, &tree->u.equality.value);
	} else if (new_tree->operation == LDB_OP_SUBSTRING) {
		new_tree->u.substring.chunks = NULL; /* FIXME! */
	} else if (new_tree->operation == LDB_OP_LESS || 
			   new_tree->operation == LDB_OP_GREATER ||
			   new_tree->operation == LDB_OP_APPROX) {
		new_tree->u.comparison.value = ldb_val_dup(new_tree, &tree->u.comparison.value);
	} else if (new_tree->operation == LDB_OP_EXTENDED) {
		new_tree->u.extended.value = ldb_val_dup(new_tree, &tree->u.extended.value);
		new_tree->u.extended.rule_id = talloc_strdup(new_tree, tree->u.extended.rule_id);
	}

	attr = map_find_attr_local(module, tree->u.equality.attr);

	if (!attr) {
		DEBUG(0, ("Unable to find local attribute '%s', leaving as is\n", tree->u.equality.attr));
		return new_tree;
	}

	if (attr->type == MAP_IGNORE) {
		talloc_free(new_tree);
		return NULL;
	}

	if (attr->convert_operator) {
		/* Run convert_operator */
		talloc_free(new_tree);
		new_tree = attr->convert_operator(module, tree);

		return new_tree;
	} else {
		new_tree->u.equality.attr = talloc_strdup(new_tree, attr->u.rename.remote_name);
	}
	
	return new_tree;
}

/* Remote DN -> Local DN */
static struct ldb_dn *map_remote_dn(struct ldb_module *module, const struct ldb_dn *dn)
{
	struct ldb_dn *newdn;
	int i;

	if (dn == NULL)
		return NULL;

	newdn = talloc_memdup(module, dn, sizeof(*dn));
	if (!newdn) 
		return NULL;

	newdn->components = talloc_array(newdn, struct ldb_dn_component, newdn->comp_num); 

	if (!newdn->components)
		return NULL;

	/* For each rdn, map the attribute name and possibly the 
	 * complete rdn */
	
	for (i = 0; i < dn->comp_num; i++) {
		const struct ldb_map_attribute *attr = map_find_attr_remote(module, dn->components[i].name);

		/* Unknown attribute - leave this dn as is and hope the best... */
		if (!attr || attr->type == MAP_KEEP) {
			newdn->components[i].name = talloc_strdup(newdn->components, dn->components[i].name);
			newdn->components[i].value = ldb_val_dup(newdn->components, &dn->components[i].value);
		} else if (attr->type == MAP_IGNORE || attr->type == MAP_GENERATE) {
			DEBUG(0, ("Local MAP_IGNORE or MAP_GENERATE attribute '%s' used in DN!", dn->components[i].name));
			talloc_free(newdn);
			return NULL;
		} else if (attr->type == MAP_CONVERT) {
			struct ldb_message_element elm, *newelm;
			struct ldb_val vals[1] = { dn->components[i].value };
			
			elm.flags = 0;
			elm.name = attr->u.convert.remote_name;
			elm.num_values = 1;
			elm.values = vals;

			newelm = attr->u.convert.convert_remote(module, attr->local_name, &elm);

			newdn->components[i].name = talloc_strdup(newdn->components, newelm->name);
			newdn->components[i].value = ldb_val_dup(newdn->components, &newelm->values[0]);

			talloc_free(newelm);
		} else if (attr->type == MAP_RENAME) {
			newdn->components[i].name = talloc_strdup(newdn->components, attr->local_name);
			newdn->components[i].value = ldb_val_dup(newdn->components, &dn->components[i].value);
		}
	}
	return newdn;
}

/* Local DN -> Remote DN */
static struct ldb_dn *map_local_dn(struct ldb_module *module, const struct ldb_dn *dn)
{	struct ldb_dn *newdn;
	int i;
	struct ldb_parse_tree eqtree, *new_eqtree;

	if (dn == NULL)
		return NULL;

	newdn = talloc_memdup(module, dn, sizeof(*dn));
	if (!newdn) 
		return NULL;

	newdn->components = talloc_array(newdn, struct ldb_dn_component, newdn->comp_num); 

	if (!newdn->components)
		return NULL;

	/* For each rdn, map the attribute name and possibly the 
	 * complete rdn using an equality convert_operator call */
	
	for (i = 0; i < dn->comp_num; i++) {
		const struct ldb_map_attribute *attr = map_find_attr_local(module, dn->components[i].name);

		/* Unknown attribute - leave this dn as is and hope the best... */
		if (!attr || attr->type == MAP_KEEP) {
			newdn->components[i].name = talloc_strdup(newdn->components, dn->components[i].name);
			newdn->components[i].value = ldb_val_dup(newdn->components, &dn->components[i].value);
			continue;
		} else if (attr->type == MAP_IGNORE || attr->type == MAP_GENERATE) {
			DEBUG(0, ("Local MAP_IGNORE/MAP_GENERATE attribute '%s' used in DN!", dn->components[i].name));
			talloc_free(newdn);
			return NULL;
		}

		/* Simple rename/convert only */
		if (attr->convert_operator) {
			/* Fancy stuff */
			eqtree.operation = LDB_OP_EQUALITY;
			eqtree.u.equality.attr = dn->components[i].name;
			eqtree.u.equality.value = dn->components[i].value;

			new_eqtree = ldb_map_parse_tree(module, newdn, &eqtree);

			/* Silently continue for now */
			if (!new_eqtree) {
				DEBUG(0, ("Unable to convert RDN for attribute %s\n", dn->components[i].name));
				continue;
			}

			newdn->components[i].name = talloc_strdup(newdn->components, new_eqtree->u.equality.attr);
			newdn->components[i].value = ldb_val_dup(newdn->components, &new_eqtree->u.equality.value);

			talloc_free(new_eqtree);
		} else if (attr->type == MAP_CONVERT) {
			struct ldb_message_element elm, *newelm;
			struct ldb_val vals[1] = { dn->components[i].value };
			elm.flags = 0;
			elm.name = attr->local_name;
			elm.num_values = 1;
			elm.values = vals;

			newelm = attr->u.convert.convert_local(module, attr->u.convert.remote_name, &elm);

			newdn->components[i].name = talloc_strdup(newdn->components, newelm->name);
			newdn->components[i].value = ldb_val_dup(newdn->components, &newelm->values[0]);

			talloc_free(newelm);
		} else if (attr->type == MAP_RENAME) {
			newdn->components[i].name = talloc_strdup(newdn->components, attr->u.rename.remote_name);
			newdn->components[i].value = ldb_val_dup(newdn->components, &dn->components[i].value);
		}
	}

	return newdn;
}

/* Loop over ldb_map_attribute array and add remote_names */
static const char **ldb_map_attrs(struct ldb_module *module, const char *const attrs[])
{
	int i;
	const char **ret;
	int ar_size = 0, last_element = 0;

	if (attrs == NULL) 
		return NULL;

	/* Start with good guess of number of elements */
	for (i = 0; attrs[i]; i++);

	ret = talloc_array(module, const char *, i);
	ar_size = i;

	for (i = 0; attrs[i]; i++) {
		int j;
		const struct ldb_map_attribute *attr = map_find_attr_local(module, attrs[i]);

		if (!attr) {
			DEBUG(0, ("Local attribute '%s' does not have a definition!\n", attrs[i]));
			continue;
		}

		switch (attr->type)
		{ 
			case MAP_IGNORE: break;
			case MAP_KEEP: 
				if (last_element >= ar_size) {
					ret = talloc_realloc(module, ret, const char *, ar_size+1);
					ar_size++;
				}
				ret[last_element] = attr->local_name;
				last_element++;
				break;

			case MAP_RENAME:
			case MAP_CONVERT:
				if (last_element >= ar_size) {
					ret = talloc_realloc(module, ret, const char *, ar_size+1);
					ar_size++;
				}
				ret[last_element] = attr->u.rename.remote_name;
				last_element++;
				break;

			case MAP_GENERATE:
				/* Add remote_names[] for this attribute to the list of 
				 * attributes to request from the remote server */
				for (j = 0; attr->u.generate.remote_names[j]; j++) {
					if (last_element >= ar_size) {
						ret = talloc_realloc(module, ret, const char *, ar_size+1);
						ar_size++;
					}
					ret[last_element] = attr->u.generate.remote_names[j];			
					last_element++;
				}
				break;
		} 
	}

	return NULL;
}

static const char **available_local_attributes(struct ldb_module *module, const struct ldb_message *msg)
{
	struct map_private *map = module->private_data;
	int i, j;
	int count = 0;
	const char **ret = talloc_array(module, const char *, 1);

	ret[0] = NULL;

	for (i = 0; map->attribute_maps[i].local_name; i++) {
		BOOL avail = False;
		const struct ldb_map_attribute *attr = &map->attribute_maps[i];

		/* If all remote attributes for this attribute are present, add the 
		 * local one to the list */
		
		switch (attr->type) {
		case MAP_IGNORE: break;
		case MAP_KEEP: 
				avail = (ldb_msg_find_ldb_val(msg, attr->local_name) != NULL); 
				break;
				
		case MAP_RENAME:
		case MAP_CONVERT:
				avail = (ldb_msg_find_ldb_val(msg, attr->u.rename.remote_name) != NULL);
				break;

		case MAP_GENERATE:
				avail = True;
				for (j = 0; attr->u.generate.remote_names[j]; j++) {
					avail &= (ldb_msg_find_ldb_val(msg, attr->u.generate.remote_names[j]) != NULL);
				}
				break;
		}

		if (!avail)
			continue;

		ret = talloc_realloc(module, ret, const char *, count+2);
		ret[count] = attr->local_name;
		ret[count+1] = NULL;
		count++;
	}

	return ret;
}

static struct ldb_message *ldb_map_message_incoming(struct ldb_module *module, const char * const*attrs, const struct ldb_message *mi)
{
	int i;
	struct ldb_message *msg = talloc_zero(module, struct ldb_message);
	struct ldb_message_element *elm, *oldelm;
	const char **newattrs = NULL;

	msg->dn = map_remote_dn(module, mi->dn);

	/* Loop over attrs, find in ldb_map_attribute array and 
	 * run generate() */

	if (attrs == NULL) {
		/* Generate list of the local attributes that /can/ be generated
		 * using the specific remote attributes */

		attrs = newattrs = available_local_attributes(module, mi);
	}

	for (i = 0; attrs[i]; i++) {
		const struct ldb_map_attribute *attr = map_find_attr_local(module, attrs[i]);

		if (!attr) {
			DEBUG(0, ("Unable to find local attribute '%s' when generating incoming message\n", attrs[i]));
			continue;
		}

		switch (attr->type) {
			case MAP_IGNORE:break;
			case MAP_RENAME:
				oldelm = ldb_msg_find_element(mi, attr->u.rename.remote_name);

				elm = talloc(msg, struct ldb_message_element);
				elm->name = talloc_strdup(elm, attr->local_name);
				elm->num_values = oldelm->num_values;
				elm->values = talloc_reference(elm, oldelm->values);

				ldb_msg_add(module->ldb, msg, elm, oldelm->flags);
				break;
				
			case MAP_CONVERT:
				oldelm = ldb_msg_find_element(mi, attr->u.rename.remote_name);
				elm = attr->u.convert.convert_local(msg, attr->local_name, oldelm);

				ldb_msg_add(module->ldb, msg, elm, elm->flags);
				break;

			case MAP_KEEP:
				oldelm = ldb_msg_find_element(mi, attr->local_name);

				elm = talloc(msg, struct ldb_message_element);

				elm->num_values = oldelm->num_values;
				elm->values = talloc_reference(elm, oldelm->values);
				elm->name = talloc_strdup(elm, oldelm->name);

				ldb_msg_add(module->ldb, msg, elm, oldelm->flags);
				break;

			case MAP_GENERATE:
				elm = attr->u.generate.generate_local(msg, attr->local_name, mi);
				ldb_msg_add(module->ldb, msg, elm, elm->flags);
				break;
			default: 
				DEBUG(0, ("Unknown attr->type for %s", attr->local_name));
				break;
		}
	}

	talloc_free(newattrs);

	return msg;
}

static struct ldb_message *ldb_map_message_outgoing(struct ldb_module *module, const struct ldb_message *mo)
{
	struct ldb_message *msg = talloc_zero(module, struct ldb_message);
	struct ldb_message_element *elm;
	int i;
	
	msg->private_data = mo->private_data;
	
	msg->dn = map_local_dn(module, mo->dn);

	/* Loop over mi and call generate_remote for each attribute */
	for (i = 0; i < mo->num_elements; i++) {
		const struct ldb_map_attribute *attr = map_find_attr_local(module, mo->elements[i].name);

		if (!attr) {
			DEBUG(0, ("Undefined local attribute '%s', ignoring\n", mo->elements[i].name));
			continue;
		}

		switch (attr->type) {
		case MAP_IGNORE: break;
		case MAP_RENAME:
			elm = talloc(msg, struct ldb_message_element);

			elm->name = talloc_strdup(elm, attr->u.rename.remote_name);
			elm->num_values = mo->elements[i].num_values;
			elm->values = talloc_reference(elm, mo->elements[i].values);

			ldb_msg_add(module->ldb, msg, elm, mo->elements[i].flags);
			break;

		case MAP_CONVERT:
			elm = attr->u.convert.convert_remote(msg, attr->local_name, &mo->elements[i]);
			ldb_msg_add(module->ldb, msg, elm, elm->flags);
			break;

		case MAP_KEEP:
			elm = talloc(msg, struct ldb_message_element);

			elm->num_values = mo->elements[i].num_values;
			elm->values = talloc_reference(elm, mo->elements[i].values);
			elm->name = talloc_strdup(elm, mo->elements[i].name);
			
			ldb_msg_add(module->ldb, msg, elm, mo->elements[i].flags);	
			break;

		case MAP_GENERATE:
			attr->u.generate.generate_remote(attr->local_name, mo, msg);
			break;
		} 
	}

	return msg;
}

/*
  rename a record
*/
static int map_rename(struct ldb_module *module, const struct ldb_dn *olddn, const struct ldb_dn *newdn)
{
	struct ldb_dn *n_olddn, *n_newdn;
	int ret;
	
	n_olddn = map_local_dn(module, olddn);
	n_newdn = map_local_dn(module, newdn);

	ret = ldb_next_rename_record(module, n_olddn, n_newdn);

	talloc_free(n_olddn);
	talloc_free(n_newdn);
	
	return ret;
}

/*
  delete a record
*/
static int map_delete(struct ldb_module *module, const struct ldb_dn *dn)
{
	struct ldb_dn *newdn;
	int ret;

	newdn = map_local_dn(module, dn);

	ret = ldb_next_delete_record(module, newdn);

	talloc_free(newdn);

	return ret;
}

/*
  search for matching records using a ldb_parse_tree
*/
static int map_search_bytree(struct ldb_module *module, const struct ldb_dn *base,
			      enum ldb_scope scope, struct ldb_parse_tree *tree,
			      const char * const *attrs, struct ldb_message ***res)
{
	int ret;
	const char **newattrs;
	struct ldb_parse_tree *new_tree;
	struct ldb_dn *new_base;
	struct ldb_message **newres;
	int i;

	new_tree = ldb_map_parse_tree(module, module, tree);
	newattrs = ldb_map_attrs(module, attrs); 
	new_base = map_local_dn(module, base);

	ret = ldb_next_search_bytree(module, new_base, scope, new_tree, newattrs, &newres);

	talloc_free(new_base);
	talloc_free(new_tree);
	talloc_free(newattrs);

	*res = talloc_array(module, struct ldb_message *, ret);

	for (i = 0; i < ret; i++) {
		(*res)[i] = ldb_map_message_incoming(module, attrs, newres[i]);
		talloc_free(newres[i]);
	}

	return ret;
}
/*
  search for matching records
*/
static int map_search(struct ldb_module *module, const struct ldb_dn *base,
		       enum ldb_scope scope, const char *expression,
		       const char * const *attrs, struct ldb_message ***res)
{
	struct map_private *map = module->private_data;
	struct ldb_parse_tree *tree;
	int ret;

	tree = ldb_parse_tree(NULL, expression);
	if (tree == NULL) {
		map->last_err_string = "expression parse failed";
		return -1;
	}

	ret = map_search_bytree(module, base, scope, tree, attrs, res);
	talloc_free(tree);
	return ret;
}

/*
  add a record
*/
static int map_add(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ldb_message *nmsg = ldb_map_message_outgoing(module, msg);
	int ret;

	ret = ldb_next_add_record(module, nmsg);

	talloc_free(nmsg);

	return ret;
}




/*
  modify a record
*/
static int map_modify(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ldb_message *nmsg = ldb_map_message_outgoing(module, msg);
	int ret;

	ret = ldb_next_modify_record(module, nmsg);

	talloc_free(nmsg);

	return ret;
}

static int map_lock(struct ldb_module *module, const char *lockname)
{
	return ldb_next_named_lock(module, lockname);
}

static int map_unlock(struct ldb_module *module, const char *lockname)
{
	return ldb_next_named_unlock(module, lockname);
}

/*
  return extended error information
*/
static const char *map_errstring(struct ldb_module *module)
{
	struct map_private *map = module->private_data;
	
	if (map->last_err_string)
		return map->last_err_string;

	return ldb_next_errstring(module);
}

static const struct ldb_module_ops map_ops = {
	.name          = "map",
	.search        = map_search,
	.search_bytree = map_search_bytree,
	.add_record    = map_add,
	.modify_record = map_modify,
	.delete_record = map_delete,
	.rename_record = map_rename,
	.named_lock    = map_lock,
	.named_unlock  = map_unlock,
	.errstring     = map_errstring
};

/* the init function */
struct ldb_module *ldb_map_init(struct ldb_context *ldb, const struct ldb_map_attribute *attrs, const struct ldb_map_objectclass *ocls, const char *options[])
{
	struct ldb_module *ctx;
	struct map_private *data;

	ctx = talloc(ldb, struct ldb_module);
	if (!ctx)
		return NULL;

	data = talloc(ctx, struct map_private);
	if (!data) {
		talloc_free(ctx);
		return NULL;
	}

	data->last_err_string = NULL;

	data->attribute_maps = attrs;
	data->objectclass_maps = ocls;
	ctx->private_data = data;
	ctx->ldb = ldb;
	ctx->prev = ctx->next = NULL;
	ctx->ops = &map_ops;

	return ctx;
}
