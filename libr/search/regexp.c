/* radare - LGPL - Copyright 2008-2010 pancake<nopcode.org> */

#include "r_search.h"
#if __UNIX__
#include <regex.h>

R_API int r_search_regexp_update(void *_s, ut64 from, const ut8 *buf, int len) {
	RSearch *s = (RSearch*)_s;
	RListIter *iter;
	char *buffer = malloc (len+1);
	char *skipz, *end;
	int count = 0;

	memcpy (buffer, buf, len);
	buffer[len]='\0';

	RSearchKeyword *kw;
	r_list_foreach (s->kws, iter, kw) {
		int reflags = REG_EXTENDED;
		int ret, delta = 0;
		regmatch_t matches[10];
		regex_t compiled;

		if (strchr (kw->binmask, 'i'))
			reflags |= REG_ICASE;

		if (regcomp (&compiled, kw->keyword, reflags)) {
			eprintf ("Cannot compile '%s' regexp\n",kw->keyword);
			return -1;
		}
		foo:
		ret = regexec (&compiled, buffer+delta, 1, matches, 0);
		if (ret) return 0;
		do {
			r_search_hit_new (s, kw, (ut64)(from+matches[0].rm_so+delta));
			delta += matches[0].rm_so+1;
			kw->count++;
			count++;
		} while (!regexec (&compiled, buffer+delta, 1, matches, 0));
		if (delta == 0)
			return 0;

		/* TODO: check if skip 0 works */
		skipz = strchr (buffer, '\0');
		end = buffer+len;
		if (skipz && skipz+1 < end) {
			for (;!*skipz&&end;skipz=skipz+1);
			delta = skipz-buffer;
			if (kw->count>0)
				goto foo;
		}
	}
	return count;
}
#else

R_API int r_search_regexp_update(void *_s, ut64 from, const ut8 *buf, int len) {
	eprintf ("r_search_regexp_update: unimplemented for this platform\n");
	return -1;
}

#endif
