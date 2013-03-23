/* radare - LGPL - Copyright 2009-2013 - pancake */

#include "r_types.h"
#include "r_util.h"

#if 0
/* TODO: the basic object lifecycle must be simplified */
struct r_class_t {
	// new/free are implicit
	.init = &r_buf_init,
	.fini = &r_buf_fini,
} r_buf_class;

#define r_buf_init(x) r_buf_class->init
#endif

R_API RBuffer *r_buf_new() {
	RBuffer *b = R_NEW (RBuffer);
	if (b) {
		b->buf = NULL;
		b->length = 0;
		b->cur = 0;
		b->base = 0LL;
		b->mmap = NULL;
	}
	return b;
}

R_API RBuffer *r_buf_mmap (const char *file, int rw) {
	RBuffer *b = r_buf_new ();
	if (!b) return NULL;
	b->mmap = r_file_mmap (file, rw, 0);
	if (b->mmap && b->mmap->len>0) {
		b->buf = b->mmap->buf;
		b->length = b->mmap->len;
		return b;
	}
	r_buf_free (b);
	return NULL; /* we just freed b, don't return it */
}

R_API RBuffer *r_buf_file (const char *file) {
	RBuffer *b = r_buf_new ();
	if (!b) return NULL;
	b->buf = (ut8*)r_file_slurp (file, &b->length);
	if (b->buf) return b;
	r_buf_free (b);
	return NULL; /* we just freed b, don't return it */
}

R_API int r_buf_set_bits(RBuffer *b, int bitoff, int bitsize, ut64 value) {
	// TODO: implement r_buf_set_bits
	// TODO: get the implementation from reg/value.c ?
	return R_FALSE;
}

R_API int r_buf_set_bytes(RBuffer *b, const ut8 *buf, int length) {
	if (b->buf) free (b->buf);
	if (length<0) return R_FALSE;
	if (!(b->buf = malloc (length+1)))
		return R_FALSE;
	memcpy (b->buf, buf, length);
	b->length = length;
	return R_TRUE;
}

R_API int r_buf_prepend_bytes(RBuffer *b, const ut8 *buf, int length) {
	if (!(b->buf = realloc (b->buf, b->length+length)))
		return R_FALSE;
	memmove (b->buf+length, b->buf, b->length);
	memcpy (b->buf, buf, length);
	b->length += length;
	return R_TRUE;
}

// TODO: R_API void r_buf_insert_bytes() // with shift
// TODO: R_API void r_buf_write_bytes() // overwrite

R_API char *r_buf_to_string(RBuffer *b) {
	char *s;
	if (!b) return strdup ("");
	s = malloc (b->length+1);
	memcpy (s, b->buf, b->length);
	s[b->length] = 0;
	return s;
}

R_API int r_buf_append_bytes(RBuffer *b, const ut8 *buf, int length) {
	if (!(b->buf = realloc (b->buf, b->length+length)))
		return R_FALSE;
	memcpy (b->buf+b->length, buf, length);
	b->length += length;
	return R_TRUE;
}

R_API int r_buf_append_nbytes(RBuffer *b, int length) {
	if (!(b->buf = realloc (b->buf, b->length+length)))
		return R_FALSE;
	memset (b->buf+b->length, 0, length);
	b->length += length;
	return R_TRUE;
}

R_API int r_buf_append_ut16(RBuffer *b, ut16 n) {
	if (!(b->buf = realloc (b->buf, b->length+sizeof (n))))
		return R_FALSE;
	memcpy (b->buf+b->length, &n, sizeof (n));
	b->length += sizeof (n);
	return R_TRUE;
}

R_API int r_buf_append_ut32(RBuffer *b, ut32 n) {
	if (!(b->buf = realloc (b->buf, b->length+sizeof (n))))
		return R_FALSE;
	memcpy (b->buf+b->length, &n, sizeof (n));
	b->length += sizeof (n);
	return R_TRUE;
}

R_API int r_buf_append_ut64(RBuffer *b, ut64 n) {
	if (!(b->buf = realloc (b->buf, b->length+sizeof (n))))
		return R_FALSE;
	memcpy (b->buf+b->length, &n, sizeof (n));
	b->length += sizeof (n);
	return R_TRUE;
}

R_API int r_buf_append_buf(RBuffer *b, RBuffer *a) {
	if (!(b->buf = realloc (b->buf, b->length+a->length)))
		return R_FALSE;
	memcpy (b->buf+b->length, a->buf, a->length);
	b->length += a->length;
	return R_TRUE;
}

static int r_buf_cpy(RBuffer *b, ut64 addr, ut8 *dst, const ut8 *src, int len, int write) {
	int end;
	if (!b) return 0;
	addr = (addr==R_BUF_CUR)? b->cur: addr-b->base;
	if (len<1 || dst == NULL || addr > b->length)
		return -1;
 	end = (int)(addr+len);
	if (end > b->length)
		len -= end-b->length;
	if (write)
		dst += addr;
	else src += addr;
	memcpy (dst, src, len);
	b->cur = addr + len;
	return len;
}

static int r_buf_fcpy_at (RBuffer *b, ut64 addr, ut8 *buf, const char *fmt, int n, int write) {
	int i, j, k, len, tsize, endian, m = 1;

	if (addr == R_BUF_CUR)
		addr = b->cur;
	else addr -= b->base;
	if (addr < 0 || addr > b->length)
		return -1;
	for (i = len = 0; i < n; i++)
	for (j = 0; fmt[j]; j++) {
		if (len > b->length)
			return -1;
		switch (fmt[j]) {
		case '0'...'9':
			if (m == 1)
				m = r_num_get(NULL, &fmt[j]);
			continue;
		case 's': tsize = 2; endian = 1; break;
		case 'S': tsize = 2; endian = 0; break;
		case 'i': tsize = 4; endian = 1; break;
		case 'I': tsize = 4; endian = 0; break;
		case 'l': tsize = 8; endian = 1; break;
		case 'L': tsize = 8; endian = 0; break;
		case 'c': tsize = 1; endian = 1; break;
		default: return -1;
		}
		for (k = 0; k < m; k++) {
			if (write) r_mem_copyendian((ut8*)&buf[addr+len+k*tsize],
					(ut8*)&b->buf[len+k*tsize], tsize, endian);
			else r_mem_copyendian((ut8*)&buf[len+k*tsize],
					(ut8*)&b->buf[addr+len+k*tsize], tsize, endian);
		}
		len += m*tsize; m = 1;
	}
	b->cur = addr + len;
	return len;
}

R_API int r_buf_read_at(RBuffer *b, ut64 addr, ut8 *buf, int len) {
	if (!b) return 0;
	return r_buf_cpy (b, addr, buf, b->buf, len, R_FALSE);
}

R_API int r_buf_fread_at (RBuffer *b, ut64 addr, ut8 *buf, const char *fmt, int n) {
	return r_buf_fcpy_at (b, addr, buf, fmt, n, R_FALSE);
}

R_API int r_buf_write_at(RBuffer *b, ut64 addr, const ut8 *buf, int len) {
	if (!b) return 0;
	return r_buf_cpy (b, addr, b->buf, buf, len, R_TRUE);
}

R_API int r_buf_fwrite_at (RBuffer *b, ut64 addr, ut8 *buf, const char *fmt, int n) {
	return r_buf_fcpy_at (b, addr, buf, fmt, n, R_TRUE);
}

R_API void r_buf_deinit(RBuffer *b) {
	if (b->mmap) {
		r_file_mmap_free (b->mmap);
		b->mmap = NULL;
	} else {
		free (b->buf);
	}
}

R_API void r_buf_free(struct r_buf_t *b) {
	if (!b) return;
	r_buf_deinit (b);
	free (b);
}
