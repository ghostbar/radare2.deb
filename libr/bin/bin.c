/* radare - LGPL - Copyright 2009-2012 - pancake, nibble */

// TODO: dlopen library and show address

#include <r_types.h>
#include <r_util.h>
#include <r_lib.h>
#include <r_list.h>
#include <r_bin.h>
#include <list.h>
#include "../config.h"

static RBinPlugin *bin_static_plugins[] = { R_BIN_STATIC_PLUGINS };
static RBinXtrPlugin *bin_xtr_static_plugins[] = { R_BIN_XTR_STATIC_PLUGINS };

static void get_strings_range(RBinArch *arch, RList *list, int min, ut64 from, ut64 to, ut64 scnrva) {
	char str[R_BIN_SIZEOF_STRINGS];
	int i, matches = 0, ctr = 0;
	RBinString *ptr = NULL;

	if (arch && arch->buf && to > arch->buf->length)
		to = arch->buf->length;
	if (to > 0xffffff) {
		eprintf ("WARNING: bin_strings buffer is too big\n");
		return;
	}
	if (arch->buf && arch->buf->buf)
	for (i = from; i < to; i++) { 
		if ((IS_PRINTABLE (arch->buf->buf[i])) && \
				matches < R_BIN_SIZEOF_STRINGS-1) {
			str[matches] = arch->buf->buf[i];
			/* add support for wide char strings */
			if (arch->buf->buf[i+1]==0) {
				if (IS_PRINTABLE (arch->buf->buf[i+2]))
					if (arch->buf->buf[i+3]==0)
						i++;
			}
			matches++;
			continue;
		}
		/* check if the length fits in our request */
		if (matches >= min) {
			if (!(ptr = R_NEW (RBinString))) {
				eprintf ("Error allocating string\n");
				break;
			}
			str[matches] = '\0';
			ptr->offset = i-matches;
			ptr->rva = ptr->offset-from+scnrva;
			//HACK if (scnrva) ptr->rva = ptr->offset-from+scnrva; else ptr->rva = ptr->offset;
			ptr->size = matches+1;
			ptr->ordinal = ctr;
			// copying so many bytes here..
			memcpy (ptr->string, str, R_BIN_SIZEOF_STRINGS);
			ptr->string[R_BIN_SIZEOF_STRINGS-1] = '\0';
			//r_name_filter (ptr->string, R_BIN_SIZEOF_STRINGS-1);
			r_list_append (list, ptr);
			ctr++;
		}
		matches = 0;
	}
}

static int is_data_section(RBinArch *a, RBinSection *s) {
	RBinObject *o = a->o;
	if (strstr (o->info->bclass, "MACH0") && strstr (s->name, "_cstring")) // OSX
		return 1;
	if (strstr (o->info->bclass, "ELF") && strstr (s->name, "data")) // LINUX
		return 1;
#define X 1
#define ROW (4|2)
	if (strstr (o->info->bclass, "PE") && s->srwx & ROW && !(s->srwx&X) )
		return 1;
	return 0;
}

static RList* get_strings(RBinArch *a, int min) {
	RBinSection *section;
	RListIter *iter;
	RList *ret;
	int count = 0;

	if (!(ret = r_list_new ())) {
		eprintf ("Error allocating array\n");
		return NULL;
	}
	ret->free = free;
	
	if (a->o->sections) {
		r_list_foreach (a->o->sections, iter, section) {
			if (is_data_section (a, section)) {
				count ++;
				get_strings_range (a, ret, min,
					section->offset, section->offset+section->size, section->rva);
			}
		}	
	}
	if (r_list_empty (a->o->sections))
		get_strings_range (a, ret, min, 0, a->size, 0);
	return ret;
}

static int r_bin_init_items(RBin *bin, int dummy) {
	int i;
	RListIter *it;
	RBinPlugin *plugin, *cp;
	RBinArch *a = &bin->cur;
	RBinObject *o = a->o;

	a->curplugin = NULL;
	r_list_foreach (bin->plugins, it, plugin) {
		if ((dummy && !strncmp (plugin->name, "any", 5)) ||
			(!dummy && (plugin->check && plugin->check (&bin->cur)))) {
			bin->cur.curplugin = plugin;
			break;
		}
	}
	cp = bin->cur.curplugin;
	if (!cp || !cp->load || !cp->load (a))
		return R_FALSE;
	if (cp->baddr) o->baddr = cp->baddr (a);
	// XXX: no way to get info from xtr pluginz?
	if (cp->size) o->size = cp->size (a);
	if (cp->binsym)
		for (i=0; i<R_BIN_SYM_LAST; i++)
			o->binsym[i] = cp->binsym (a, i);
	if (cp->entries) o->entries = cp->entries (a);
	if (cp->fields) o->fields = cp->fields (a);
	if (cp->imports) o->imports = cp->imports (a);
	o->info = cp->info? cp->info (a): NULL;
	if (cp->libs) o->libs = cp->libs (a);
	if (cp->relocs) o->relocs = cp->relocs (a);
	if (cp->sections) o->sections = cp->sections (a);
	if (cp->strings) o->strings = cp->strings (a);
	else o->strings = get_strings (a, 4);
	if (cp->symbols) o->symbols = cp->symbols (a);
	if (cp->classes) o->classes = cp->classes (a);
	if (cp->lines) o->lines = cp->lines (a);
	return R_TRUE;
}

#define RBINLISTFREE(x) if(x){r_list_free(x);x=NULL;}
static void r_bin_free_items(RBin *bin) {
	int i;
	RBinArch *a = &bin->cur;
	RBinObject *o = a->o;
	RBINLISTFREE (o->entries);
	RBINLISTFREE (o->fields);
	RBINLISTFREE (o->imports);
	RBINLISTFREE (o->libs);
	RBINLISTFREE (o->relocs);
	RBINLISTFREE (o->sections);
	RBINLISTFREE (o->strings);
	RBINLISTFREE (o->symbols);
	RBINLISTFREE (o->classes);
	free (o->info);
	o->info = NULL;
	if (o->binsym)
		for (i=0; i<R_BIN_SYM_LAST; i++)
			free (o->binsym[i]);
	if (a->curplugin && a->curplugin->destroy)
		a->curplugin->destroy (a);
}

static void r_bin_init(RBin *bin) {
	RListIter *it;
	RBinXtrPlugin *xtr;

	if (bin->cur.o) {
		if (!bin->cur.o->referenced)
			r_bin_free_items (bin);
		free (bin->cur.file);
	}
	memset (&bin->cur, 0, sizeof (bin->cur));
	bin->cur.o = R_NEW0 (RBinObject);
	memset (bin->cur.o, 0, sizeof (RBinObject));
	bin->curxtr = NULL;
	r_list_foreach (bin->binxtrs, it, xtr) {
		if (xtr->check && xtr->check (bin)) {
			bin->curxtr = xtr;
			break;
		}
	}
	if (bin->curxtr && bin->curxtr->load)
		bin->curxtr->load (bin);
}

static int r_bin_extract(RBin *bin, int idx) {
	if (bin->curxtr && bin->curxtr->extract)
		return bin->curxtr->extract (bin, idx);
	if (!bin->file)
		return R_FALSE;
	bin->cur.file = strdup (bin->file);
	bin->cur.buf = r_buf_mmap (bin->file, 0);
	return R_TRUE;
}

R_API int r_bin_add(RBin *bin, RBinPlugin *foo) {
	RListIter *it;
	RBinPlugin *plugin;

	if (foo->init)
		foo->init (bin->user);

	r_list_foreach(bin->plugins, it, plugin) {
		if (!strcmp (plugin->name, foo->name))
			return R_FALSE;
	}
	r_list_append(bin->plugins, foo);

	return R_TRUE;
}

R_API int r_bin_xtr_add(RBin *bin, RBinXtrPlugin *foo) {
	RListIter *it;
	RBinXtrPlugin *xtr;

	if (foo->init)
		foo->init (bin->user);

	// avoid duplicates
	r_list_foreach (bin->binxtrs, it, xtr) {
		if (!strcmp (xtr->name, foo->name))
			return R_FALSE;
	}
	r_list_append (bin->binxtrs, foo);

	return R_TRUE;
}

R_API void* r_bin_free(RBin *bin) {
	if (!bin) return NULL;
	r_bin_free_items (bin);
	if (bin->curxtr && bin->curxtr->destroy)
		bin->curxtr->destroy (bin);
	r_list_free (bin->binxtrs);
	r_list_free (bin->plugins);
	free (bin->file);
	free (bin);
	return NULL;
}

R_API int r_bin_list(RBin *bin) {
	RListIter *it;
	RBinXtrPlugin *plugin;
	RBinXtrPlugin *xtr;

	r_list_foreach (bin->plugins, it, plugin) {
		printf ("bin %-10s %s\n", plugin->name, plugin->desc);
	}

	r_list_foreach (bin->binxtrs, it, xtr) {
		printf ("bin-xtr %-10s %s\n", xtr->name, xtr->desc);
	}

	return R_FALSE;
}

R_API int r_bin_load(RBin *bin, const char *file, int dummy) {
	if (!bin || !file)
		return R_FALSE;
	bin->file = r_file_abspath (file);
	r_bin_init (bin);
	bin->narch = r_bin_extract (bin, 0);
	if (bin->narch == 0)
		return R_FALSE;
	/* FIXME: temporary hack to fix malloc:// */
	if (bin->cur.buf == NULL)
		return R_FALSE;
	return r_bin_init_items (bin, dummy);
}

R_API ut64 r_bin_get_baddr(RBin *bin) {
	return bin->cur.o->baddr;
}

R_API RBinAddr* r_bin_get_sym(RBin *bin, int sym) {
	if (sym<0 || sym>=R_BIN_SYM_LAST)
		return NULL;
	return bin->cur.o->binsym[sym];
}

// XXX: those accessors are redundant
R_API RList* r_bin_get_entries(RBin *bin) {
	return bin->cur.o->entries;
}

R_API RList* r_bin_get_fields(RBin *bin) {
	return bin->cur.o->fields;
}

R_API RList* r_bin_get_imports(RBin *bin) {
	return bin->cur.o->imports;
}

R_API RBinInfo* r_bin_get_info(RBin *bin) {
	return bin->cur.o->info;
}

R_API RList* r_bin_get_libs(RBin *bin) {
	return bin->cur.o->libs;
}

R_API RList* r_bin_get_relocs(RBin *bin) {
	return bin->cur.o->relocs;
}

R_API RList* r_bin_get_sections(RBin *bin) {
	return bin->cur.o->sections;
}

R_API RBinSection* r_bin_get_section_at(RBin *bin, ut64 off, int va) {
	RBinObject *o = bin->cur.o;
	RBinSection *section;
	RListIter *iter;
	ut64 from, to;

	if (o->sections)
	r_list_foreach (o->sections, iter, section) {
		from = va ? o->baddr+section->rva : section->offset;
		to = va ? o->baddr+section->rva+section->vsize :
				  section->offset + section->size;
		if (off >= from && off < to)
			return section;
	}
	return NULL;
}

R_API RList* r_bin_get_strings(RBin *bin) {
	return bin->cur.o->strings;
}

R_API RList* r_bin_get_symbols(RBin *bin) {
	return bin->cur.o->symbols;
}

R_API int r_bin_is_big_endian (RBin *bin) {
	return bin->cur.o->info->big_endian;
}

R_API int r_bin_is_stripped (RBin *bin) {
	return R_BIN_DBG_STRIPPED (bin->cur.o->info->dbg_info);
}

R_API int r_bin_is_static (RBin *bin) {
	return R_BIN_DBG_STATIC (bin->cur.o->info->dbg_info);
}

// TODO: Integrate with r_bin_dbg */
R_API int r_bin_has_dbg_linenums (RBin *bin) {
	return R_BIN_DBG_LINENUMS (bin->cur.o->info->dbg_info);
}

R_API int r_bin_has_dbg_syms (RBin *bin) {
	return R_BIN_DBG_SYMS (bin->cur.o->info->dbg_info);
}

R_API int r_bin_has_dbg_relocs (RBin *bin) {
	return R_BIN_DBG_RELOCS (bin->cur.o->info->dbg_info);
}

R_API RBin* r_bin_new() {
	int i;
	RBinPlugin *static_plugin;
	RBinXtrPlugin *static_xtr_plugin;
	RBin *bin = R_NEW0 (RBin);
	if (!bin) return NULL;
	bin->plugins = r_list_new();
	bin->plugins->free = free;
	bin->cur.o = R_NEW0 (RBinObject);
	for (i=0; bin_static_plugins[i]; i++) {
		static_plugin = R_NEW (RBinPlugin);
		memcpy (static_plugin, bin_static_plugins[i],
			sizeof (RBinPlugin));
		r_bin_add (bin, static_plugin);
	}
	bin->binxtrs = r_list_new ();
	bin->binxtrs->free = free;
	for (i=0; bin_xtr_static_plugins[i]; i++) {
		static_xtr_plugin = R_NEW (RBinXtrPlugin);
		memcpy (static_xtr_plugin, bin_xtr_static_plugins[i],
			sizeof (RBinXtrPlugin));
		r_bin_xtr_add (bin, static_xtr_plugin);
	}
	return bin;
}

/* arch and bits are implicit in the plugin name, do we really need
 * to overwrite bin->cur.info? */
R_API int r_bin_use_arch(RBin *bin, const char *arch, int bits, const char *name) {
	RBinObject *o = bin->cur.o;
	RListIter *it;
	RBinPlugin *plugin;

	if (!o->info) o->info = R_NEW0 (RBinInfo);
	strncpy (o->info->arch, arch, R_BIN_SIZEOF_STRINGS);
	o->info->bits = bits;

	r_list_foreach (bin->plugins, it, plugin) {
		if (!strcmp (name, plugin->name)) {
			bin->cur.curplugin = plugin;
			return R_TRUE;
		}
	}

	return R_FALSE;
}

// DUPDUPDUP
R_API int r_bin_select(RBin *bin, const char *arch, int bits, const char *name) {
	int i;
	RBinInfo *info;
	for (i=0; i<bin->narch; i++) {
		r_bin_select_idx (bin, i);
		info = bin->cur.o->info;
		if (!info || !bin->cur.file ||
			(arch && !strstr (info->arch, arch)) ||
			(bits && bits != info->bits) ||
			(name && !strstr (info->file, name)))
			continue;
		return R_TRUE;
	}
	return R_FALSE;
}

R_API int r_bin_select_idx(RBin *bin, int idx) {
	r_bin_free_items (bin);
	if (r_bin_extract (bin, idx))
		return r_bin_init_items (bin, R_FALSE);
	return R_FALSE;
}

R_API void r_bin_list_archs(RBin *bin) {
	int i;
	for (i = 0; i < bin->narch; i++)
		if (r_bin_select_idx (bin, i)) {
			RBinInfo *info = bin->cur.o->info;
			printf ("%03i 0x%08"PFMT64x" %s_%i %s\n", i, 
				bin->cur.offset, info->arch,
				info->bits, info->machine);
		}
}

R_API void r_bin_set_user_ptr(RBin *bin, void *user) {
	bin->user = user;
}

static int getoffset (RBin *bin, int type, int idx) {
	RBinArch *a = &bin->cur;
	if (a && a->curplugin && a->curplugin->get_offset)
		return a->curplugin->get_offset (a, type, idx);
	return -1;
}

R_API void r_bin_bind (RBin *bin, RBinBind *b) {
	b->bin = bin;
	b->get_offset = getoffset;
}

R_API RBuffer *r_bin_create (RBin *bin, const ut8 *code, int codelen, const ut8 *data, int datalen) {
	RBinArch *a = &bin->cur;
	if (codelen<0) codelen = 0;
	if (datalen<0) datalen = 0;
	if (a && a->curplugin && a->curplugin->create)
		return a->curplugin->create (bin, code, codelen, data, datalen);
	return NULL;
}

R_API RBinObject *r_bin_get_object(RBin *bin, int flags) {
	bin->cur.o->referenced = R_TRUE;
	return bin->cur.o;
}

R_API void r_bin_object_free(RBinObject *obj) {
	free (obj);
}

R_API RList* /*<RBinClass>*/r_bin_get_classes(RBin *bin) {
	return bin->cur.o->classes;
}

R_API ut64 r_bin_get_offset (RBin *bin) {
	return bin->cur.offset;
}

R_API ut64 r_bin_get_size (RBin *bin) {
	return bin->cur.o->size;
}
