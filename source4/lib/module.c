/* 
   Unix SMB/CIFS implementation.
   module loading system

   Copyright (C) Jelmer Vernooij 2002
   
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

#ifdef HAVE_DLOPEN
int smb_load_module(const char *module_name)
{
	void *handle;
	init_module_function *init;
	int status;
	const char *error;

	/* Always try to use LAZY symbol resolving; if the plugin has 
	 * backwards compatibility, there might be symbols in the 
	 * plugin referencing to old (removed) functions
	 */
	handle = sys_dlopen(module_name, RTLD_LAZY);

	if(!handle) {
		DEBUG(0, ("Error loading module '%s': %s\n", module_name, sys_dlerror()));
		return False;
	}

	init = sys_dlsym(handle, "init_module");

	/* we must check sys_dlerror() to determine if it worked, because
           sys_dlsym() can validly return NULL */
	error = sys_dlerror();
	if (error) {
		DEBUG(0, ("Error trying to resolve symbol 'init_module' in %s: %s\n", module_name, error));
		return False;
	}

	status = init();

	DEBUG(2, ("Module '%s' loaded\n", module_name));

	return status;
}

/* Load all modules in list and return number of 
 * modules that has been successfully loaded */
int smb_load_modules(const char **modules)
{
	int i;
	int success = 0;

	for(i = 0; modules[i]; i++){
		if(smb_load_module(modules[i])) {
			success++;
		}
	}

	DEBUG(2, ("%d modules successfully loaded\n", success));

	return success;
}

int smb_probe_module(const char *subsystem, const char *module)
{
	char *full_path;
	int rc;
	TALLOC_CTX *mem_ctx;
	
	/* Check for absolute path */
	if(module[0] == '/')return smb_load_module(module);
	
	mem_ctx = talloc_init("smb_probe_module");
	if (!mem_ctx) {
		DEBUG(0,("No memory for loading modules\n"));
		return False;
	}
	full_path = talloc_strdup(mem_ctx, lib_path(mem_ctx, subsystem));
	full_path = talloc_asprintf(mem_ctx, "%s/%s.%s", 
		full_path, module, shlib_ext());
	
	rc = smb_load_module(full_path);
	talloc_destroy(mem_ctx);
	return rc;
}

#else /* HAVE_DLOPEN */

int smb_load_module(const char *module_name)
{
	DEBUG(0,("This samba executable has not been built with plugin support"));
	return False;
}

int smb_load_modules(const char **modules)
{
	DEBUG(0,("This samba executable has not been built with plugin support"));
	return False;
}

int smb_probe_module(const char *subsystem, const char *module)
{
	DEBUG(0,("This samba executable has not been built with plugin support, not probing")); 
	return False;
}

#endif /* HAVE_DLOPEN */

void init_modules(void)
{
	if(lp_preload_modules()) 
		smb_load_modules(lp_preload_modules());
	/* FIXME: load static modules */
}
