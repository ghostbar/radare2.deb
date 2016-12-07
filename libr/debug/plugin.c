/* radare - LGPL - Copyright 2009-2016 pancake */

#include <r_debug.h>
#include "../config.h"

static RDebugPlugin *debug_static_plugins[] = {
	R_DEBUG_STATIC_PLUGINS
};

R_API void r_debug_plugin_init(RDebug *dbg) {
	int i;
	dbg->plugins = r_list_new ();
	for (i = 0; debug_static_plugins[i]; i++) {
		RDebugPlugin *p = R_NEW (RDebugPlugin);
		memcpy (p, debug_static_plugins[i], sizeof (RDebugPlugin));
		r_debug_plugin_add (dbg, p);
	}
}

R_API bool r_debug_use(RDebug *dbg, const char *str) {
	if (str) {
		RDebugPlugin *h;
		RListIter *iter;
		r_list_foreach (dbg->plugins, iter, h) {
			if (h->name && !strcmp (str, h->name)) {
				dbg->h = h;
				if (dbg->anal && dbg->anal->cur)
					r_debug_set_arch (dbg, dbg->anal->cur->arch, dbg->bits);
				dbg->bp->breakpoint = dbg->h->breakpoint;
				dbg->bp->user = dbg;
			}
		}
	}
	if (dbg->h && dbg->h->reg_profile) {
		char *p = dbg->h->reg_profile (dbg);
		if (p) {
			r_reg_set_profile_string (dbg->reg, p);
			if (dbg->anal) {
				//r_reg_free (dbg->anal->reg);
				dbg->anal->reg = dbg->reg;
			}
			if (dbg->h->init)
				dbg->h->init (dbg);
			r_reg_set_profile_string (dbg->reg, p);
			free (p);
		} else {
			eprintf ("Cannot retrieve reg profile from debug plugin (%s)\n", dbg->h->name);
		}
	}
	return (dbg->h != NULL);
}

R_API int r_debug_plugin_list(RDebug *dbg, int mode) {
	char spaces[16];
	int count = 0;
	memset (spaces, ' ', 15);
	spaces[15] = 0;
	RDebugPlugin *h;
	RListIter *iter;
	r_list_foreach (dbg->plugins, iter, h) {
		int sp = 8-strlen (h->name);
		spaces[sp] = 0;
		if (mode == 'q') {
			dbg->cb_printf ("%s\n", h->name);
		} else {
			dbg->cb_printf ("%d  %s  %s %s%s\n",
					count, (h == dbg->h)? "dbg": "---",
					h->name, spaces, h->license);
		}
		spaces[sp] = ' ';
		count++;
	}
	return false;
}

R_API bool r_debug_plugin_add(RDebug *dbg, RDebugPlugin *foo) {
	if (!dbg || !foo || !foo->name) {
		return false;
	}
	r_list_append (dbg->plugins, foo);
	return true;
}
