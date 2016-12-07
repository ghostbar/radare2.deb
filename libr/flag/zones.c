/* radare - LGPL - Copyright 2016 - pancake */

#include <r_flag.h>
#include <r_util.h>

#define DB f->zones

#if !R_FLAG_ZONE_USE_SDB

static RFlagZoneItem *r_flag_zone_get (RFlag *f, const char *name) {
	RListIter *iter;
	RFlagZoneItem *zi;
	r_list_foreach (DB, iter, zi) {
		if (!strcmp (name, zi->name)) {
			return zi;
		}
	}
	return NULL;
}
#endif

R_API bool r_flag_zone_add(RFlag *f, const char *name, ut64 addr) {
#if 1
	if (!name || !f || !*name) {
		return false;
	}
#endif
#if R_FLAG_ZONE_USE_SDB
	RFlagZoneItem zi = { 0, 0, (const char *)name };
	if (!f || !DB || !name) {
		return false;
	}
	const char *bound = sdb_const_get (DB, name, NULL);
	if (bound) {
		sdb_fmt_tobin (bound, "qq", &zi);
		if (addr < zi.from) {
			zi.from = addr;
		}
		if (addr > zi.to) {
			zi.to = addr;
		}
		char *newBounds = sdb_fmt_tostr (&zi, "qq");
		sdb_set (DB, name, newBounds, 0);
		free (newBounds);
	} else {
		sdb_set (DB, name, sdb_fmt (0, "%"PFMT64d",%"PFMT64d, addr, addr), 0);
	}
#else
	RFlagZoneItem *zi = r_flag_zone_get (f, name);
	if (zi) {
		if (addr < zi->from) {
			zi->from = addr;
		}
		if (addr > zi->to) {
			zi->to = addr;
		}
	} else {
		if (!DB) {
			r_flag_zone_reset (f);
		}
		zi = R_NEW0 (RFlagZoneItem);
		zi->name = strdup (name);
		zi->from = zi->to = addr;
		r_list_append (DB, zi);
	}
#endif
	return true;
}

R_API bool r_flag_zone_reset(RFlag *f) {
#if R_FLAG_ZONE_USE_SDB
	return sdb_reset (DB);
#else
	r_list_free (f->zones);
	f->zones = r_list_newf (r_flag_zone_item_free);
	return true;
#endif
}

R_API bool r_flag_zone_del(RFlag *f, const char *name) {
#if R_FLAG_ZONE_USE_SDB
	return sdb_unset (DB, name, 0);
#else
	RListIter *iter;
	RFlagZoneItem *zi;
	r_list_foreach (DB, iter, zi) {
		if (!strcmp (name, zi->name)) {
			r_list_delete (DB, iter);
			return true;
		}
	}
	return false;
#endif
}

#if R_FLAG_ZONE_USE_SDB

typedef struct r_flag_zone_context_t {
	RFlag *f;
	ut64 addr;
	ut64 l, h; // lower, higher closest offsets
	const char **prev;
	const char **next;
} RFlagZoneContext;

static int cb(void *user, const char *name, const char *from_to) {
	RFlagZoneContext *zc = (RFlagZoneContext*)user;
	RFlagZoneItem zi = { 0, 0, name };
	sdb_fmt_tobin (from_to, "qq", &zi);
	if (zi.from > zc->addr) {
		if (zc->h == UT64_MAX) {
			zc->h = zi.from;
			*zc->next = name;
		} else {
			if (zi.from < zc->h) {
				zc->h = zi.from;
				*zc->next = name;
			}
		}
	}
	if (zi.from < zc->addr) {
		if (zc->l == UT64_MAX) {
			zc->l = zi.from;
			*zc->prev = name;
		} else {
			if (zi.from >= zc->l) {
				zc->l = zi.from;
				*zc->prev = name;
			}
		}
	}
	if (zi.to <= zc->addr) {
		if (zc->l == UT64_MAX) {
			zc->l = zi.to;
			*zc->prev = name;
		} else {
			if (zi.to >= zc->l) {
				zc->l = zi.to;
				*zc->prev = name;
			}
		}
	}
	if (zi.to > zc->addr) {
		if (zc->h == UT64_MAX) {
			zc->h = zi.to;
			*zc->next = name;
		} else {
			if (zi.to < zc->h) {
				zc->h = zi.to;
				*zc->next = name;
			}
		}
	}
	return 1;
}

R_API bool r_flag_zone_around(RFlag *f, ut64 addr, const char **prev, const char **next) {
	RFlagZoneContext ctx = { f, addr, 0, UT64_MAX, prev, next };
	*prev = *next = NULL;
	sdb_foreach (DB, cb, &ctx);
	return true;
}

static int cb_list(void *user, const char *name, const char *from_to) {
	eprintf ("%s\t%s\n", name, from_to);
	return 1;
}

R_API bool r_flag_zone_list(RFlag *f, int mode) {
	sdb_foreach (DB, cb_list, NULL);
	return true;
}

#else

R_API void r_flag_zone_item_free(void *a) {
	RFlagZoneItem *zi = a;
	free (zi->name);
	free (zi);
}

R_API bool r_flag_zone_around(RFlag *f, ut64 addr, const char **prev, const char **next) {
	RListIter *iter;
	RFlagZoneItem *zi;
	*prev = *next = NULL;
	ut64 h = UT64_MAX, l = 0LL;

	r_list_foreach (DB, iter, zi) {
		if (zi->from > addr) {
			if (h == UT64_MAX) {
				h = zi->from;
				*next = zi->name;
			} else {
				if (zi->from < h) {
					h = zi->from;
					*next = zi->name;
				}
			}
		}
		if (zi->from < addr) {
			if (l == UT64_MAX) {
				l = zi->from;
				*prev = zi->name;
			} else {
				if (zi->from >= l) {
					l = zi->from;
					*prev = zi->name;
				}
			}
		}
		if (zi->to <= addr) {
			if (l == UT64_MAX) {
				l = zi->to;
				*prev = zi->name;
			} else {
				if (zi->to >= l) {
					l = zi->to;
					*prev = zi->name;
				}
			}
		}
		if (zi->to > addr) {
			if (h == UT64_MAX) {
				h = zi->to;
				*next = zi->name;
			} else {
				if (zi->to < h) {
					h = zi->to;
					*next = zi->name;
				}
			}
		}
	}
	return true;
}

R_API bool r_flag_zone_list(RFlag *f, int mode) {
	RListIter *iter;
	RFlagZoneItem *zi;
	r_list_foreach (DB, iter, zi) {
		if (mode == '*') {
			f->cb_printf ("f %s @ 0x08%"PFMT64x"\n", zi->name, zi->from);
			f->cb_printf ("f %s @ 0x08%"PFMT64x"\n", zi->name, zi->to);
		} else {
			f->cb_printf ("0x08%"PFMT64x"  0x%08"PFMT64x"  %s\n",
					zi->from, zi->to, zi->name);
		}
	}
	return true;
}
#endif

// #define __MAIN__ 1
#if __MAIN__
#define FZ(x) r_flag_zone_##x

int main() {
	const char *a, *b;
	RFlagZone *fz = r_flag_zone_new ();

	FZ(add)(fz, "main", 0x80000);
	FZ(add)(fz, "network", 0x85000);
	FZ(add)(fz, "libc", 0x90000);

	FZ(add)(fz, "network", 0x000);

	FZ(around)(fz, 0x83000, &a, &b);
	printf ("%s %s\n", a, b);

	FZ(around)(fz, 0x50000, &a, &b);
	printf ("%s %s\n", a, b);

	FZ(around)(fz, 0x500000, &a, &b);
	printf ("%s %s\n", a, b);

	FZ(list)(fz);

	r_flag_zone_free (fz);
	return 0;
}
#endif
