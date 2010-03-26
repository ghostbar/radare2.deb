/* radare - LGPL - Copyright 2009-2010 */
/*   nibble<.ds@gmail.com> */
/*   pancake<nopcode.org> */

#include <r_anal.h>
#include <r_util.h>
#include <r_list.h>
#include "../config.h"

static RAnalHandle *anal_static_plugins[] = 
	{ R_ANAL_STATIC_PLUGINS };

static RAnalVarType anal_default_vartypes[] =
	{{ "char",  "b",  1 },
	 { "byte",  "b",  1 },
	 { "int",   "d",  4 },
	 { "int32", "d",  4 },
	 { "dword", "x",  4 },
	 { "float", "f",  4 },
	 { NULL,    NULL, 0 }};

R_API RAnal *r_anal_new() {
	return r_anal_init (R_NEW (RAnal));
}

R_API RAnal *r_anal_free(RAnal *a) {
	if (a) {
		/* TODO: Free a->anals here */
		if (a->bbs)
			r_list_destroy (a->bbs);
		if (a->fcns)
			r_list_destroy (a->fcns);
		if (a->vartypes)
			r_list_destroy (a->vartypes);
	}
	free (a);
	return NULL;
}

R_API RAnal *r_anal_init(RAnal *anal) {
	int i;

	if (anal) {
		memset (anal, 0, sizeof (RAnal));
		anal->bbs = r_anal_bb_list_new ();
		anal->fcns = r_anal_fcn_list_new ();
		anal->vartypes = r_anal_var_type_list_new ();
		r_anal_set_bits (anal, 32);
		r_anal_set_big_endian (anal, R_FALSE);
		INIT_LIST_HEAD (&anal->anals);
		for (i=0; anal_static_plugins[i]; i++)
			r_anal_add (anal, anal_static_plugins[i]);
		for (i=0; anal_default_vartypes[i].name; i++)
			r_anal_var_type_add (anal, anal_default_vartypes[i].name,
					anal_default_vartypes[i].size, anal_default_vartypes[i].fmt);
	}
	return anal;
}

R_API void r_anal_set_user_ptr(RAnal *anal, void *user) {
	anal->user = user;
}

R_API int r_anal_add(RAnal *anal, RAnalHandle *foo) {
	if (foo->init)
		foo->init(anal->user);
	list_add_tail(&(foo->list), &(anal->anals));
	return R_TRUE;
}

// TODO: Must be deprecated
R_API int r_anal_list(RAnal *anal) {
	struct list_head *pos;
	list_for_each_prev(pos, &anal->anals) {
		RAnalHandle *h = list_entry(pos, RAnalHandle, list);
		printf (" %s: %s\n", h->name, h->desc);
	}
	return R_FALSE;
}

R_API int r_anal_use(RAnal *anal, const char *name) {
	struct list_head *pos;
	list_for_each_prev (pos, &anal->anals) {
		RAnalHandle *h = list_entry(pos, RAnalHandle, list);
		if (!strcmp (h->name, name)) {
			anal->cur = h;
			return R_TRUE;
		}
	}
	return R_FALSE;
}

R_API int r_anal_set_bits(RAnal *anal, int bits) {
	switch (bits) {
	case 8:
	case 16:
	case 32:
	case 64:
		anal->bits = bits;
		return R_TRUE;
	}
	return R_FALSE;
}

R_API int r_anal_set_big_endian(RAnal *anal, int bigend) {
	anal->big_endian = bigend;
	return R_TRUE;
}
