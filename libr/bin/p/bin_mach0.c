/* radare - LGPL - Copyright 2009-2011 pancake<@nopcode.org> */

#include <r_types.h>
#include <r_util.h>
#include <r_lib.h>
#include <r_bin.h>
#include "mach0/mach0.h"

static int load(RBinArch *arch) {
	if(!(arch->bin_obj = MACH0_(r_bin_mach0_new_buf) (arch->buf)))
		return R_FALSE;
	return R_TRUE;
}

static int destroy(RBinArch *arch) {
	MACH0_(r_bin_mach0_free) (arch->bin_obj);
	return R_TRUE;
}

static ut64 baddr(RBinArch *arch) {
	return MACH0_(r_bin_mach0_get_baddr) (arch->bin_obj);
}

static RList* entries(RBinArch *arch) {
	RList *ret;
	RBinAddr *ptr = NULL;
	struct r_bin_mach0_addr_t *entry = NULL;

	if (!(ret = r_list_new ()))
		return NULL;
	ret->free = free;
	if (!(entry = MACH0_(r_bin_mach0_get_entrypoint) (arch->bin_obj)))
		return ret;
	if ((ptr = R_NEW (RBinAddr))) {
		memset (ptr, '\0', sizeof (RBinAddr));
		ptr->offset = entry->offset;
		ptr->rva = entry->addr;
		r_list_append (ret, ptr);
	}
	free (entry);
	return ret;
}

static RList* sections(RBinArch *arch) {
	RList *ret = NULL;
	RBinSection *ptr = NULL;
	struct r_bin_mach0_section_t *sections = NULL;
	int i;

	if (!(ret = r_list_new ()))
		return NULL;
	ret->free = free;
	if (!(sections = MACH0_(r_bin_mach0_get_sections) (arch->bin_obj)))
		return ret;
	for (i = 0; !sections[i].last; i++) {
		if (!(ptr = R_NEW (RBinSection)))
			break;
		strncpy (ptr->name, (char*)sections[i].name, R_BIN_SIZEOF_STRINGS);
		ptr->size = sections[i].size;
		ptr->vsize = sections[i].size;
		ptr->offset = sections[i].offset;
		ptr->rva = sections[i].addr;
		ptr->srwx = sections[i].srwx;
		r_list_append (ret, ptr);
	}
	free (sections);
	return ret;
}

static RList* symbols(RBinArch *arch) {
	RList *ret = NULL;
	RBinSymbol *ptr = NULL;
	struct r_bin_mach0_symbol_t *symbols = NULL;
	int i;

	if (!(ret = r_list_new ()))
		return NULL;
	ret->free = free;
	if (!(symbols = MACH0_(r_bin_mach0_get_symbols) (arch->bin_obj)))
		return ret;
	for (i = 0; !symbols[i].last; i++) {
		if (!(ptr = R_NEW (RBinSymbol)))
			break;
		strncpy (ptr->name, (char*)symbols[i].name, R_BIN_SIZEOF_STRINGS);
		strncpy (ptr->forwarder, "NONE", R_BIN_SIZEOF_STRINGS);
		strncpy (ptr->bind, "NONE", R_BIN_SIZEOF_STRINGS);
		strncpy (ptr->type, "FUNC", R_BIN_SIZEOF_STRINGS); //XXX Get the right type
		if (symbols[i].type == R_BIN_MACH0_SYMBOL_TYPE_LOCAL)
			strncat (ptr->type, "_LOCAL", R_BIN_SIZEOF_STRINGS);
		ptr->rva = symbols[i].addr;
		ptr->offset = symbols[i].offset;
		ptr->size = symbols[i].size;
		ptr->ordinal = 0;
		r_list_append (ret, ptr);
	}
	free (symbols);
	return ret;
}

static RList* imports(RBinArch *arch) {
	RList *ret = NULL;
	RBinImport *ptr = NULL;
	struct r_bin_mach0_import_t *imports = NULL;
	int i;

	if (!(ret = r_list_new ()))
		return NULL;
	ret->free = free;
	if (!(imports = MACH0_(r_bin_mach0_get_imports) (arch->bin_obj)))
		return ret;
	for (i = 0; !imports[i].last; i++) {
		if (!(ptr = R_NEW (RBinImport)))
			break;
		strncpy (ptr->name, (char*)imports[i].name, R_BIN_SIZEOF_STRINGS);
		strncpy (ptr->bind, "NONE", R_BIN_SIZEOF_STRINGS);
		if (imports[i].type == R_BIN_MACH0_IMPORT_TYPE_FUNC)
			strncpy (ptr->type, "FUNC", R_BIN_SIZEOF_STRINGS);
		else strncpy (ptr->type, "OBJECT", R_BIN_SIZEOF_STRINGS);
		ptr->rva = imports[i].addr;
		ptr->offset = imports[i].offset;
		ptr->size = 0;
		ptr->ordinal = 0;
		ptr->hint = 0;
		r_list_append (ret, ptr);
	}
	free (imports);
	return ret;
}

static RList* libs(RBinArch *arch) {
	int i;
	char *ptr = NULL;
	struct r_bin_mach0_lib_t *libs;
	RList *ret = r_list_new ();
	if (!ret) return NULL;
	ret->free = free;
	if ((libs = MACH0_(r_bin_mach0_get_libs) (arch->bin_obj))) {
		for (i = 0; !libs[i].last; i++) {
			ptr = strdup (libs[i].name);
			r_list_append (ret, ptr);
		}
		free (libs);
	}
	return ret;
}

static RBinInfo* info(RBinArch *arch) {
	char *str;
	RBinInfo *ret = NULL;

	if ((ret = R_NEW (RBinInfo)) == NULL)
		return NULL;
	memset (ret, '\0', sizeof (RBinInfo));
	strncpy (ret->file, arch->file, R_BIN_SIZEOF_STRINGS);
	strncpy (ret->rpath, "NONE", R_BIN_SIZEOF_STRINGS);
	if ((str = MACH0_(r_bin_mach0_get_class) (arch->bin_obj))) {
		strncpy (ret->bclass, str, R_BIN_SIZEOF_STRINGS);
		free (str);
	}
	strncpy(ret->rclass, "mach0", R_BIN_SIZEOF_STRINGS);
	/* TODO get os*/
	strncpy (ret->os, "darwin", R_BIN_SIZEOF_STRINGS);
	strncpy (ret->subsystem, "darwin", R_BIN_SIZEOF_STRINGS);
	if ((str = MACH0_(r_bin_mach0_get_cputype) (arch->bin_obj))) {
		strncpy (ret->arch, str, R_BIN_SIZEOF_STRINGS);
		free (str);
	}
	if ((str = MACH0_(r_bin_mach0_get_cpusubtype) (arch->bin_obj))) {
		strncpy (ret->machine, str, R_BIN_SIZEOF_STRINGS);
		free (str);
	}
	if ((str = MACH0_(r_bin_mach0_get_filetype) (arch->bin_obj))) {
		strncpy (ret->type, str, R_BIN_SIZEOF_STRINGS);
		free (str);
	}
	ret->bits = MACH0_(r_bin_mach0_get_bits) (arch->bin_obj);
	ret->big_endian = MACH0_(r_bin_mach0_is_big_endian) (arch->bin_obj);
	/* TODO detailed debug info */
	ret->dbg_info = 0;
	return ret;
}

#if !R_BIN_MACH064
static int check(RBinArch *arch) {
	if (!memcmp (arch->buf->buf, "\xce\xfa\xed\xfe", 4) ||
		!memcmp (arch->buf->buf, "\xfe\xed\xfa\xce", 4))
		return R_TRUE;
	return R_FALSE;
}

#if 0
typedef struct r_bin_create_t {
	int arch;
	ut8 *code;
	int codelen;
	ut8 *data;
	int datalen;
} RBinCreate;
#endif

static RBuffer* create(RBin* bin, const ut8 *code, int codelen, const ut8 *data, int datalen) {
	ut32 filesize, codeva, datava;
	ut32 ncmds, cmdsize, magiclen;
	ut32 p_codefsz, p_codeva, p_codesz, p_codepa;
	ut32 p_datafsz, p_datava, p_datasz, p_datapa;
	ut32 p_cmdsize, p_entry, p_tmp;
	ut32 baddr = 0x1000;
	int is_arm = !strcmp (bin->curarch.info->arch, "arm");
	RBuffer *buf = r_buf_new ();
	if (bin->curarch.info->bits == 64) {
		eprintf ("TODO: Please use mach064 instead of mach0\n");
		return 0;
	}

#define B(x,y) r_buf_append_bytes(buf,(const ut8*)x,y)
#define D(x) r_buf_append_ut32(buf,x)
#define Z(x) r_buf_append_nbytes(buf,x)
#define W(x,y,z) r_buf_write_at(buf,x,(const ut8*)y,z)
#define WZ(x,y) p_tmp=buf->length;Z(x);W(p_tmp,y,strlen(y))

	/* MACH0 HEADER */
	B ("\xce\xfa\xed\xfe", 4); // header
	if (is_arm) {
		D (12); // cpu type (arm)
		D (3); // subtype (all?)
	} else {
		/* x86-32 */
		D (7); // cpu type (x86)
		D (3); // subtype (i386-all)
	}
	D (2); // filetype (executable)

	if (data && datalen>0) {
		ncmds = 3;
		cmdsize = 0;
	} else {
		ncmds = 2;
		cmdsize = 0;
	}
	
	/* COMMANDS */
	D (ncmds); // ncmds
	p_cmdsize = buf->length;
	D (-1); // cmdsize
	D (0); // flags
	magiclen = buf->length;

	/* TEXT SEGMENT */
	D (1);   // cmd.LC_SEGMENT
	D (124); // sizeof (cmd)
	WZ (16, "__TEXT");
	D (baddr); // vmaddr
	D (0x1000); // vmsize XXX
	D (0); // fileoff
	p_codefsz = buf->length;
	D (-1); // filesize
	D (7); // maxprot
	D (5); // initprot
	D (1); // nsects
	D (0); // flags
	WZ (16, "__text");
	WZ (16, "__TEXT");
	p_codeva = buf->length; // virtual address
	D (-1);
	p_codesz = buf->length; // size of code (end-start)
	D (-1);
	p_codepa = buf->length; // code - baddr
	D (-1); //_start-0x1000);
	D (2); // align
	D (0); // reloff
	D (0); // nrelocs
	D (0); // flags
	D (0); // reserved
	D (0);

	if (data && datalen>0) {
		/* DATA SEGMENT */
		D (1);   // cmd.LC_SEGMENT
		D (124); // sizeof (cmd)
		p_tmp = buf->length;
		Z (16);
		W (p_tmp, "__TEXT", 6); // segment name
		D (0x2000); // vmaddr
		D (0x1000); // vmsize
		D (0); // fileoff
		p_datafsz = buf->length;
		D (-1); // filesize
		D (6); // maxprot
		D (6); // initprot
		D (1); // nsects
		D (0); // flags

		WZ (16, "__data");
		WZ (16, "__DATA");

		p_datava = buf->length;
		D (-1);
		p_datasz = buf->length;
		D (-1);
		p_datapa = buf->length;
		D (-1); //_start-0x1000);
		D (2); // align
		D (0); // reloff
		D (0); // nrelocs
		D (0); // flags
		D (0); // reserved
		D (0);
	}

	/* THREAD STATE */
	D (5); // LC_UNIXTHREAD
	D (80); // sizeof (cmd)
	if (is_arm) {
		/* arm */
		D (1); // i386-thread-state
		D (17); // thread-state-count
		p_entry = buf->length + (16*sizeof (ut32));
		Z (17 * sizeof (ut32));
		// mach0-arm has one byte more
	} else {
		/* x86-32 */
		D (1); // i386-thread-state
		D (16); // thread-state-count
		p_entry = buf->length + (10*sizeof (ut32));
		Z (16 * sizeof (ut32));
	}

	cmdsize = buf->length - magiclen;

	codeva = buf->length + baddr;
	datava = buf->length + codelen + baddr;
	W (p_entry, &codeva, 4); // set PC

	/* fill header variables */
	W (p_cmdsize, &cmdsize, 4);
	filesize = magiclen + cmdsize + codelen + datalen;
	// TEXT SEGMENT //
	W (p_codefsz, &filesize, 4);
	W (p_codeva, &codeva, 4);
	W (p_codesz, &codelen, 4);
	p_tmp = codeva - baddr;
	W (p_codepa, &p_tmp, 4);

	B (code, codelen);

	if (data && datalen>0) {
		/* append data */
		W (p_datafsz, &filesize, 4);
		W (p_datava, &datava, 4);
		W (p_datasz, &datalen, 4);
		p_tmp = datava - baddr;
		W (p_datapa, &p_tmp, 4);
		B (data, datalen);
	}

	return buf;
}

static RBinAddr* binsym(RBinArch *arch, int sym) {
	ut64 addr;
	RBinAddr *ret = NULL;
	switch (sym) {
	case R_BIN_SYM_MAIN:
		addr = MACH0_(r_bin_mach0_get_main) (arch->bin_obj);
		if (!addr || !(ret = R_NEW (RBinAddr)))
			return NULL;
		memset (ret, '\0', sizeof (RBinAddr));
		ret->offset = ret->rva = addr;
		break;
	}
	return ret;
}

struct r_bin_plugin_t r_bin_plugin_mach0 = {
	.name = "mach0",
	.desc = "mach0 bin plugin",
	.init = NULL,
	.fini = NULL,
	.load = &load,
	.destroy = &destroy,
	.check = &check,
	.baddr = &baddr,
	.binsym = &binsym,
	.entries = &entries,
	.sections = &sections,
	.symbols = &symbols,
	.imports = &imports,
	.strings = NULL,
	.info = &info,
	.fields = NULL,
	.libs = &libs,
	.relocs = NULL,
	.meta = NULL,
	.write = NULL,
	.create = &create,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_BIN,
	.data = &r_bin_plugin_mach0
};
#endif
#endif
