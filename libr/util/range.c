/* radare - LGPL - Copyright 2008-2010 pancake<nopcode.org> */

#include <r_util.h>

// TODO: redesign this api

#if 0
// TODO: add tags to ranges
#endif

//void (*ranges_new_callback)(struct range_t *r) = NULL;

R_API int r_range_init(RRange *r) {
	r->count = 0;
	r->changed = 0;
	INIT_LIST_HEAD (&r->ranges);
	return 0;
}

R_API RRange *r_range_new() {
	RRange *r = R_NEW (RRange);
	if (r == NULL)
		return NULL;
	r_range_init (r);
	return r;
}

R_API RRange *r_range_free(RRange *r) {
	struct list_head *pos;
	list_for_each (pos, &r->ranges) {
		RRangeItem *h = list_entry (pos, RRangeItem, list);
		free (h);
	}
	free (r);
	return NULL;
}

// TODO: optimize by just returning the pointers to the internal foo?
R_API int r_range_get_data(RRange *rgs, ut64 addr, ut8 *buf, int len) {
	RRangeItem *r = r_range_item_get(rgs, addr);
	if (r == NULL)
		return 0;
	if (r->datalen < len)
		len = r->datalen;
	memcpy(buf, r->data, len);
	return len;
}

R_API int r_range_set_data(RRange *rgs, ut64 addr, const ut8 *buf, int len) {
	RRangeItem *r = r_range_item_get(rgs, addr);
	if (r == NULL)
		return 0;
	r->data = (ut8*)malloc(len);
	r->datalen = len;
	memcpy(r->data, buf, len);
	return 1;
}

RRangeItem *r_range_item_get(RRange *rgs, ut64 addr)
{
	RRangeItem *r;
	struct list_head *pos;
	list_for_each(pos, &rgs->ranges) {
		r = list_entry(pos, RRangeItem, list);
		if (addr >= r->fr && addr < r->to)
			return r;
	}
	return NULL;
}

/* returns the sum of all the ranges contained */
// XXX: can be catched while adding/removing elements
R_API ut64 r_range_size(RRange *rgs) {
	struct list_head *pos;
	RRangeItem *r;
	ut64 sum = 0;

	list_for_each (pos, &rgs->ranges) {
		r = list_entry (pos, RRangeItem, list);
		sum += r->to - r->fr;
	}
	return sum;
}

R_API RRange *r_range_new_from_string(const char *string)
{
	RRange *rgs = r_range_new();
	r_range_add_from_string(rgs, string);
	return rgs;
}

R_API int r_range_add_from_string(RRange *rgs, const char *string) {
	ut64 addr, addr2;
	int i, len = strlen (string)+1;
	char *str = alloca (len);
	char *p = str;
	char *p2 = NULL;
	RRangeItem *r;

	memcpy (str, string, len);

	for (i=0;i<len;i++) {
		switch(str[i]) {
		case '-':
			str[i]='\0';
			p2 = p;
			p = str+i+1;
			break;
		case ',':
			str[i]='\0';
			if (p2) {
				addr = r_num_get(NULL, p);
				addr2 = r_num_get(NULL, p2);
				r = r_range_add(rgs, addr, addr2, 1);
				p2 = NULL;
			} else {
				addr = r_num_get(NULL, p);
				r = r_range_add(rgs, addr, addr+1, 1);
			}
			p = str+i+1;
			str[i]=',';
			break;
		}
	}
	if (p2) {
		addr = r_num_get (NULL, p);
		addr2 = r_num_get (NULL, p2);
		r = r_range_add (rgs, addr, addr2, 1);
	} else 
	if (p) {
		addr = r_num_get (NULL, p);
		r = r_range_add (rgs, addr, addr+1, 1);
	}

	return rgs->changed;
}

#if 0
    update to      new one     update fr   update fr/to  ignore

   |______|        |___|           |_____|      |____|      |_______|  range_t
+     |______|   +      |__|   + |___|      + |_________|  +  |__|     fr/to
  ------------   -----------   -----------  -------------  -----------
=  |_________|   = |___||__|   = |_______|  = |_________|   |_______|  result
#endif

RRangeItem *r_range_add(RRange *rgs, ut64 fr, ut64 to, int rw) {
	struct list_head *pos;
	RRangeItem *r;
	RRangeItem *ret = NULL;
	int add = 1;

	r_num_minmax_swap (&fr, &to);

	list_for_each (pos, &rgs->ranges) {
		r = list_entry(pos, RRangeItem, list);
		if (r->fr == fr && r->to==to) {
			add = 0;
		} else
		if (r->fr<=fr && r->fr <= to && r->to>=fr && r->to <= to) {
			r->to = to;
			ret = r;
			add = 0;
		} else
		if (r->fr>=fr && r->fr<=to && r->to>=fr && r->to >= to) {
			r->fr = fr;
			ret = r;
			add = 0;
		} else
		if (r->fr<=fr && r->fr<=to && r->to>=fr && r->to >= to) {
			/* ignore */
			add = 0;
		} else
		if (r->fr>=fr && r->fr<=to && r->to>=fr && r->to <= to) {
			r->fr = fr;
			r->to = to;
			ret = r;
			add = 0;
		}
	}

	if (rw && add) {
		ret = R_NEW (RRangeItem);
		ret->fr = fr;
		ret->to = to;
		ret->datalen = 0;
		ret->data = NULL;
		list_add_tail (&(ret->list), &rgs->ranges);
		rgs->changed = 1;
	}

	return ret;
}

#if 0
    update to      ignore      update fr      delete        split

   |______|        |___|           |_____|      |____|       |________|  range_t
-     |______|   -      |__|   - |___|      - |_________|  -    |__|     fr/to
  ------------   -----------   -----------  -------------  ------------
=  |__|          =             =     |___|  =                |__|  |__|   result
#endif

R_API int r_range_sub(RRange *rgs, ut64 fr, ut64 to) {
	RRangeItem *r;
	struct list_head *pos;

	r_num_minmax_swap (&fr, &to);

	__reloop:
	list_for_each(pos, &rgs->ranges) {
		r = list_entry(pos, RRangeItem, list);
		/* update to */
		if (r->fr<fr && r->fr < to && r->to>fr && r->to < to) {
			r->to = fr;
		} else
		/* update fr */
		if (r->fr>fr && r->fr<to && r->to>fr && r->to>to) {
			r->fr = to;
		}
		/* delete */
		if (r->fr>fr && r->fr<to && r->to>fr && r->to < to) {
			/* delete */
			list_del(&(r->list));
			rgs->changed = 1;
			goto __reloop;
		}
		/* split */
		if (r->fr<fr && r->fr<to && r->to>fr && r->to > to) {
			r->to = fr;
			r_range_add (rgs, to, r->to, 1);
			//ranges_add(rang, to, r->to, 1);
			goto __reloop;
		}
	}
	return 0;
}

/* TODO: should remove some of them right? */
//int r_range_merge(RRange *r)
R_API int r_range_merge(RRange *rgs, RRange *r) {
	struct list_head *pos;
	list_for_each(pos, &r->ranges) {
		RRangeItem *r = list_entry (pos, RRangeItem, list);
		r_range_add (rgs, r->fr, r->to, 0);
	}
	return 0;
}

//int ranges_is_used(ut64 addr)
R_API int r_range_contains(RRange *rgs, ut64 addr) {
	struct list_head *pos;
	list_for_each (pos, &rgs->ranges) {
		RRangeItem *r = list_entry(pos, RRangeItem, list);
		if (addr >= r->fr && addr <= r->to)
			return R_TRUE;
	}
	return R_FALSE;
}

R_API int r_range_sort(RRange *rgs) {
	struct list_head *pos, *pos2, *n, *n2;

	if (!rgs->changed)
		return R_FALSE;
	rgs->changed = R_FALSE;

	list_for_each_safe (pos, n, &rgs->ranges) {
		RRangeItem *r = list_entry (pos, RRangeItem, list);
		list_for_each_safe (pos2, n2, &rgs->ranges) {
			RRangeItem *r2 = list_entry (pos2, RRangeItem, list);
			if ((r != r2) && (r->fr > r2->fr)) {
				list_move (pos, pos2);
				rgs->changed = 1;
			}
		}
	}
	return rgs->changed;
}

R_API void r_range_percent(RRange *rgs) {
	struct list_head *pos;
	int w, i;
	ut64 seek, step;
	ut64 dif, fr = -1, to = -1;

	list_for_each (pos, &rgs->ranges) {
		RRangeItem *r = list_entry(pos, RRangeItem, list);
		if (fr == -1) {
			/* init */
			fr = r->fr;
			to = r->to;
		} else {
			if (fr>r->fr) fr = r->fr;
			if (to<r->to) to = r->to;
		}
	}
	w = 65 ; // columns
	if (fr != -1) {
		dif = to-fr;
		if (dif<w) step = 1; // XXX
		else step = dif/w;
		seek = 0;
	} else step = fr = to = 0;
	seek = 0;
	// XXX do not use printf here!
	printf ("0x%08llx [", fr);
	for (i=0; i<w; i++) {
		if (r_range_contains (rgs, seek))
			printf ("#");
		else printf (".");
		seek += step;
	}
	printf ("] 0x%08llx\n", to);
}

// TODO: total can be cached in rgs!!
int r_range_list(RRange *rgs, int rad) {
	ut64 total = 0;
	struct list_head *pos;
	r_range_sort (rgs);
	list_for_each (pos, &rgs->ranges) {
		RRangeItem *r = list_entry (pos, RRangeItem, list);
		if (rad) printf ("ar+ 0x%08llx 0x%08llx\n", r->fr, r->to);
		else printf ("0x%08llx 0x%08llx ; %lld\n", r->fr, r->to, r->to-r->fr);
		total += (r->to-r->fr);
	}
	eprintf ("Total bytes: %lld\n", total);
	return 0;
}

int r_range_get_n(RRange *rgs, int n, ut64 *fr, ut64 *to) {
	int count = 0;
	struct list_head *pos;
	r_range_sort (rgs);
	list_for_each (pos, &rgs->ranges) {
		RRangeItem *r = list_entry (pos, RRangeItem, list);
		if  (count == n) {
			*fr = r->fr;
			*to = r->to;
			return 1;
		}
		count++;
	}
	return 0;
}

#if 0
     .....|______________________|...
      |_____|  |____|  |_______|
    ---------------------------------
            |__|    |__|       |_|      
#endif
RRange *r_range_inverse(RRange *rgs, ut64 fr, ut64 to, int flags) {
	ut64 total = 0;
	struct list_head *pos;
	RRangeItem *r = NULL;
	RRange *newrgs = r_range_new();

	r_range_sort(rgs);

	list_for_each(pos, &rgs->ranges) {
		r = list_entry(pos, RRangeItem, list);
		if (r->fr > fr && r->fr < to) {
			r_range_add(newrgs, fr, r->fr, 1);
			//eprintf("0x%08llx .. 0x%08llx\n", fr, r->fr);
			total += (r->fr - fr);
			fr = r->to;
		}
	}
	if (fr < to) {
		//eprintf("0x%08llx .. 0x%08llx\n", fr, to);
		r_range_add(newrgs, fr, to, 1);
		total += (to-fr);
	}
	// eprintf("Total bytes: %lld\n", total);

	return newrgs;
}
