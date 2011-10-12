/* Public domain. */

#include <stdio.h>
#include <unistd.h>
#include "alloc.h"
#include "cdb.h"
#include "cdb_make.h"

int cdb_make_start(struct cdb_make *c, int fd) {
	c->head = 0;
	c->split = 0;
	c->hash = 0;
	c->numentries = 0;
	c->fd = fd;
	c->pos = sizeof c->final;
	buffer_init (&c->b, (BufferOp)write, fd, c->bspace, sizeof c->bspace);
	return seek_set (fd, c->pos);
}

// WTF?!?
static inline int posplus(struct cdb_make *c, ut32 len) {
	ut32 newpos = c->pos + len;
	if (newpos < len)
		return 0;
	c->pos = newpos;
	return 1;
}

int cdb_make_addend(struct cdb_make *c, ut32 keylen, ut32 datalen, ut32 h) {
	struct cdb_hplist *head = c->head;
	if (!head || (head->num >= CDB_HPLIST)) {
		if (!(head = R_ANEW (struct cdb_hplist)))
			return 0;
		head->num = 0;
		head->next = c->head;
		c->head = head;
	}
	head->hp[head->num].h = h;
	head->hp[head->num].p = c->pos;
	++head->num;
	++c->numentries;
	return posplus (c, KVLSZ+keylen+datalen);
}

static void pack_kvlen(ut8 *buf, ut32 klen, ut32 vlen) {
	buf[0] = (ut8)klen;
	buf[1] = (ut8)((vlen    ) & 255);
	buf[2] = (ut8)((vlen>>8 ) & 255);
	buf[3] = (ut8)((vlen>>16) & 255);
}

int cdb_make_addbegin(struct cdb_make *c,unsigned int keylen,unsigned int datalen) {
	ut8 buf[KVLSZ];
	pack_kvlen (buf, keylen, datalen);
	return buffer_putalign (&c->b, (const char *)buf, KVLSZ);
}

int cdb_make_add(struct cdb_make *c,const char *key,unsigned int keylen,const char *data,unsigned int datalen) {
	if (!cdb_make_addbegin (c, keylen, datalen)) return 0;
	if (!buffer_putalign (&c->b, key, keylen)) return 0;
	if (!buffer_putalign (&c->b, data, datalen)) return 0;
	return cdb_make_addend (c, keylen, datalen, cdb_hash (key, keylen));
}

int cdb_make_finish(struct cdb_make *c) {
	int i;
	char buf[8];
	struct cdb_hp *hp;
	struct cdb_hplist *x, *n;
	ut32 len, u, memsize, count, where;

	for (i=0; i<256; i++)
		c->count[i] = 0;

	for (x=c->head; x; x=x->next) {
		i = x->num;
		while (i--)
			c->count[255 & x->hp[i].h]++;
	}

	memsize = 1;
	for (i=0; i<256; i++) {
		u = c->count[i] * 2;
		if (u > memsize)
			memsize = u;
	}
	memsize += c->numentries; /* no overflow possible up to now */
	u = (ut32) 0 - (ut32) 1; // TODO: use UT32_MAX here
	u /= sizeof (struct cdb_hp);
	if (memsize > u) return 0;

	c->split = (struct cdb_hp *) alloc (memsize * sizeof (struct cdb_hp));
	if (!c->split) return 0;

	c->hash = c->split + c->numentries;

	for (u=i=0; i<256; i++) {
		u += c->count[i]; /* bounded by numentries, so no overflow */
		c->start[i] = u;
	}

	for (x=c->head; x; x=x->next) {
		i = x->num;
		while (i--)
			c->split[--c->start[255 & x->hp[i].h]] = x->hp[i];
	}

	for (i=0; i<256; i++) {
		count = c->count[i];
		len = count<<1; //(*2)
		ut32_pack (c->final + 8 * i, c->pos);
		ut32_pack (c->final + 8 * i + 4, len);

		for (u=0; u<len; u++)
			c->hash[u].h = c->hash[u].p = 0;

		hp = c->split + c->start[i];
		for (u=0; u<count; u++) {
			where = (hp->h >> 8) % len;
			while (c->hash[where].p)
				if (++where == len)
					where = 0;
			c->hash[where] = *hp++;
		}

		for (u = 0; u<len; u++) {
			ut32_pack (buf, c->hash[u].h);
			ut32_pack (buf + 4, c->hash[u].p);
			if (!buffer_putalign (&c->b, buf, 8)) return 0;
			if (!posplus (c, 8)) return 0;
		}
	}

	if (!buffer_flush (&c->b)) return 0;
	if (!seek_begin (c->fd)) return 0;

	// free chills
	for (x = c->head; x;) {
		n = x->next;
		free (x);
		x = n;
	}
	alloc_free (c->split);
	return buffer_putflush (&c->b, c->final, sizeof c->final);
}
