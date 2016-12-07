/* radare - LGPL - Copyright 2015-2016 - inisider */

#include "mach0_classes.h"

#define RO_META (1 << 0)

struct MACH0_(SMethodList) {
	ut32 entsize;
	ut32 count;
	/* SMethod first;  These structures follow inline */
};

struct MACH0_(SMethod) {
	mach0_ut name;  /* SEL (32/64-bit pointer) */
	mach0_ut types; /* const char * (32/64-bit pointer) */
	mach0_ut imp;   /* IMP (32/64-bit pointer) */
};

struct MACH0_(SClass) {
	mach0_ut isa;	/* SClass* (32/64-bit pointer) */
	mach0_ut superclass; /* SClass* (32/64-bit pointer) */
	mach0_ut cache;      /* Cache (32/64-bit pointer) */
	mach0_ut vtable;     /* IMP * (32/64-bit pointer) */
	mach0_ut data;       /* SClassRoT * (32/64-bit pointer) */
};

struct MACH0_(SClassRoT) {
	ut32 flags;
	ut32 instanceStart;
	ut32 instanceSize;
#ifdef R_BIN_MACH064
	ut32 reserved;
#endif
	mach0_ut ivarLayout;     /* const uint8_t* (32/64-bit pointer) */
	mach0_ut name;		/* const char* (32/64-bit pointer) */
	mach0_ut baseMethods;    /* const SMEthodList* (32/64-bit pointer) */
	mach0_ut baseProtocols;  /* const SProtocolList* (32/64-bit pointer) */
	mach0_ut ivars;		/* const SIVarList* (32/64-bit pointer) */
	mach0_ut weakIvarLayout; /* const uint8_t * (32/64-bit pointer) */
	mach0_ut baseProperties; /* const SObjcPropertyList* (32/64-bit pointer) */
};

struct MACH0_(SProtocolList) {
	mach0_ut count; /* uintptr_t (a 32/64-bit value) */
			/* SProtocol* list[0];  These pointers follow inline */
};

struct MACH0_(SProtocol) {
	mach0_ut isa;			/* id* (32/64-bit pointer) */
	mach0_ut name;			/* const char * (32/64-bit pointer) */
	mach0_ut protocols;		/* SProtocolList* (32/64-bit pointer) */
	mach0_ut instanceMethods;	/* SMethodList* (32/64-bit pointer) */
	mach0_ut classMethods;		/* SMethodList* (32/64-bit pointer) */
	mach0_ut optionalInstanceMethods; /* SMethodList* (32/64-bit pointer) */
	mach0_ut optionalClassMethods;    /* SMethodList* (32/64-bit pointer) */
	mach0_ut instanceProperties;      /* struct SObjcPropertyList* (32/64-bit pointer) */
};

struct MACH0_(SIVarList) {
	ut32 entsize;
	ut32 count;
	/* SIVar first;  These structures follow inline */
};

struct MACH0_(SIVar) {
	mach0_ut offset; /* uintptr_t * (32/64-bit pointer) */
	mach0_ut name;   /* const char * (32/64-bit pointer) */
	mach0_ut type;   /* const char * (32/64-bit pointer) */
	ut32 alignment;
	ut32 size;
};

struct MACH0_(SObjcProperty) {
	mach0_ut name;       /* const char * (32/64-bit pointer) */
	mach0_ut attributes; /* const char * (32/64-bit pointer) */
};

struct MACH0_(SObjcPropertyList) {
	ut32 entsize;
	ut32 count;
	/* struct SObjcProperty first;  These structures follow inline */
};

///////////////////////////////////////////////////////////////////////////////
static mach0_ut get_pointer(mach0_ut p, ut32 *offset, ut32 *left, RBinFile *arch);
static void copy_sym_name_with_namespace(char *class_name, char *read_name, RBinSymbol *sym);
static void get_ivar_list_t(mach0_ut p, RBinFile *arch, RBinClass *klass);
static void get_objc_property_list(mach0_ut p, RBinFile *arch, RBinClass *klass);
static void get_method_list_t(mach0_ut p, RBinFile *arch, char *class_name, RBinClass *klass, bool is_static);
static void get_protocol_list_t(mach0_ut p, RBinFile *arch, RBinClass *klass);
static void get_class_ro_t(mach0_ut p, RBinFile *arch, ut32 *is_meta_class, RBinClass *klass);
static void get_class_t(mach0_ut p, RBinFile *arch, RBinClass *klass, bool dupe);
static void __r_bin_class_free(RBinClass *p);

static int is_thumb(RBinFile *arch) {
	struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;
	if (bin->hdr.cputype == 12) {
		if (bin->hdr.cpusubtype == 9) {
			return 1;
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static mach0_ut get_pointer(mach0_ut p, ut32 *offset, ut32 *left, RBinFile *arch) {
	mach0_ut r;
	mach0_ut addr;

	static RList *sctns = NULL;
	RListIter *iter = NULL;
	RBinSection *s = NULL;

	if (!sctns) {
		sctns = r_bin_plugin_mach.sections (arch);
		if (!sctns) {
			// retain just for debug
			// eprintf ("there is no sections\n");
			return 0;
		}
	}

	addr = p;
	r_list_foreach (sctns, iter, s) {
		if (addr >= s->vaddr && addr < s->vaddr + s->vsize) {
			if (offset) {
				*offset = addr - s->vaddr;
			}
			if (left) {
				*left = s->vsize - (addr - s->vaddr);
			}
			r = (s->paddr + (addr - s->vaddr));
#if 0
			r_list_free (sctns);
			sctns = NULL;
#endif
			return r;
		}
	}

	if (offset) {
		*offset = 0;
	}
	if (left) {
		*left = 0;
	}

#if 0
	r_list_free (sctns);
	sctns = NULL;
#endif

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static void copy_sym_name_with_namespace(char *class_name, char *read_name, RBinSymbol *sym) {
	if (!class_name) {
		class_name = "";
	}
	sym->classname = strdup (class_name);
	sym->name = strdup (read_name);
}

///////////////////////////////////////////////////////////////////////////////
static void get_ivar_list_t(mach0_ut p, RBinFile *arch, RBinClass *klass) {
	struct MACH0_(SIVarList) il;
	struct MACH0_(SIVar) i;
	mach0_ut r;
	ut32 offset, left, j;
	char *name = NULL;
	int len;
	bool bigendian;
	mach0_ut ivar_offset_p, ivar_offset;
	RBinField *field = NULL;
	ut8 sivarlist[sizeof (struct MACH0_(SIVarList))] = {0};
	ut8 sivar[sizeof (struct MACH0_(SIVar))] = {0};
	ut8 offs[sizeof (mach0_ut)] = {0};

	if (!arch || !arch->o || !arch->o->bin_obj || !arch->o->info) {
		eprintf ("uncorrect RBinFile pointer\n");
		return;
	}
	bigendian = arch->o->info->big_endian;
	if (!(r = get_pointer (p, &offset, &left, arch))) {
		return;
	}
	memset (&il, '\0', sizeof (struct MACH0_(SIVarList)));
	if (r + left < r || r + sizeof (struct MACH0_(SIVarList)) < r) {
		return;
	}
	if (r > arch->size || r + left > arch->size) {
		return;
	}
	if (r + sizeof (struct MACH0_(SIVarList)) > arch->size) {
		return;
	}
	if (left < sizeof (struct MACH0_(SIVarList))) {
		if (r_buf_read_at (arch->buf, r, sivarlist, left) != left) {
			return;
		}
	} else {
		len = r_buf_read_at (arch->buf, r, sivarlist, sizeof (struct MACH0_(SIVarList)));
		if (len != sizeof (struct MACH0_(SIVarList))) {
			return;
		}
	}
	il.entsize = r_read_ble (&sivarlist[0], bigendian, 32);
	il.count = r_read_ble (&sivarlist[4], bigendian, 32);
	p += sizeof (struct MACH0_(SIVarList));
	offset += sizeof (struct MACH0_(SIVarList));
	for (j = 0; j < il.count; j++) {
		r = get_pointer (p, &offset, &left, arch);
		if (!r) {
			return;
		}
		if (!(field = R_NEW0 (RBinField))) {
			// retain just for debug
			// eprintf ("RBinField allocation error\n");
			return;
		}
		memset (&i, '\0', sizeof (struct MACH0_(SIVar)));
		if (r + left < r || r + sizeof (struct MACH0_(SIVar)) < r) {
			goto error;
		}
		if (r > arch->size || r + left > arch->size) {
			goto error;
		}
		if (r + sizeof (struct MACH0_(SIVar)) > arch->size) {
			goto error;
		}
		if (left < sizeof (struct MACH0_(SIVar))) {
			if (r_buf_read_at (arch->buf, r, sivar, left) != left) {
				goto error;
			}
		} else {
			len = r_buf_read_at (arch->buf, r, sivar, sizeof (struct MACH0_(SIVar)));
			if (len != sizeof (struct MACH0_(SIVar))) {
				goto error;
			}
		}
		switch (sizeof (mach0_ut)) {
		case 4:
			i.offset = r_read_ble (&sivar[0], bigendian, 32);
			i.name = r_read_ble (&sivar[4], bigendian, 32);
			i.type = r_read_ble (&sivar[8], bigendian, 32);
			i.alignment = r_read_ble (&sivar[12], bigendian, 32);
			i.size = r_read_ble (&sivar[16], bigendian, 32);
			break;
		case 8:
			i.offset = r_read_ble (&sivar[0], bigendian, 64);
			i.name = r_read_ble (&sivar[8], bigendian, 64);
			i.type = r_read_ble (&sivar[16], bigendian, 64);
			i.alignment = r_read_ble (&sivar[24], bigendian, 32);
			i.size = r_read_ble (&sivar[28], bigendian, 32);
			break;
		}
		ivar_offset_p = get_pointer (i.offset, NULL, &left, arch);

		if (ivar_offset_p > arch->size) {
			goto error;
		}
		if (ivar_offset_p + sizeof (ivar_offset) > arch->size) {
			goto error;
		}
		if (ivar_offset_p != 0 && left >= sizeof (mach0_ut)) {
			len = r_buf_read_at (arch->buf, ivar_offset_p, offs, sizeof (mach0_ut));
			if (len != sizeof (mach0_ut)) {
				eprintf ("Error reading\n");
				goto error;
			}
			ivar_offset = r_read_ble (offs, bigendian, 8 * sizeof (mach0_ut));
			field->vaddr = ivar_offset;
		}

		r = get_pointer (i.name, NULL, &left, arch);
		if (r != 0) {
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;
			if (r + left < r) {
				goto error;
			}
			if (r > arch->size || r + left > arch->size) {
				goto error;
			}
			if (bin->has_crypto) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = malloc (left + 1);
				len = r_buf_read_at (arch->buf, r, (ut8 *)name, left);
				if (len < 1) {
					eprintf ("Error reading\n");
					R_FREE (name);
					goto error;
				}
				name[left] = 0;
			}
			field->name = r_str_newf ("%s::%s%s", klass->name, "(ivar)", name);
			R_FREE (name);
		}
#if 0
		r = get_pointer (i.type, NULL, &left, arch);
		if (r != 0) {
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *) arch->o->bin_obj;
			int is_crypted = bin->has_crypto;
			if (r + left < r) return;
			if (r > arch->size || r + left > arch->size) return;

			if (is_crypted == 1) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = malloc (left);
				r_buf_read_at (arch->buf, r, (ut8 *)name, left);
			}

			R_FREE (name);
		}
#endif
		r_list_append (klass->fields, field);
		p += sizeof (struct MACH0_(SIVar));
		offset += sizeof (struct MACH0_(SIVar));
	}
	return;

error:
	R_FREE (field);
	return;
}

///////////////////////////////////////////////////////////////////////////////
static void get_objc_property_list(mach0_ut p, RBinFile *arch, RBinClass *klass) {
	struct MACH0_(SObjcPropertyList) opl;
	struct MACH0_(SObjcProperty) op;
	mach0_ut r;
	ut32 offset, left, j;
	char *name = NULL;
	int len;
	bool bigendian;
	RBinField *property = NULL;
	ut8 sopl[sizeof (struct MACH0_(SObjcPropertyList))] = {0};
	ut8 sop[sizeof (struct MACH0_(SObjcProperty))] = {0};

	if (!arch || !arch->o || !arch->o->bin_obj || !arch->o->info) {
		eprintf ("uncorrect RBinFile pointer\n");
		return;
	}
	bigendian = arch->o->info->big_endian;
	r = get_pointer (p, &offset, &left, arch);
	if (!r) {
		return;
	}
	memset (&opl, '\0', sizeof (struct MACH0_(SObjcPropertyList)));
	if (r + left < r || r + sizeof (struct MACH0_(SObjcPropertyList)) < r) {
		return;
	}
	if (r > arch->size || r + left > arch->size) {
		return;
	}
	if (r + sizeof (struct MACH0_(SObjcPropertyList)) > arch->size) {
		return;
	}
	if (left < sizeof (struct MACH0_(SObjcPropertyList))) {
		if (r_buf_read_at (arch->buf, r, sopl, left) != left) {
			return;
		}
	} else {
		len = r_buf_read_at (arch->buf, r, sopl, sizeof (struct MACH0_(SObjcPropertyList)));
		if (len != sizeof (struct MACH0_(SObjcPropertyList))) {
			return;
		}
	}

	opl.entsize = r_read_ble (&sopl[0], bigendian, 32);
	opl.count = r_read_ble (&sopl[4], bigendian, 32);

	p += sizeof (struct MACH0_(SObjcPropertyList));
	offset += sizeof (struct MACH0_(SObjcPropertyList));
	for (j = 0; j < opl.count; j++) {
		r = get_pointer (p, &offset, &left, arch);
		if (!r) {
			return;
		}

		if (!(property = R_NEW0 (RBinField))) {
			// retain just for debug
			// eprintf("RBinClass allocation error\n");
			return;
		}

		memset (&op, '\0', sizeof (struct MACH0_(SObjcProperty)));

		if (r + left < r || r + sizeof (struct MACH0_(SObjcProperty)) < r) {
			goto error;
		}
		if (r > arch->size || r + left > arch->size) {
			goto error;
		}
		if (r + sizeof (struct MACH0_(SObjcProperty)) > arch->size) {
			goto error;
		}

		if (left < sizeof (struct MACH0_(SObjcProperty))) {
			if (r_buf_read_at (arch->buf, r, sop, left) != left) {
				goto error;
			}
		} else {
			len = r_buf_read_at (arch->buf, r, sop, sizeof (struct MACH0_(SObjcProperty)));
			if (len != sizeof (struct MACH0_(SObjcProperty))) {
				goto error;
			}
		}
		op.name = r_read_ble (&sop[0], bigendian, 8 * sizeof (mach0_ut));
		op.attributes = r_read_ble (&sop[sizeof (mach0_ut)], bigendian, 8 * sizeof (mach0_ut));
		r = get_pointer (op.name, NULL, &left, arch);
		if (r) {
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;
			if (r > arch->size || r + left > arch->size) {
				goto error;
			}
			if (r + left < r) {
				goto error;
			}
			if (bin->has_crypto) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = calloc (1, left + 1);
				if (!name) {
					goto error;
				}
				if (r_buf_read_at (arch->buf, r, (ut8 *)name, left) != left) {
					goto error;
				}
			}
			property->name = r_str_newf ("%s::%s%s", klass->name,
						"(property)", name);
			R_FREE (name);
		}
#if 0
		r = get_pointer (op.attributes, NULL, &left, arch);
		if (r != 0) {
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *) arch->o->bin_obj;
			int is_crypted = bin->has_crypto;

			if (r > arch->size || r + left > arch->size) goto error;
			if (r + left < r) goto error;

			if (is_crypted == 1) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = malloc (left);
				len = r_buf_read_at (arch->buf, r, (ut8 *)name, left);
				if (len == 0 || len == -1) goto error;
			}

			R_FREE (name);
		}
#endif
		r_list_append (klass->fields, property);

		p += sizeof (struct MACH0_(SObjcProperty));
		offset += sizeof (struct MACH0_(SObjcProperty));
	}
	return;
error:
	R_FREE (property);
	R_FREE (name);
	return;
}

///////////////////////////////////////////////////////////////////////////////
static void get_method_list_t(mach0_ut p, RBinFile *arch, char *class_name, RBinClass *klass, bool is_static) {
	struct MACH0_(SMethodList) ml;
	struct MACH0_(SMethod) m;
	mach0_ut r;
	ut32 offset, left, i;
	char *name = NULL;
	int len;
	bool bigendian;
	ut8 sml[sizeof (struct MACH0_(SMethodList))] = {0};
	ut8 sm[sizeof (struct MACH0_(SMethod))] = {0};

	RBinSymbol *method = NULL;
	if (!arch || !arch->o || !arch->o->bin_obj || !arch->o->info) {
		eprintf ("incorrect RBinFile pointer\n");
		return;
	}
	bigendian = arch->o->info->big_endian;
	r = get_pointer (p, &offset, &left, arch);
	if (!r) {
		return;
	}
	memset (&ml, '\0', sizeof (struct MACH0_(SMethodList)));

	if (r + left < r || r + sizeof (struct MACH0_(SMethodList)) < r) {
		return;
	}
	if (r > arch->size || r + left > arch->size) {
		return;
	}
	if (r + sizeof (struct MACH0_(SMethodList)) > arch->size) {
		return;
	}
	if (left < sizeof (struct MACH0_(SMethodList))) {
		if (r_buf_read_at (arch->buf, r, sml, left) != left) {
			return;
		}
	} else {
		len = r_buf_read_at (arch->buf, r, sml, sizeof (struct MACH0_(SMethodList)));
		if (len != sizeof (struct MACH0_(SMethodList))) {
			return;
		}
	}
	ml.entsize = r_read_ble (&sml[0], bigendian, 32);
	ml.count = r_read_ble (&sml[4], bigendian, 32);

	p += sizeof (struct MACH0_(SMethodList));
	offset += sizeof (struct MACH0_(SMethodList));
	for (i = 0; i < ml.count; i++) {
		r = get_pointer (p, &offset, &left, arch);
		if (!r) {
			return;
		}

		if (!(method = R_NEW0 (RBinSymbol))) {
			// retain just for debug
			// eprintf ("RBinClass allocation error\n");
			return;
		}
		memset (&m, '\0', sizeof (struct MACH0_(SMethod)));
		if (r + left < r || r + sizeof (struct MACH0_(SMethod)) < r) {
			goto error;
		}
		if (r > arch->size || r + left > arch->size) {
			goto error;
		}
		if (r + sizeof (struct MACH0_(SMethod)) > arch->size) {
			goto error;
		}
		if (left < sizeof (struct MACH0_(SMethod))) {
			if (r_buf_read_at (arch->buf, r, sm, left) != left) {
				goto error;
			}
		} else {
			len = r_buf_read_at (arch->buf, r, sm, sizeof (struct MACH0_(SMethod)));
			if (len != sizeof (struct MACH0_(SMethod))) {
				goto error;
			}
		}
		m.name = r_read_ble (&sm[0], bigendian, 8 * sizeof (mach0_ut));
		m.types = r_read_ble (&sm[sizeof (mach0_ut)], bigendian, 8 * sizeof (mach0_ut));
		m.imp = r_read_ble (&sm[2 * sizeof (mach0_ut)], bigendian, 8 * sizeof (mach0_ut));

		r = get_pointer (m.name, NULL, &left, arch);
		if (r) {
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;
			if (r + left < r) {
				goto error;
			}
			if (r > arch->size || r + left > arch->size) {
				goto error;
			}
			if (bin->has_crypto) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = malloc (left + 1);
				len = r_buf_read_at (arch->buf, r, (ut8 *)name, left);
				name[left] = 0;
				if (len < 1) {
					goto error;
				}
			}
			copy_sym_name_with_namespace (class_name, name, method);
			R_FREE (name);
		}
#if OBJC_UNNECESSARY
		/* @12@0:4^{   _xmlAttr=^vi*^{   _xmlNode   }^{   _xmlNode   }^{   _xmlNode   }^{   _xmlAttr   }^{   _xmlAttr   }^{   _xmlDoc   }^{   _xmlNs   }i^v   }8) */
		/* @8@0:4 */
		r = get_pointer (m.types, NULL, &left, arch);
		if (r != 0) {
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;

			if (r + left < r || r > arch->size || r + left > arch->size) {
				goto error;
			}
			if (bin->has_crypto) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = malloc (left + 1);
				if (!name) {
					goto error;
				}
				if (r_buf_read_at (arch->buf, r, (ut8 *)name, left) != left) {
					free (name);
					goto error;
				}
				name[left] = 0;
			}
			R_FREE (name);
		}
#endif
		method->vaddr = m.imp;
		method->type = is_static
			? "FUNC"
			: "METH";
		if (is_thumb (arch)) {
			if (method->vaddr & 1) {
				method->vaddr >>= 1;
				method->vaddr <<= 1;
				//eprintf ("0x%08llx METHOD %s\n", method->vaddr, method->name);
			}
		}
		r_list_append (klass->methods, method);
		p += sizeof (struct MACH0_(SMethod));
		offset += sizeof (struct MACH0_(SMethod));
	}
	return;
error:
	R_FREE (method);
	R_FREE (name);
	return;
}

///////////////////////////////////////////////////////////////////////////////
static void get_protocol_list_t(mach0_ut p, RBinFile *arch, RBinClass *klass) {
	struct MACH0_(SProtocolList) pl = { 0 };
	struct MACH0_(SProtocol) pc;
	char *class_name = NULL;
	ut32 offset, left, i, j;
	mach0_ut q, r;
	int len;
	bool bigendian;
	ut8 spl[sizeof (struct MACH0_(SProtocolList))] = {0};
	ut8 spc[sizeof (struct MACH0_(SProtocol))] = {0};
	ut8 sptr[sizeof (mach0_ut)] = {0};

	if (!arch || !arch->o || !arch->o->bin_obj || !arch->o->info) {
		eprintf ("get_protocol_list_t: Invalid RBinFile pointer\n");
		return;
	}
	bigendian = arch->o->info->big_endian;
	if (!(r = get_pointer (p, &offset, &left, arch))) {
		return;
	}

	if (r + left < r || r + sizeof (struct MACH0_(SProtocolList)) < r) {
		return;
	}
	if (r > arch->size || r + left > arch->size) {
		return;
	}
	if (r + sizeof (struct MACH0_(SProtocolList)) > arch->size) {
		return;
	}
	if (left < sizeof (struct MACH0_(SProtocolList))) {
		if (r_buf_read_at (arch->buf, r, spl, left) != left) {
			return;
		}
	} else {
		len = r_buf_read_at (arch->buf, r, spl, sizeof (struct MACH0_(SProtocolList)));
		if (len != sizeof (struct MACH0_(SProtocolList))) {
			return;
		}
	}
	pl.count = r_read_ble (&spl[0], bigendian, 8 * sizeof (mach0_ut));

	p += sizeof (struct MACH0_(SProtocolList));
	offset += sizeof (struct MACH0_(SProtocolList));
	for (i = 0; i < pl.count; i++) {
		if (!(r = get_pointer (p, &offset, &left, arch))) {
			return;
		}
		q = 0;
		if (r + left < r || r + sizeof (mach0_ut) < r) {
			return;
		}
		if (r > arch->size || r + left > arch->size) {
			return;
		}
		if (r + sizeof (mach0_ut) > arch->size) {
			return;
		}
		if (left < sizeof (ut32)) {
			if (r_buf_read_at (arch->buf, r, sptr, left) != left) {
				return;
			}
		} else {
			len = r_buf_read_at (arch->buf, r, sptr, sizeof (mach0_ut));
			if (len != sizeof (mach0_ut)) {
				return;
			}
		}
		q = r_read_ble (&sptr[0], bigendian, 8 * sizeof (mach0_ut));
		if (!(r = get_pointer (q, &offset, &left, arch))) {
			return;
		}
		memset (&pc, '\0', sizeof (struct MACH0_(SProtocol)));
		if (r + left < r || r + sizeof (struct MACH0_(SProtocol)) < r) {
			return;
		}
		if (r > arch->size || r + left > arch->size) {
			return;
		}
		if (r + sizeof (struct MACH0_(SProtocol)) > arch->size) {
			return;
		}
		if (left < sizeof (struct MACH0_(SProtocol))) {
			if (r_buf_read_at (arch->buf, r, spc, left) != left) {
				return;
			}
		} else {
			len = r_buf_read_at (arch->buf, r, spc, sizeof (struct MACH0_(SProtocol)));
			if (len != sizeof (struct MACH0_(SProtocol))) {
				return;
			}
		}
		j = 0;
		pc.isa = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.name = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.protocols = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.instanceMethods = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.classMethods = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.optionalInstanceMethods = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.optionalClassMethods = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));
		j += sizeof (mach0_ut);
		pc.instanceProperties = r_read_ble (&spc[j], bigendian, 8 * sizeof (mach0_ut));

		r = get_pointer (pc.name, NULL, &left, arch);
		if (r != 0) {
			char *name = NULL;
			struct MACH0_(obj_t) *bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;
			if (r + left < r) {
				return;
			}
			if (r > arch->size || r + left > arch->size) {
				return;
			}
			if (bin->has_crypto) {
				name = strdup ("some_encrypted_data");
				left = strlen (name) + 1;
			} else {
				name = malloc (left);
				if (!name) {
					return;
				}
				if (r_buf_read_at (arch->buf, r, (ut8 *)name, left) != left) {
					R_FREE (name);
					return;
				}
			}
			class_name = r_str_newf ("%s::%s%s", klass->name, "(protocol)", name);
			R_FREE (name);
		}

		if (pc.instanceMethods > 0) {
			get_method_list_t (pc.instanceMethods, arch, class_name, klass, false);
		}
		if (pc.classMethods > 0) {
			get_method_list_t (pc.classMethods, arch, class_name, klass, true);
		}
		R_FREE (class_name);
		p += sizeof (ut32);
		offset += sizeof (ut32);
	}
}

static const char *skipnum(const char *s) {
	while (IS_NUMBER (*s)) s++;
	return s;
}

// TODO: split up between module + classname
static char *demangle_classname(const char *s) {
	int modlen, len;
	const char *kstr;
	char *ret, *klass, *module;
	if (!strncmp (s, "_TtC", 4)) {
		len = atoi (s + 4);
		modlen = strlen (s + 4);
		if (len >= modlen) {
			return strdup (s);
		}
		module = r_str_ndup (skipnum (s + 4), len);
		kstr = skipnum (s + 4) + len;
		len = atoi (kstr);
		modlen = strlen (kstr);
		if (len >= modlen) {
			free (module);
			return strdup (s);
		}
		klass = r_str_ndup (skipnum (kstr), len);
		ret = r_str_newf ("%s.%s", module, klass);
		free (module);
		free (klass);
	} else {
		ret = strdup (s);
	}
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
static void get_class_ro_t(mach0_ut p, RBinFile *arch, ut32 *is_meta_class, RBinClass *klass) {
	struct MACH0_(obj_t) * bin;
	struct MACH0_(SClassRoT) cro = { 0 };
	ut32 offset, left, i;
	ut64 r, s;
	int len;
	bool bigendian;
	ut8 scro[sizeof (struct MACH0_(SClassRoT))] = {0};

	if (!arch || !arch->o || !arch->o->bin_obj || !arch->o->info) {
		eprintf ("Invalid RBinFile pointer\n");
		return;
	}
	bigendian = arch->o->info->big_endian;
	bin = (struct MACH0_(obj_t) *)arch->o->bin_obj;
	if (!(r = get_pointer (p, &offset, &left, arch))) {
		// eprintf ("No pointer\n");
		return;
	}

	if (r + left < r || r + sizeof (cro) < r) {
		return;
	}
	if (r > arch->size || r + left > arch->size) {
		return;
	}
	if (r + sizeof (cro) > arch->size) {
		return;
	}

	// TODO: use r_buf_fread to avoid endianness issues
	if (left < sizeof (cro)) {
		eprintf ("Not enough data for SClassRoT\n");
		return;
	}
	len = r_buf_read_at (arch->buf, r, scro, sizeof (cro));
	if (len < 1) {
		return;
	}
	i = 0;
	cro.flags = r_read_ble (&scro[i], bigendian, 8 * sizeof (ut32));
	i += sizeof (ut32);
	cro.instanceStart = r_read_ble (&scro[i], bigendian, 8 * sizeof (ut32));
	i += sizeof (ut32);
	cro.instanceSize = r_read_ble (&scro[i], bigendian, 8 * sizeof (ut32));
	i += sizeof (ut32);
#ifdef R_BIN_MACH064
	cro.reserved = r_read_ble (&scro[i], bigendian, 8 * sizeof (ut32));
	i += sizeof (ut32);
#endif
	cro.ivarLayout = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	cro.name = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	cro.baseMethods = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	cro.baseProtocols = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	cro.ivars = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	cro.weakIvarLayout = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	cro.baseProperties = r_read_ble (&scro[i], bigendian, 8 * sizeof (mach0_ut));

	s = r;
	if ((r = get_pointer (cro.name, NULL, &left, arch))) {
		if (left < 1 || r + left < r) {
			return;
		}
		if (r > arch->size || r + left > arch->size) {
			return;
		}
		if (bin->has_crypto) {
			klass->name = strdup ("some_encrypted_data");
			left = strlen (klass->name) + 1;
		} else {
			char *name = malloc (left + 1);
			if (name) {
				int rc = r_buf_read_at (arch->buf, r, (ut8 *)name, left);
				if (rc != left) {
					rc = 0;
				}
				name[rc] = 0;
				klass->name = demangle_classname (name);
				free (name);
			}
		}
		//eprintf ("0x%x  %s\n", s, klass->name);
		sdb_num_set (bin->kv, sdb_fmt (0, "objc_class_%s.offset", klass->name), s, 0);
	}
#ifdef R_BIN_MACH064
	sdb_set (bin->kv, sdb_fmt (0, "objc_class.format", 0), "lllll isa super cache vtable data", 0);
#else
	sdb_set (bin->kv, sdb_fmt (0, "objc_class.format", 0), "xxxxx isa super cache vtable data", 0);
#endif

	if (cro.baseMethods > 0) {
		get_method_list_t (cro.baseMethods, arch, klass->name, klass, false);
	}

	if (cro.baseProtocols > 0) {
		get_protocol_list_t (cro.baseProtocols, arch, klass);
	}

	if (cro.ivars > 0) {
		get_ivar_list_t (cro.ivars, arch, klass);
	}

	if (cro.baseProperties > 0) {
		get_objc_property_list (cro.baseProperties, arch, klass);
	}

	if (is_meta_class) {
		*is_meta_class = (cro.flags & RO_META)? 1: 0;
	}
}

static mach0_ut get_isa_value() {
	// TODO: according to otool sources this is taken from relocs
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
static void get_class_t(mach0_ut p, RBinFile *arch, RBinClass *klass, bool dupe) {
	struct MACH0_(SClass) c = { 0 };
	const int size = sizeof (struct MACH0_(SClass));
	mach0_ut r = 0;
	ut32 offset = 0, left = 0;
	ut32 is_meta_class = 0;
	int len;
	bool bigendian;
	ut8 sc[sizeof (struct MACH0_(SClass))] = {0};
	ut32 i;

	if (!arch || !arch->o || !arch->o->info) {
		return;
	}
	bigendian = arch->o->info->big_endian;

	if (!(r = get_pointer (p, &offset, &left, arch))) {
		return;
	}
	if ((r + left) < r || (r + size) < r) {
		return;
	}
	if (r > arch->size || r + left > arch->size) {
		return;
	}
	if (r + size > arch->size) {
		return;
	}

	if (left < size) {
		eprintf ("Cannot parse obj class info out of bounds\n");
		return;
	}
	len = r_buf_read_at (arch->buf, r, sc, size);
	if (len != size) {
		return;
	}

	i = 0;
	c.isa = r_read_ble (&sc[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	c.superclass = r_read_ble (&sc[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	c.cache = r_read_ble (&sc[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	c.vtable = r_read_ble (&sc[i], bigendian, 8 * sizeof (mach0_ut));
	i += sizeof (mach0_ut);
	c.data = r_read_ble (&sc[i], bigendian, 8 * sizeof (mach0_ut));

	klass->addr = c.isa;
	get_class_ro_t (c.data & ~0x3, arch, &is_meta_class, klass);

#if SWIFT_SUPPORT
	if (q (c.data + n_value) & 7) {
		eprintf ("This is a Swift class");
	}
#endif
	if (!is_meta_class && !dupe) {
		mach0_ut isa_n_value = get_isa_value ();
		ut64 tmp = klass->addr;
		get_class_t (c.isa + isa_n_value, arch, klass, true);
		klass->addr = tmp;
	}
}

///////////////////////////////////////////////////////////////////////////////
static void __r_bin_class_free(RBinClass *p) {
	RBinSymbol *symbol = NULL;
	RBinField *field = NULL;
	RListIter *iter = NULL;

	if (!p) {
		return;
	}
	r_list_foreach (p->methods, iter, symbol) {
		free (symbol->name);
		free (symbol->classname);
		R_FREE (symbol);
	}
	r_list_foreach (p->fields, iter, field) {
		free (field->name);
		R_FREE (field);
	}
	r_list_free (p->methods);
	r_list_free (p->fields);
	r_bin_class_free (p);
}

#if 0
static RList *parse_swift_classes(RBinFile *bf) {
	bool is_swift = false;
	RBinString *str;
	RListIter *iter;
	RBinClass *cls;
	RList *ret;
	char *lib;

	r_list_foreach (bf->o->libs, iter, lib) {
		if (strstr (lib, "libswift")) {
			is_swift = true;
			break;
		}
	}
	if (!is_swift) {
		return NULL;
	}

	int idx = 0;
	ret = r_list_newf (r_bin_string_free);
	r_list_foreach (bf->o->strings, iter, str) {
		if (!strncmp (str->string, "_TtC", 4)) {
			char *msg = strdup (str->string + 4);
			cls = R_NEW0 (RBinClass);
			cls->name = strdup (msg);
			cls->super = strdup (msg);
			cls->index = idx++;
			r_list_append (ret, cls);
			free (msg);
		}
	}
	return ret;
}
#endif

RList *MACH0_(parse_classes)(RBinFile *arch) {
	RList /*<RBinClass>*/ *ret = NULL;
	ut64 num_of_unnamed_class = 0;
	RBinClass *klass = NULL;
	RListIter *iter = NULL;
	RBinSection *s = NULL;
	ut32 i = 0, size = 0;
	RList *sctns = NULL;
	bool is_found = false;
	mach0_ut p = 0;
	ut32 left = 0;
	int len;
	bool bigendian;
	ut8 pp[sizeof (mach0_ut)] = {0};

	if (!arch || !arch->o || !arch->o->bin_obj || !arch->o->info) {
		return NULL;
	}
	bigendian = arch->o->info->big_endian;

	/* check if it's Swift */
	//ret = parse_swift_classes (arch);

	// searching of section with name __objc_classlist
	if (!(sctns = r_bin_plugin_mach.sections (arch))) {
		// retain just for debug
		// eprintf ("there is no sections\n");
		return ret;
	}

	r_list_foreach (sctns, iter, s) {
		if (strstr (s->name, "__objc_classlist") != NULL) {
			is_found = true;
			break;
		}
	}

	if (!is_found) {
		// retain just for debug
		// eprintf ("there is no section __objc_classlist\n");
		goto get_classes_error;
	}
	// end of seaching of section with name __objc_classlist

	if (!ret && !(ret = r_list_new ())) {
		// retain just for debug
		// eprintf ("RList<RBinClass> allocation error\n");
		goto get_classes_error;
	}

	ret->free = (RListFree)__r_bin_class_free;

	// start of getting information about each class in file
	for (i = 0; i < s->size; i += sizeof (mach0_ut)) {
		if (!(klass = R_NEW0 (RBinClass))) {
			// retain just for debug
			// eprintf ("RBinClass allocation error\n");
			goto get_classes_error;
		}

		if (!(klass->methods = r_list_new ())) {
			// retain just for debug
			// eprintf ("RList<RBinField> allocation error\n");
			goto get_classes_error;
		}

		if (!(klass->fields = r_list_new ())) {
			// retain just for debug
			// eprintf ("RList<RBinSymbol> allocation error\n");
			goto get_classes_error;
		}
		p = 0;
		left = s->size - i;
		if (left < sizeof (mach0_ut)) {
			eprintf ("Chopped classlist data\n");
			break;
		}
		size = sizeof (mach0_ut);

		if (s->paddr > arch->size || s->paddr + size > arch->size) {
			goto get_classes_error;
		}
		if (s->paddr + size < s->paddr) {
			goto get_classes_error;
		}
		len = r_buf_read_at (arch->buf, s->paddr + i, pp, sizeof (mach0_ut));
		if (len != sizeof (mach0_ut)) {
			goto get_classes_error;
		}
		p = r_read_ble (&pp[0], bigendian, 8 * sizeof (mach0_ut));
		get_class_t (p, arch, klass, false);
		if (!klass->name) {
			klass->name = r_str_newf ("UnnamedClass%" PFMT64d, num_of_unnamed_class);
			if (!klass->name) {
				goto get_classes_error;
			}
			num_of_unnamed_class++;
		}
		r_list_append (ret, klass);
	}
	r_list_free (sctns);
	return ret;

get_classes_error:
	r_list_free (sctns);
	r_list_free (ret);
	__r_bin_class_free (klass);
	return NULL;
}
