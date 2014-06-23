/* radare - LGPL - Copyright 2010-2013 - nibble, pancake */

#include <r_anal.h>
#include <r_util.h>
#include <r_list.h>

R_API RAnalRef *r_anal_ref_new() {
	RAnalRef *ref = R_NEW (RAnalRef);
	if (ref) {
		ref->addr = -1;
		ref->at = -1;
		ref->type = R_ANAL_REF_TYPE_CODE;
	}
	return ref;
}

R_API RList *r_anal_ref_list_new() {
	RList *list = r_list_new ();
	list->free = &r_anal_ref_free;
	return list;
}

R_API void r_anal_ref_free(void *ref) {
	free (ref);
}

#define USE_NEW_REFS 1
// TODO: use sdb or hashmap for fucks sake
R_API int r_anal_ref_add(RAnal *anal, ut64 addr, ut64 at, int type) {
#if USE_NEW_REFS
	const char *types = type=='c'?"jmp":
		type=='C'?"call": type=='s'?"string": "data";
	r_anal_xrefs_set (anal, types, at, addr);
#else
	RAnalRef *ref = NULL, *refi;
	RListIter *iter, *iter2;
	RAnalFunction *fcni;
	// search in funrefs
	r_list_foreach (anal->fcns, iter, fcni) {
		r_list_foreach (fcni->refs, iter2, refi) {
			if (at == refi->at) {
				if (addr == refi->addr) {
					return R_FALSE;
				}
				ref = refi;
				break;
			}
		}
	}
	if (!ref)
	r_list_foreach (anal->refs, iter, refi) {
		if (at == refi->at) {
			if (addr == refi->addr) {
				return R_FALSE;
			}
			ref = refi;
			break;
		}
	}
	if (ref == NULL) {
		if (!(ref = r_anal_ref_new ()))
			return R_FALSE;
		r_list_append (anal->refs, ref);
	}
	ref->addr = addr;
	ref->at = at;
	ref->type = type;
#endif
	return R_TRUE;
}

R_API int r_anal_ref_del(RAnal *anal, ut64 at, ut64 addr) {
#if USE_NEW_REFS
	r_anal_xrefs_deln (anal, "code", at, addr);
	r_anal_xrefs_deln (anal, "data", at, addr);
#else
	RAnalRef *refi;
	RListIter *iter, *iter_tmp;
	if (at == 0) {
		r_list_free (anal->refs);
		if (!(anal->refs = r_anal_ref_list_new ()))
			return R_FALSE;
	} else {
		r_list_foreach_safe (anal->refs, iter, iter_tmp, refi) {
			if (at == refi->at)
				r_list_delete (anal->refs, iter);
		}
	}
#endif
	return R_TRUE;
}

R_API RList *r_anal_xrefs_get (RAnal *anal, ut64 addr);
// XXX: MAJOR SLOWDOWN PLZ FIX
R_API RList *r_anal_xref_get(RAnal *anal, ut64 addr) {
	return r_anal_xrefs_get (anal, addr);
}

/*
	RAnalFunction *fcni;
	RAnalRef *refi, *ref, *refr;
	RListIter *iter, *iter2, *iter3;
	RList *ret;

for (list = sdb_list_begin (DB); list; list = sdb_list_next (list)) {
	char *str = astring();
	eprintf ("--> %s\n", str);
}

char *list = sdb_list_begin(DB)
while (list) {
	
	list = sdb_list_next (list);
}
	$ sdb xrefs
	()types=code,data
	()data.0x1200=0x1000,0x1030,0x1090
	()code.0x3020=0x2010,0x2042

	int clen, dlen;
	const char **coderefs, **datarefs;

	coderefs = r_anal_xrefs_get (anal, "code", &clen);
	datarefs = r_anal_xrefs_get (anal, "data", &dlen);
	if (!coderefs && !datarefs)
		return NULL;

	xrefs[0x80480] = { type: "data", from: "
	RAnalFudr_anal_fcn_get_at (anal, addr);
#endif
	if (!(ret = r_anal_ref_list_new ()))
		return NULL;
	// XXX: this is just a hack that makes analysis/disasm much slower but
	// work as expected. We need to redesign the whole analysis engine :)
	// - find real reverse xrefs by deep walk
	// - addr = our target destination
	r_list_foreach (anal->fcns, iter, fcni) {
		r_list_foreach (fcni->refs, iter2, refi) {
			if (refi->addr == addr) {
				int gonext = 0;
				r_list_foreach (ret, iter3, refr) {
					if (refr->addr == refi->at) // same sauce, so we can skip
						gonext = 1;
				}
				if (gonext) continue;
				// wtf copying xrefs for new lists .. tahts insanely slow
				if (!(ref = r_anal_ref_new ())) {
					r_list_destroy (ret);
					return NULL;
				}
				// NOTE: swapped hacky valuez
				ref->addr = refi->at;
				ref->at = refi->addr;
				ref->type = refi->type;
				r_list_append (ret, ref);
			}
		}
	}
	if (r_list_length (ret)>0)
		return ret;

	r_list_foreach (anal->fcns, iter, fcni) {
		if (addr >= fcni->addr && addr < fcni->addr+fcni->size) {
			r_list_foreach (fcni->xrefs, iter2, refi) {
				if (refi->at == addr) {
					if (!(ref = r_anal_ref_new ())) {
						r_list_destroy (ret);
						return NULL;
					}
					ref->addr = refi->addr;
					ref->at = refi->at;
					ref->type= refi->type;
					r_list_append (ret, ref);
				}
			}
			break; // may break on corner cases
		}
	}
	r_list_foreach (anal->refs, iter2, refi)
		if (refi->addr == addr) {
			if (!(ref = r_anal_ref_new ())) {
				r_list_destroy (ret);
				return NULL;
			}
			ref->addr = refi->at;
			ref->at = refi->addr;
			ref->type= refi->type;
			r_list_append (ret, ref);
		}
	return ret;
}
*/
