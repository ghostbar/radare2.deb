/* radare - LGPL - Copyright 2009-2010 nibble<.ds@gmail.com> */

#include <stdio.h>

#include <r_types.h>
#include <r_util.h>
#include <r_asm.h>
#include <list.h>
#include "../config.h"

#define D if (0)

static struct r_asm_handle_t *asm_static_plugins[] = 
	{ R_ASM_STATIC_PLUGINS };

static int r_asm_pseudo_string(struct r_asm_aop_t *aop, char *input) {
	int len = strlen (input)+1;
	r_hex_bin2str ((ut8*)input, len, aop->buf_hex);
	strncpy ((char*)aop->buf, input, R_ASM_BUFSIZE);
	return len;
}

static inline int r_asm_pseudo_arch(struct r_asm_t *a, char *input) {
	if (!r_asm_use (a, input)) {
		eprintf ("Error: Unknown plugin\n");
		return -1;
	}
	return 0;
}

static inline int r_asm_pseudo_bits(struct r_asm_t *a, char *input) {
	if (!(r_asm_set_bits (a, r_num_math (NULL, input))))
		eprintf ("Error: Unsupported bits value\n");
	else return 0;
	return -1;
}

static inline int r_asm_pseudo_org(struct r_asm_t *a, char *input) {
	r_asm_set_pc (a, r_num_math (NULL, input));
	return 0;
}

static inline int r_asm_pseudo_byte(struct r_asm_aop_t *aop, char *input) {
	int len = r_hex_str2bin (input, aop->buf);
	strncpy (aop->buf_hex, r_str_trim(input), R_ASM_BUFSIZE);
	return len;
}

R_API RAsm *r_asm_new() {
	return r_asm_init (R_NEW (RAsm));
}

R_API void r_asm_free(struct r_asm_t *a) {
	// TOOD: free plugins and so on
	free(a);
}

/* return fastcall register argument 'idx' for a syscall with 'num' args */
R_API const char *r_asm_fastcall(struct r_asm_t *a, int idx, int num) {
	struct r_asm_fastcall_t *fastcall;
	const char *ret = NULL;
	if (a && a->cur && a->cur->fastcall)
		fastcall = *a->cur->fastcall;
	if (fastcall && idx<=num)
		ret = fastcall[num].arg[idx];
	return ret;
}

R_API struct r_asm_t *r_asm_init(struct r_asm_t *a) {
	int i;
	if (a) {
		a->user = NULL;
		a->cur = NULL;
		a->bits = 32;
		a->big_endian = 0;
		a->syntax = R_ASM_SYNTAX_INTEL;
		a->pc = 0;
		INIT_LIST_HEAD (&a->asms);
		for (i=0; asm_static_plugins[i]; i++)
			r_asm_add (a, asm_static_plugins[i]);
	}
	return a;
}

R_API void r_asm_set_user_ptr(struct r_asm_t *a, void *user) {
	a->user = user;
}

R_API int r_asm_add(struct r_asm_t *a, struct r_asm_handle_t *foo) {
	struct list_head *pos;
	// TODO: cache foo->name length and use memcmp instead of strcmp
	if (foo->init)
		foo->init (a->user);
	/* avoid dupped plugins */
	list_for_each_prev (pos, &a->asms) {
		RAsmHandle *h = list_entry (pos, RAsmHandle, list);
		if (!strcmp (h->name, foo->name))
			return R_FALSE;
	}
	
	list_add_tail (&(foo->list), &(a->asms));
	return R_TRUE;
}

R_API int r_asm_del(struct r_asm_t *a, const char *name) {
	/* TODO: Implement r_asm_del */
	return R_FALSE;
}

R_API int r_asm_list(struct r_asm_t *a) {
	struct list_head *pos;
	list_for_each_prev(pos, &a->asms) {
		RAsmHandle *h = list_entry(pos, RAsmHandle, list);
		printf ("asm %s\t %s\n", h->name, h->desc);
	}
	return R_FALSE;
}

// TODO: this can be optimized using r_str_hash()
R_API int r_asm_use(struct r_asm_t *a, const char *name) {
	struct list_head *pos;
	list_for_each_prev (pos, &a->asms) {
		RAsmHandle *h = list_entry (pos, RAsmHandle, list);
		if (!strcmp (h->name, name)) {
			a->cur = h;
			return R_TRUE;
		}
	}
	return R_FALSE;
}

R_API int r_asm_set_subarch(struct r_asm_t *a, const char *name) {
	int ret = R_FALSE;
	if (a->cur && a->cur->set_subarch)
		ret = a->cur->set_subarch(a, name);
	return ret;
}

static int has_bits(struct r_asm_handle_t *h, int bits) {
	int i;
	if (h && h->bits)
		for(i=0; h->bits[i]; i++)
			if (bits == h->bits[i])
				return R_TRUE;
	return R_FALSE;
}

R_API int r_asm_set_bits(struct r_asm_t *a, int bits) {
	if (has_bits (a->cur, bits)) {
		a->bits = bits;
		return R_TRUE;
	}
	return R_FALSE;
}

R_API int r_asm_set_big_endian(struct r_asm_t *a, int boolean) {
	a->big_endian = boolean;
	return R_TRUE;
}

R_API int r_asm_set_syntax(struct r_asm_t *a, int syntax) {
	switch (syntax) {
	case R_ASM_SYNTAX_INTEL:
	case R_ASM_SYNTAX_ATT:
		a->syntax = syntax;
		return R_TRUE;
	default:
		return R_FALSE;
	}
}

R_API int r_asm_set_pc(struct r_asm_t *a, ut64 pc) {
	a->pc = pc;
	return R_TRUE;
}

R_API int r_asm_disassemble(struct r_asm_t *a, struct r_asm_aop_t *aop, ut8 *buf, ut64 len) {
	int ret = 0;
	if (a->cur && a->cur->disassemble)
		ret = a->cur->disassemble(a, aop, buf, len);
	if (ret > 0) {
		memcpy (aop->buf, buf, ret);
		r_hex_bin2str (buf, ret, aop->buf_hex);
	}
	return ret;
}

R_API int r_asm_assemble(struct r_asm_t *a, struct r_asm_aop_t *aop, const char *buf) {
	int ret = 0;
	struct list_head *pos;
	if (a->cur) {
		if (a->cur->assemble)
			ret = a->cur->assemble (a, aop, buf);
		/* find callback if no assembler support in current plugin */
		else list_for_each_prev (pos, &a->asms) {
			RAsmHandle *h = list_entry (pos, RAsmHandle, list);
			if (h->arch && h->assemble && has_bits(h, a->bits)
			&& !strcmp(a->cur->arch, h->arch)) {
				ret = h->assemble(a, aop, buf);
				break;
			}
		}
	}
	if (aop && ret > 0) {
		r_hex_bin2str (aop->buf, ret, aop->buf_hex);
		aop->inst_len = ret;
		aop->buf_hex[ret*2]=0;
		strncpy (aop->buf_asm, buf, R_ASM_BUFSIZE);
	}
	return ret;
}

R_API struct r_asm_code_t* r_asm_mdisassemble(struct r_asm_t *a, ut8 *buf, ut64 len) {
	struct r_asm_aop_t aop;
	struct r_asm_code_t *acode;
	int ret, slen;
	ut64 idx;

	if (!(acode = r_asm_code_new()))
		return NULL;
	if (!(acode->buf = malloc(len)))
		return r_asm_code_free(acode);
	memcpy(acode->buf, buf, len);
	if (!(acode->buf_hex = malloc(2*len+1)))
		return r_asm_code_free(acode);
	r_hex_bin2str(buf, len, acode->buf_hex);
	if (!(acode->buf_asm = malloc(2)))
		return r_asm_code_free(acode);
	
	for (idx = ret = slen = 0, acode->buf_asm[0] = '\0'; idx < len; idx+=ret) {
		r_asm_set_pc(a, a->pc + ret);
		ret = r_asm_disassemble (a, &aop, buf+idx, len-idx);
		if (ret<1) {
			eprintf ("error disassemble at offset %lld\n", idx);
			return r_asm_code_free (acode);
		}
		//eprintf ("++ %d %d\n", ret, aop.inst_len);
		//ret = aop.inst_len; // are always equal??
		slen += strlen (aop.buf_asm) + 2;
		if(!(acode->buf_asm = realloc (acode->buf_asm, slen)))
			return r_asm_code_free (acode);
		strcat (acode->buf_asm, aop.buf_asm);
		strcat (acode->buf_asm, "\n");
		//if (idx + ret < len)
	}

	acode->len = idx;

	return acode;
}

R_API struct r_asm_code_t* r_asm_massemble(struct r_asm_t *a, const char *buf) {
	char *lbuf = NULL, *ptr2, *ptr = NULL, *ptr_start = NULL,
		 *tokens[R_ASM_BUFSIZE], buf_token[R_ASM_BUFSIZE];
	int labels = 0, stage, ret, idx, ctr, i, j;
	struct r_asm_aop_t aop;
	ut64 off;
	RAsmCode *acode = NULL;

	if (buf == NULL)
		return NULL;
	if (!(acode = r_asm_code_new ()))
		return NULL;
	if (!(acode->buf_asm = malloc (strlen (buf)+16)))
		return r_asm_code_free (acode);
	strcpy (acode->buf_asm, buf);
	if (!(acode->buf_hex = malloc (64)))
		return r_asm_code_free (acode);
	acode->buf_hex[0]=0;
	if (!(acode->buf = malloc (64)))
		return r_asm_code_free (acode);
	lbuf = strdup (buf);

	if (strchr (lbuf, ':'))
		labels = 1;

	/* Tokenize */
	for (tokens[0] = lbuf, ctr = 0;
		(ptr = strchr (tokens[ctr], ';')) || 
		(ptr = strchr (tokens[ctr], '\n')) ||
		(ptr = strchr (tokens[ctr], '\r'));
		tokens[++ctr] = ptr+1)
			*ptr = '\0';

	/* Stage 0-1: Parse labels*/
	/* Stage 2: Assemble */
	for (stage = 0; stage < 3; stage++) {
		if (stage < 2 && !labels)
			continue;
		for (idx = ret = i = j = 0, off = a->pc, acode->buf_hex[0] = '\0';
			i <= ctr; i++, idx += ret) {
			strncpy (buf_token, tokens[i], R_ASM_BUFSIZE);
			for (ptr_start = buf_token; *ptr_start &&
				isseparator (*ptr_start); ptr_start++);
			ptr = strchr (ptr_start, '#'); /* Comments */
			if (ptr) *ptr = '\0';
			if (stage == 2) {
				r_asm_set_pc (a, a->pc + ret);
				off = a->pc;
			} else off +=ret;
			ret = 0;
			if (!*ptr_start)
				continue;
			//eprintf ("LINE %d %s\n", stage, ptr_start);
			if (labels) /* Labels */
			if ((ptr = strchr (ptr_start, ':'))) {
				char food[64];
				if (stage != 2) {
					*ptr = 0;
					snprintf (food, sizeof (food), "0x%llx", off);
					r_asm_code_set_equ (acode, ptr_start, food);
					D eprintf ("SETEQU (%s,%s)\n", ptr_start, food);
				}
				ptr_start = ptr + 1;
			}
			if (*ptr_start == '\0') {
				ret = 0;
				continue;	
			} else if (*ptr_start == '.') { /* pseudo */
				ptr = ptr_start;
				if (!memcmp (ptr, ".string ", 8))
					ret = r_asm_pseudo_string (&aop, ptr+8);
				else if (!memcmp (ptr, ".arch ", 6))
					ret = r_asm_pseudo_arch (a, ptr+6);
				else if (!memcmp (ptr, ".bits ", 6))
					ret = r_asm_pseudo_bits (a, ptr+6);
				else if (!memcmp (ptr, ".byte ", 6))
					ret = r_asm_pseudo_byte (&aop, ptr+6);
				else if (!memcmp (ptr, ".global ", 8)) {
				//	eprintf (".global directive not yet implemented\n");
					ret = 0;
					continue;
				} else if (!memcmp (ptr, ".equ ", 5)) {
					ptr2 = strchr (ptr+5, ',');
					if (ptr2) {
						*ptr2 = '\0';
						r_asm_code_set_equ (acode, ptr+5, ptr2+1);
					} else eprintf ("TODO: undef equ\n");
				} else if (!memcmp (ptr, ".org ", 5)) {
					ret = r_asm_pseudo_org (a, ptr+5);
					off = a->pc;
				} else {
					eprintf ("Unknown keyword (%s)\n", ptr);
					return r_asm_code_free (acode);
				}
				if (!ret) continue;
				else if (ret < 0) {
					eprintf ("!!! Oops\n");
					return r_asm_code_free (acode);
				}
			} else { /* Instruction */
				if (acode->equs) {
					char *str = r_asm_code_equ_replace (acode, strdup (ptr_start));
					D eprintf ("%s\n", str);
					ret = r_asm_assemble (a, &aop, str);
					free (str);
				} else {
					ret = r_asm_assemble (a, &aop, ptr_start);
					D eprintf ("%s\n", ptr_start);
				}
			}
			if (stage == 2) {
				if (ret < 1) {
					printf ("Cannot assemble '%s'\n", ptr_start);
					return r_asm_code_free (acode);
				} else {
					acode->len = idx + ret;
					if (!(acode->buf = realloc (acode->buf, (idx+ret)*2)))
						return r_asm_code_free (acode);
					if (!(acode->buf_hex = realloc (acode->buf_hex, (acode->len*2)+1)))
						return r_asm_code_free (acode);
					memcpy (acode->buf+idx, aop.buf, ret);
					strcat (acode->buf_hex, aop.buf_hex);
				}
			}
		}
	}
	return acode;
}
