/* radare - LGPL - Copyright 2009-2011 nibble<.ds@gmail.com> */

/* TODO:
 * Use -v to show version information.. not -V .. like the rest of tools
 *  --- needs sync with callers and so on..
 * -L [lib]  dlopen library and show addr
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <r_types.h>
#include <r_list.h>
#include <r_lib.h>
#include <r_bin.h>
#include <r_flags.h>
#include <r_util.h>

#define ACTION_UNK       0x00000
#define ACTION_ENTRIES   0x00001
#define ACTION_IMPORTS   0x00002
#define ACTION_SYMBOLS   0x00004
#define ACTION_SECTIONS  0x00008
#define ACTION_INFO      0x00010
#define ACTION_OPERATION 0x00020
#define ACTION_HELP      0x00040
#define ACTION_STRINGS   0x00080
#define ACTION_FIELDS    0x00100
#define ACTION_LIBS      0x00200
#define ACTION_SRCLINE   0x00400
#define ACTION_MAIN      0x00800
#define ACTION_EXTRACT   0x01000
#define ACTION_RELOCS    0x02000
#define ACTION_LISTARCHS 0x04000
#define ACTION_CREATE    0x08000
#define ACTION_CLASSES   0x10000

static struct r_lib_t *l;
static struct r_bin_t *bin = NULL;
static int rad = R_FALSE;
static int rw = R_FALSE;
static int va = R_FALSE;
static ut64 gbaddr = 0LL;
static char* file = NULL;
static char* output = "out";
static char* create = NULL;
static ut64 at = 0LL;
static char *name = NULL;

static int rabin_show_help() {
	printf ("rabin2 [options] [file]\n"
		" -A              list archs\n"
		" -a [arch_bits]  set arch (x86_32, arm_32, x86_64)\n"
		" -b [addr]       override baddr\n"
		" -c [fmt:C:D]    create [elf,mach0,pe] with Code and Data hexpairs (see -a)\n"
		" -C              list classes\n"
		" -p [patchfile]  patch file (see man rabin2)\n"
		" -e              entrypoint\n"
		" -f [str]        select sub-bin named str\n"
		" -i              imports (symbols imported from libraries)\n"
		" -s              symbols (exports)\n"
		" -S              sections\n"
		" -M              main (show address of main symbol)\n"
		" -z              strings\n"
		" -I              binary info\n"
		" -H              header fields\n"
		" -l              linked libraries\n"
		" -R              relocations\n"
		" -O [str]        write/extract operations (str=help for help)\n"
		" -o [str]        output file/folder for write operations (out by default)\n"
		" -r              radare output\n"
		" -v              use vaddr in radare output\n"
		" -m [addr]       show source line at addr\n"
		" -L              list supported bin plugins\n"
		" -@ [addr]       show section, symbol or import at addr\n"
		" -n [str]        show section, symbol or import named str\n"
		" -x              extract bins contained in file\n"
		" -V              show version information\n"
		" -h              this help\n");
	return 1;
}

static int rabin_show_entrypoints() {
	RList *entries;
	RListIter *iter;
	RBinAddr *entry;
	int i = 0;

	ut64 baddr = gbaddr? gbaddr: r_bin_get_baddr (bin);

	if ((entries = r_bin_get_entries (bin)) == NULL)
		return R_FALSE;

	if (rad) printf ("fs symbols\n");
	else eprintf ("[Entrypoints]\n");

	r_list_foreach (entries, iter, entry) {
		if (rad) {
			printf ("f entry%i @ 0x%08"PFMT64x"\n", i, va?baddr+entry->rva:entry->offset);
			printf ("s entry%i\n", i);
		} else printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" baddr=0x%08"PFMT64x"\n",
				baddr+entry->rva, entry->offset, baddr);
		i++;
	}

	if (!rad) eprintf ("\n%i entrypoints\n", i);

	return R_TRUE;
}

static int rabin_show_main() {
	RBinAddr *binmain;
	ut64 baddr = gbaddr? gbaddr: r_bin_get_baddr (bin);

	if ((binmain = r_bin_get_sym (bin, R_BIN_SYM_MAIN)) == NULL)
		return R_FALSE;
	if (rad) {
		printf ("fs symbols\n");
		printf ("f main @ 0x%08"PFMT64x"\n", va? baddr+binmain->rva: binmain->offset);
	} else {
		eprintf ("[Main]\n");
		printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x"\n",
			baddr+binmain->rva, binmain->offset);
	}
	return R_TRUE;
}

static int rabin_extract(int all) {
	char outfile[512], outpath[512], *path, *ptr;
	int i = 0;

	// XXX: Wrong for w32 (/)
	if (all) {
		for (i=0; i<bin->narch; i++) {
			r_bin_select_idx (bin, i);
			if (bin->curarch.info == NULL) {
				eprintf ("No extract info found.\n");
			} else {
				path = strdup (bin->curarch.file);
				if ((ptr = strrchr (path, '/'))) {
					*ptr = '\0';
					ptr = ptr+1;
				}
				else ptr = bin->curarch.file;
				snprintf (outpath, sizeof (outpath), "%s/%s", output, path);
				if (!r_sys_rmkdir (outpath)) {
					eprintf ("Error creating dir structure\n");
					return R_FALSE;
				}
				snprintf (outfile, sizeof (outfile), "%s/%s.%s_%i",
						outpath, ptr, bin->curarch.info->arch,
						bin->curarch.info->bits);
				if (!r_file_dump (outfile, bin->curarch.buf->buf, bin->curarch.size)) {
					eprintf ("Error extracting %s\n", outfile);
					return R_FALSE;
				} else printf ("%s created (%i)\n", outfile, bin->curarch.size);
			}
		}
	} else { /* XXX: Use 'output' for filename? */
		if (bin->curarch.info == NULL) {
			eprintf ("No extract info found.\n");
		} else {
			if ((ptr = strrchr (bin->curarch.file, '/')))
				ptr = ptr+1;
			else ptr = bin->curarch.file;
			snprintf (outfile, sizeof (outfile), "%s.%s_%i", ptr,
					bin->curarch.info->arch, bin->curarch.info->bits);
			if (!r_file_dump (outfile, bin->curarch.buf->buf, bin->curarch.size)) {
				eprintf ("Error extracting %s\n", outfile);
				return R_FALSE;
			} else printf ("%s created (%i)\n", outfile, bin->curarch.size);
		}
	}
	return R_TRUE;
}

static int rabin_show_libs() {
	RList *libs;
	RListIter *iter;
	char* lib;
	int i = 0;

	if ((libs = r_bin_get_libs (bin)) == NULL)
		return R_FALSE;

	eprintf ("[Linked libraries]\n");
	r_list_foreach (libs, iter, lib) {
		printf ("%s\n", lib);
		i++;
	}
	if (!rad) eprintf ("\n%i libraries\n", i);
	
	return R_TRUE;
}

static int rabin_show_relocs() {
	RList *relocs;
	RListIter *iter;
	RBinReloc *reloc;
	int i = 0;

	ut64 baddr = gbaddr?gbaddr:r_bin_get_baddr (bin);

	if ((relocs = r_bin_get_relocs (bin)) == NULL)
		return R_FALSE;

	if (rad) {
		printf ("fs relocs\n");
		r_list_foreach (relocs, iter, reloc) {
			printf ("f reloc.%s @ 0x%08"PFMT64x"\n", reloc->name, va?baddr+reloc->rva:reloc->offset);
			i++;
		}
	} else {
		eprintf ("[Relocations]\n");
		r_list_foreach (relocs, iter, reloc) {
			printf ("sym=%02i addr=0x%08"PFMT64x" off=0x%08"PFMT64x" type=0x%08x %s\n",
				reloc->sym, baddr+reloc->rva, reloc->offset, reloc->type, reloc->name);
			i++;
		}
		eprintf ("\n%i relocations\n", i);
	}

	return R_TRUE;
}

static int rabin_show_imports() {
	RList *imports;
	RListIter *iter;
	RBinImport *import;
	ut64 baddr;
	int i = 0;

	baddr = gbaddr?gbaddr:r_bin_get_baddr (bin);

	if ((imports = r_bin_get_imports (bin)) == NULL)
		return R_FALSE;

	if (!at) {
		if (rad) printf ("fs imports\n");
		else eprintf ("[Imports]\n");
	}

	r_list_foreach (imports, iter, import) {
		if (name && strcmp (import->name, name))
			continue;
		if (at) {
			if (baddr+import->rva == at || import->offset == at)
				printf ("%s\n", import->name);
		} else {
			if (rad) {
				r_name_filter (import->name, sizeof (import->name));
				if (import->size) 
					printf ("af+ 0x%08"PFMT64x" %"PFMT64d" imp.%s i\n",
							va?baddr+import->rva:import->offset, import->size, import->name);
				printf ("f imp.%s @ 0x%08"PFMT64x"\n",
						import->name, va?baddr+import->rva:import->offset);
			} else printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" ordinal=%03"PFMT64d" "
						   "hint=%03"PFMT64d" bind=%s type=%s name=%s\n",
						   baddr+import->rva, import->offset,
						   import->ordinal, import->hint,  import->bind,
						   import->type, import->name);
		}
		i++;
	}

	if (!at && !rad) eprintf ("\n%i imports\n", i);

	return R_TRUE;
}

static void rabin_show_classes() {
	RBinClass *c;
	RListIter *iter;
	RList *cs = r_bin_get_classes (bin);
	r_list_foreach (cs, iter, c) {
		if (rad) {
			printf ("f class.%s\n", c->name);
			if (c->super)
				printf ("f super.%s.%s\n", c->name, c->super);
		} else {
			printf ("class = %s\n", c->name);
			if (c->super)
				printf ("  super = %s\n", c->super);
		}
		// TODO: show belonging methods and fields
	}
}

static int rabin_show_symbols(ut64 at) {
	RList *symbols;
	RListIter *iter;
	RBinSymbol *symbol;
	ut64 baddr;
	int i = 0;

	baddr = gbaddr?gbaddr:r_bin_get_baddr (bin);

	if ((symbols = r_bin_get_symbols (bin)) == NULL)
		return R_FALSE;

	if (!at) {
		if (rad) printf ("fs symbols\n");
		else eprintf ("[Symbols]\n");
	}

	r_list_foreach (symbols, iter, symbol) {
		if (name && strcmp (symbol->name, name))
			continue;
		if (at) {
			if ((symbol->size != 0 &&
				((baddr+symbol->rva <= at && baddr+symbol->rva+symbol->size > at) ||
				(symbol->offset <= at && symbol->offset+symbol->size > at))) ||
				baddr+symbol->rva == at || symbol->offset == at)
				printf("%s\n", symbol->name);
		} else {
			if (rad) {
				char *mn = r_bin_demangle (bin, symbol->name);
				if (mn) {
					printf ("s 0x%08"PFMT64x"\n\"CC %s\"\n", symbol->offset, mn);
					free (mn);
				}
				r_name_filter (symbol->name, sizeof (symbol->name));
				if (!strncmp (symbol->type,"OBJECT", 6))
					printf ("Cd %"PFMT64d" @ 0x%08"PFMT64x"\n",
							symbol->size, va?baddr+symbol->rva:symbol->offset);
				printf ("f sym.%s %"PFMT64d" 0x%08"PFMT64x"\n",
						symbol->name, symbol->size,
						va?baddr+symbol->rva:symbol->offset);
			} else printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" ordinal=%03"PFMT64d" "
						   "forwarder=%s sz=%"PFMT64d" bind=%s type=%s name=%s\n",
						   baddr+symbol->rva, symbol->offset,
						   symbol->ordinal, symbol->forwarder,
						   symbol->size, symbol->bind, symbol->type, 
						   symbol->name);
		}
		i++;
	}

	if (!at && !rad) eprintf ("\n%i symbols\n", i);

	return R_TRUE;
}

static int rabin_show_strings() {
	RList *strings;
	RListIter *iter;
	RBinString *string;
	RBinSection *section;
	int i = 0;
	ut64 baddr = gbaddr? gbaddr: r_bin_get_baddr (bin);

	if ((strings = r_bin_get_strings (bin)) == NULL)
		return R_FALSE;

	if (rad) printf ("fs strings\n");
	else eprintf ("[strings]\n");

	r_list_foreach (strings, iter, string) {
		section = r_bin_get_section_at (bin, string->offset, 0);
		if (rad) {
			r_name_filter (string->string, sizeof (string->string));
			printf ("f str.%s %"PFMT64d" @ 0x%08"PFMT64x"\n"
				"Cs %"PFMT64d" @ 0x%08"PFMT64x"\n",
				string->string, string->size, va?baddr+string->rva:string->offset,
				string->size, va?baddr+string->rva:string->offset);
		} else printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" ordinal=%03"PFMT64d" "
			"sz=%"PFMT64d" section=%s string=%s\n",
			baddr+string->rva, string->offset,
			string->ordinal, string->size,
			section?section->name:"unknown", string->string);
		i++;
	}

	if (!rad) eprintf ("\n%i strings\n", i);
	
	return R_TRUE;
}

static int rabin_show_sections(ut64 at) {
	RList *sections;
	RListIter *iter;
	RBinSection *section;
	ut64 baddr;
	int i = 0;

	baddr = gbaddr?gbaddr:r_bin_get_baddr (bin);

	if ((sections = r_bin_get_sections (bin)) == NULL)
		return R_FALSE;

	if (!at) {
		if (rad) printf ("fs sections\n");
		else eprintf ("[Sections]\n");
	}

	r_list_foreach (sections, iter, section) {
		if (name && strcmp (section->name, name))
			continue;
		r_name_filter (section->name, sizeof (section->name));
		if (at) {
			if ((section->size != 0 &&
				((baddr+section->rva <= at && baddr+section->rva+section->size > at) ||
				(section->offset <= at && section->offset+section->size > at))) ||
				baddr+section->rva == at || section->offset == at)
				printf ("%s\n", section->name);
		} else {
			if (rad) {
				printf ("S 0x%08"PFMT64x" 0x%08"PFMT64x" 0x%08"PFMT64x" 0x%08"PFMT64x" %s %d\n",
					section->offset, baddr+section->rva,
					section->size, section->vsize, section->name, (int)section->srwx);
				printf ("f section.%s %"PFMT64d" 0x%08"PFMT64x"\n",
					section->name, section->size, va?baddr+section->rva:section->offset);
				printf ("CC [%02i] va=0x%08"PFMT64x" pa=0x%08"PFMT64x" sz=%"PFMT64d" vsz=%"PFMT64d" "
					"rwx=%c%c%c%c %s @ 0x%08"PFMT64x"\n",
					i, baddr+section->rva, section->offset, section->size, section->vsize,
					R_BIN_SCN_SHAREABLE (section->srwx)?'s':'-',
					R_BIN_SCN_READABLE (section->srwx)?'r':'-',
					R_BIN_SCN_WRITABLE (section->srwx)?'w':'-',
					R_BIN_SCN_EXECUTABLE (section->srwx)?'x':'-',
					section->name,va?baddr+section->rva:section->offset);
			} else printf ("idx=%02i addr=0x%08"PFMT64x" off=0x%08"PFMT64x" sz=%"PFMT64d" vsz=%"PFMT64d" "
				"perm=%c%c%c%c name=%s\n",
				i, baddr+section->rva, section->offset, section->size, section->vsize,
				R_BIN_SCN_SHAREABLE (section->srwx)?'s':'-',
				R_BIN_SCN_READABLE (section->srwx)?'r':'-',
				R_BIN_SCN_WRITABLE (section->srwx)?'w':'-',
				R_BIN_SCN_EXECUTABLE (section->srwx)?'x':'-',
				section->name);
		}
		i++;
	}

	if (!at && !rad) eprintf ("\n%i sections\n", i);

	return R_TRUE;
}

static int rabin_show_info() {
	RBinInfo *info;

	if ((info = r_bin_get_info (bin)) == NULL)
		return R_FALSE;

	if (rad) {
		if (!strcmp (info->type, "fs")) {
			printf ("e file.type=fs\n");
			printf ("m %s /root 0\n", info->arch);
		} else {
			printf (
			"e file.type=%s\n"
			"e cfg.bigendian=%s\n"
			"e asm.os=%s\n"
			"e asm.arch=%s\n"
			"e anal.plugin=%s\n"
			"e asm.bits=%i\n"
			"e asm.dwarf=%s\n",
				info->rclass, info->big_endian?"true":"false", info->os,
				info->arch, info->arch, info->bits,
				R_BIN_DBG_STRIPPED (info->dbg_info)?"false":"true");
		}
	} else {
		// if type is 'fs' show something different?
		eprintf ("[File info]\n");
		printf ("File=%s\n"
				"Type=%s\n"
				"Class=%s\n"
				"Arch=%s %i\n"
				"Machine=%s\n"
				"OS=%s\n"
				"Subsystem=%s\n"
				"Big endian=%s\n"
				"Stripped=%s\n"
				"Static=%s\n"
				"Line_nums=%s\n"
				"Local_syms=%s\n"
				"Relocs=%s\n"
				"RPath=%s\n",
				info->file, info->type, info->bclass,
				info->arch, info->bits, info->machine, info->os, 
				info->subsystem, info->big_endian?"True":"False",
				R_BIN_DBG_STRIPPED (info->dbg_info)?"True":"False",
				R_BIN_DBG_STATIC (info->dbg_info)?"True":"False",
				R_BIN_DBG_LINENUMS (info->dbg_info)?"True":"False",
				R_BIN_DBG_SYMS (info->dbg_info)?"True":"False",
				R_BIN_DBG_RELOCS (info->dbg_info)?"True":"False",
				info->rpath);
	}
	
	return R_TRUE;
}

static int rabin_show_fields() {
	RList *fields;
	RListIter *iter;
	RBinField *field;
	ut64 size, baddr;
	int i = 0;

	baddr = gbaddr? gbaddr: r_bin_get_baddr (bin);
	size = bin->curarch.size;

	if ((fields = r_bin_get_fields (bin)) == NULL)
		return R_FALSE;

	if (rad) printf ("fs header\n");
	else eprintf ("[Header fields]\n");

	r_list_foreach (fields, iter, field) {
		if (rad) {
			r_name_filter (field->name, sizeof (field->name));
			printf ("f header.%s @ 0x%08"PFMT64x"\n",
					field->name, va?baddr+field->rva:field->offset);
			printf ("[%02i] addr=0x%08"PFMT64x" off=0x%08"PFMT64x" name=%s\n",
					i, baddr+field->rva, field->offset, field->name);
		} else printf ("idx=%02i addr=0x%08"PFMT64x" off=0x%08"PFMT64x" name=%s\n",
					   i, baddr+field->rva, field->offset, field->name);
		i++;
	}

	if (rad) {
		/* add program header section */
		printf ("S 0 0x%"PFMT64x" 0x%"PFMT64x" 0x%"PFMT64x" ehdr rwx\n",
			 baddr, size, size);
	} else eprintf ("\n%i fields\n", i);

	return R_TRUE;
}

static int rabin_dump_symbols(int len) {
	RList *symbols;
	RListIter *iter;
	RBinSymbol *symbol;
	ut8 *buf;
	char *ret;
	int olen = len;

	if ((symbols = r_bin_get_symbols (bin)) == NULL)
		return R_FALSE;

	r_list_foreach (symbols, iter, symbol) {
		if (symbol->size != 0 && (olen > symbol->size || olen == 0))
			len = symbol->size;
		else if (symbol->size == 0 && olen == 0)
			len = 32;
		else len = olen;

		if (!(buf = malloc (len)) || !(ret = malloc(len*2+1)))
			return R_FALSE;
		r_buf_read_at (bin->curarch.buf, symbol->offset, buf, len);
		r_hex_bin2str (buf, len, ret);
		printf ("%s %s\n", symbol->name, ret);
		free (buf);
		free (ret);
	}

	return R_TRUE;
}

static int rabin_dump_sections(char *scnname) {
	RList *sections;
	RListIter *iter;
	RBinSection *section;
	ut8 *buf;
	char *ret;

	if ((sections = r_bin_get_sections (bin)) == NULL)
		return R_FALSE;

	r_list_foreach (sections, iter, section) {
		if (!strcmp (scnname, section->name)) {
			if (!(buf = malloc (section->size)) ||
					!(ret = malloc (section->size*2+1)))
				return R_FALSE;
			r_buf_read_at (bin->curarch.buf, section->offset, buf, section->size);
			r_hex_bin2str (buf, section->size, ret);
			printf ("%s\n", ret);
			free (buf);
			free (ret);
			break;
		}
	}

	return R_TRUE;
}

static int rabin_do_operation(const char *op) {
	char *arg = NULL, *ptr = NULL, *ptr2 = NULL;

	if (!strcmp (op, "help")) {
		printf ("Operation string:\n"
				"  Dump symbols: d/s/1024\n"
				"  Dump section: d/S/.text\n"
				"  Resize section: r/.data/1024\n");
		return R_FALSE;
	}
	/* Implement alloca with fixed-size buffer? */
	if (!(arg = strdup (op)))
		return R_FALSE;

	if ((ptr = strchr (arg, '/'))) {
		ptr[0] = '\0';
		ptr = ptr + 1;
		if ((ptr2 = strchr (ptr, '/'))) {
			ptr2[0] = '\0';
			ptr2 = ptr2 + 1;
		}
	}

	switch (arg[0]) {
	case 'd':
		if (!ptr)
			goto _rabin_do_operation_error;
		switch (*ptr) {
		case 's':
			if (ptr2) {
				if (!rabin_dump_symbols (r_num_math(NULL, ptr2)))
					return R_FALSE;
			} else if (!rabin_dump_symbols (0))
					return R_FALSE;
			break;
		case 'S':
			if (!ptr2)
				goto _rabin_do_operation_error;
			if (!rabin_dump_sections (ptr2))
				return R_FALSE;
			break;
		default:
			goto _rabin_do_operation_error;
		}
		break;
	case 'r':
		r_bin_wr_scn_resize (bin, ptr, r_num_math (NULL, ptr2));
		r_bin_wr_output (bin, output);
		break;
	default:
	_rabin_do_operation_error:
		eprintf ("Unknown operation. use -O help\n");
		return R_FALSE;
	}

	free (arg);

	return R_TRUE;
}

static int rabin_show_srcline(ut64 at) {
	char *srcline;
	if ((srcline = r_bin_meta_get_source_line (bin, at))) {
		printf ("%s\n", srcline);
		free (srcline);
		return R_TRUE;
	}
	return R_FALSE;
}

/* bin callback */
static int __lib_bin_cb(struct r_lib_plugin_t *pl, void *user, void *data) {
	struct r_bin_plugin_t *hand = (struct r_bin_plugin_t *)data;
	//printf(" * Added (dis)assembly plugin\n");
	r_bin_add (bin, hand);
	return R_TRUE;
}

static int __lib_bin_dt(struct r_lib_plugin_t *pl, void *p, void *u) {
	return R_TRUE;
}

/* binxtr callback */
static int __lib_bin_xtr_cb(struct r_lib_plugin_t *pl, void *user, void *data) {
	struct r_bin_xtr_plugin_t *hand = (struct r_bin_xtr_plugin_t *)data;
	//printf(" * Added (dis)assembly plugin\n");
	r_bin_xtr_add (bin, hand);
	return R_TRUE;
}

static int __lib_bin_xtr_dt(struct r_lib_plugin_t *pl, void *p, void *u) {
	return R_TRUE;
}

int main(int argc, char **argv) {
	int c, bits = 0;
	int action = ACTION_UNK;
	const char *op = NULL;
	char *arch = NULL, *arch_name = NULL;

	bin = r_bin_new ();
	l = r_lib_new ("radare_plugin");
	r_lib_add_handler (l, R_LIB_TYPE_BIN, "bin plugins",
					   &__lib_bin_cb, &__lib_bin_dt, NULL);
	r_lib_add_handler (l, R_LIB_TYPE_BIN_XTR, "bin xtr plugins",
					   &__lib_bin_xtr_cb, &__lib_bin_xtr_dt, NULL);

	{ /* load plugins everywhere */
		char *homeplugindir = r_str_home (".radare/plugins");
		r_lib_opendir (l, getenv ("LIBR_PLUGINS"));
		r_lib_opendir (l, homeplugindir);
		r_lib_opendir (l, LIBDIR"/radare2/");
	}

	while ((c = getopt (argc, argv, "Af:a:B:b:c:CMm:n:@:VisSzIHelRwO:o:p:rvLhx")) != -1) {
		switch(c) {
		case 'A':
			action |= ACTION_LISTARCHS;
			break;
		case 'a':
			if (optarg) arch = strdup (optarg);
			break;
		case 'c':
			action = ACTION_CREATE;
			create = strdup (optarg);
			break;
		case 'C':
			action |= ACTION_CLASSES;
			break;
		case 'f':
			if (optarg) arch_name = strdup (optarg);
			break;
		case 'B':
			bits = r_num_math (NULL, optarg);
			break;
		case 'm':
			at = r_num_math (NULL, optarg);
			action |= ACTION_SRCLINE;
			break;
		case 'i':
			action |= ACTION_IMPORTS;
			break;
		case 's':
			action |= ACTION_SYMBOLS;
			break;
		case 'S':
			action |= ACTION_SECTIONS;
			break;
		case 'z':
			action |= ACTION_STRINGS;
			break;
		case 'I':
			action |= ACTION_INFO;
			break;
		case 'H':
			action |= ACTION_FIELDS;
			break;
		case 'e':
			action |= ACTION_ENTRIES;
			break;
		case 'M':
			action |= ACTION_MAIN;
			break;
		case 'l':
			action |= ACTION_LIBS;
			break;
		case 'R':
			action |= ACTION_RELOCS;
			break;
		case 'x':
			action |= ACTION_EXTRACT;
			break;
		case 'w':
			rw = R_TRUE;
			break;
		case 'O':
			op = optarg;
			action |= ACTION_OPERATION;
			if (optind==argc)
				return rabin_do_operation (op);
			break;
		case 'o':
			output = optarg;
			break;
		case 'r':
			rad = R_TRUE;
			break;
		case 'v':
			va = R_TRUE;
			break;
		case 'L':
			r_bin_list (bin);
			return 1;
		case 'b':
			gbaddr = r_num_math (NULL, optarg);
			break;
		case '@':
			at = r_num_math (NULL, optarg);
			break;
		case 'n':
			name = optarg;
			break;
		case 'V':
			printf ("rabin2 v"R2_VERSION"\n");
			return 0;
		case 'h':
		default:
			action |= ACTION_HELP;
		}
	}

	file = argv[optind];
	if (action == ACTION_HELP || action == ACTION_UNK || file == NULL)
		return rabin_show_help ();

	if (arch) {
		char *ptr;
		ptr = strchr (arch, '_');
		if (ptr) {
			*ptr = '\0';
			bits = r_num_math (NULL, ptr+1);
		}
	}
	if (action & ACTION_CREATE) {
		// TODO: move in a function outside
		RBuffer *b;
		int datalen, codelen;
		ut8 *data = NULL, *code = NULL;
		char *p2, *p = strchr (create, ':');
		if (!p) {
			eprintf ("Invalid format for -c flag. Use 'format:codehexpair:datahexpair'\n");
			return 1;
		}
		*p++ = 0;
		p2 = strchr (p, ':');
		if (p2) {
			// has data
			*p2++ = 0;
			data = malloc (strlen (p2));
			datalen = r_hex_str2bin (p2, data);
		}
		code = malloc (strlen (p));
		codelen = r_hex_str2bin (p, code);
		if (!arch) arch = "x86";
		if (!bits) bits = 32;

		if (!r_bin_use_arch (bin, arch, bits, create)) {
			eprintf ("Cannot set arch\n");
			return 1;
		}
		b = r_bin_create (bin, code, codelen, data, datalen);
		if (b) {
			if (r_file_dump (file, b->buf, b->length)) {
				eprintf ("dumped %d bytes in '%s'\n", b->length, file);
				r_file_chmod (file, "+x", 0);
			} else eprintf ("error dumping into a.out\n");
			r_buf_free (b);
		} else eprintf ("Cannot create binary for this format '%s'.\n", create);
		r_bin_free (bin);
		return 0;
	}

	if (!r_bin_load (bin, file, R_FALSE) && !r_bin_load (bin, file, R_TRUE)) {
		eprintf ("r_bin: Cannot open '%s'\n", file);
		return 1;
	}

	if (action & ACTION_LISTARCHS || ((arch || bits || arch_name) &&
		!r_bin_select (bin, arch, bits, arch_name))) {
		r_bin_list_archs (bin);
		free (arch);
		free (arch_name);
		r_bin_free (bin);
		return 1;
	}

	if (action&ACTION_SECTIONS)
		rabin_show_sections (at);
	if (action&ACTION_ENTRIES)
		rabin_show_entrypoints ();
	if (action&ACTION_MAIN)
		rabin_show_main ();
	if (action&ACTION_IMPORTS)
		rabin_show_imports ();
	if (action&ACTION_CLASSES)
		rabin_show_classes ();
	if (action&ACTION_SYMBOLS)
		rabin_show_symbols (at);
	if (action&ACTION_STRINGS)
		rabin_show_strings ();
	if (action&ACTION_INFO)
		rabin_show_info ();
	if (action&ACTION_FIELDS)
		rabin_show_fields();
	if (action&ACTION_LIBS)
		rabin_show_libs ();
	if (action&ACTION_RELOCS)
		rabin_show_relocs ();
	if (action&ACTION_SRCLINE)
		rabin_show_srcline (at);
	if (action&ACTION_EXTRACT)
		rabin_extract ((arch==NULL && arch_name==NULL && bits==0));
	if (op != NULL && action&ACTION_OPERATION)
		rabin_do_operation (op);

	free (arch);
	r_bin_free (bin);

	return 0;
}
