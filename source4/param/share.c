/* 
   Unix SMB/CIFS implementation.
   
   Modular services configuration system
   
   Copyright (C) Simo Sorce	2006
   
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
#include "param/share.h"
#include "build.h"

const char *share_string_option(struct share_config *scfg, const char *opt_name, const char *defval)
{
	return scfg->ctx->ops->string_option(scfg, opt_name, defval);
}

int share_int_option(struct share_config *scfg, const char *opt_name, int defval)
{
	return scfg->ctx->ops->int_option(scfg, opt_name, defval);
}

BOOL share_bool_option(struct share_config *scfg, const char *opt_name, BOOL defval)
{
	return scfg->ctx->ops->bool_option(scfg, opt_name, defval);
}

const char **share_string_list_option(TALLOC_CTX *mem_ctx, struct share_config *scfg, const char *opt_name)
{
	return scfg->ctx->ops->string_list_option(mem_ctx, scfg, opt_name);
}

NTSTATUS share_list_all(TALLOC_CTX *mem_ctx, struct share_context *sctx, int *count, const char ***names)
{
	return sctx->ops->list_all(mem_ctx, sctx, count, names);
}

NTSTATUS share_get_config(TALLOC_CTX *mem_ctx, struct share_context *sctx, const char *name, struct share_config **scfg)
{
	return sctx->ops->get_config(mem_ctx, sctx, name, scfg);
}

NTSTATUS share_create(struct share_context *sctx, const char *name, struct share_info *info, int count)
{
	if (sctx->ops->create) {
		return sctx->ops->create(sctx, name, info, count);
	}
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS share_set(struct share_context *sctx, const char *name, struct share_info *info, int count)
{
	if (sctx->ops->set) {
		return sctx->ops->set(sctx, name, info, count);
	}
	return NT_STATUS_NOT_IMPLEMENTED;
}

NTSTATUS share_remove(struct share_context *sctx, const char *name)
{
	if (sctx->ops->remove) {
		return sctx->ops->remove(sctx, name);
	}
	return NT_STATUS_NOT_IMPLEMENTED;
}

/* List of currently available share backends */
static struct share_ops **backends = NULL;

static const struct share_ops *share_backend_by_name(const char *name)
{
	int i;

	for (i = 0; backends && backends[i]; i++) {
		if (strcmp(backends[i]->name, name) == 0) {
			return backends[i];
		}
	}

	return NULL;
}

/*
  Register the share backend
*/
NTSTATUS share_register(const struct share_ops *ops)
{
	int i;

	if (share_backend_by_name(ops->name) != NULL) {
		DEBUG(0,("SHARE backend [%s] already registered\n", ops->name));
		return NT_STATUS_OBJECT_NAME_COLLISION;
	}

	i = 0;
	while (backends && backends[i]) {
		i++;
	}

	backends = realloc_p(backends, struct share_ops *, i + 2);
	if (!backends) {
		smb_panic("out of memory in share_register");
	}

	backends[i] = malloc(sizeof(struct share_ops));
	if (!backends[i]) {
		smb_panic("out of memory in share_register");
	}

	backends[i] = smb_xmemdup(ops, sizeof(*ops));
	backends[i]->name = smb_xstrdup(ops->name);

	backends[i + 1] = NULL;

	DEBUG(3, ("SHARE backend [%s] registered.\n", ops->name));

	return NT_STATUS_OK;
}

NTSTATUS share_get_context_by_name(TALLOC_CTX *mem_ctx, 
								   const char *backend_name,
								   struct share_context **ctx)
{
	const struct share_ops *ops;

	ops = share_backend_by_name(backend_name);
	if (!ops) {
		DEBUG(0, ("share_init_connection: share backend [%s] not found!\n", lp_share_backend()));
		return NT_STATUS_INTERNAL_ERROR;
	}

	return ops->init(mem_ctx, ops, ctx);
}

NTSTATUS share_get_context(TALLOC_CTX *mem_ctx, struct share_context **ctx)
{
	return share_get_context_by_name(mem_ctx, lp_share_backend(), ctx);
}

/*
  initialise the SHARE subsystem
*/
NTSTATUS share_init(void)
{
	init_module_fn static_init[] = STATIC_share_MODULES;
	init_module_fn *shared_init = load_samba_modules(NULL, "share");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	return NT_STATUS_OK;
}
