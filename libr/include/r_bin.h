/* radare - LGPL - Copyright 2008-2013 - nibble, pancake */

#ifndef _INCLUDE_R_BIN_H_
#define _INCLUDE_R_BIN_H_

#include <r_util.h>
#include <r_types.h>
#include <r_list.h>
#include <r_bin_dwarf.h>

#ifdef __cplusplus
extern "C" {
#endif

R_LIB_VERSION_HEADER (r_bin);

#define R_BIN_SCN_EXECUTABLE(x) x & 0x1
#define R_BIN_SCN_WRITABLE(x)   x & 0x2
#define R_BIN_SCN_READABLE(x)   x & 0x4
#define R_BIN_SCN_SHAREABLE(x)  x & 0x8

#define R_BIN_DBG_STRIPPED(x) x & 0x01
#define R_BIN_DBG_STATIC(x)   x & 0x02
#define R_BIN_DBG_LINENUMS(x) x & 0x04
#define R_BIN_DBG_SYMS(x)     x & 0x08
#define R_BIN_DBG_RELOCS(x)   x & 0x10

#define R_BIN_SIZEOF_STRINGS 512
#define R_BIN_MAX_ARCH 1024

enum {
	R_BIN_SYM_ENTRY,
	R_BIN_SYM_INIT,
	R_BIN_SYM_MAIN,
	R_BIN_SYM_FINI,
	R_BIN_SYM_LAST
};

// name mangling types
enum {
	R_BIN_NM_NONE = 0,
	R_BIN_NM_JAVA = 1,
	R_BIN_NM_CXX = 2,
	R_BIN_NM_OBJC= 3,
	R_BIN_NM_ANY = -1,
};

enum {
	R_BIN_CLASS_PRIVATE,
	R_BIN_CLASS_PUBLIC,
	R_BIN_CLASS_FRIENDLY,
	R_BIN_CLASS_PROTECTED,
};

enum {
	R_BIN_RELOC_8,
	R_BIN_RELOC_16,
	R_BIN_RELOC_32,
	R_BIN_RELOC_64
};

typedef struct r_bin_addr_t {
	ut64 rva;
	ut64 offset;
} RBinAddr;

typedef struct r_bin_hash_t {
	const char *type;
	ut64 addr;
	int len;
	ut64 from;
	ut64 to;
	ut8 buf[32];
	const char *cmd;
} RBinHash;

typedef struct r_bin_info_t {
	char file[R_BIN_SIZEOF_STRINGS];
	char type[R_BIN_SIZEOF_STRINGS];
	char bclass[R_BIN_SIZEOF_STRINGS];
	char rclass[R_BIN_SIZEOF_STRINGS];
	char arch[R_BIN_SIZEOF_STRINGS];
	char machine[R_BIN_SIZEOF_STRINGS];
	char os[R_BIN_SIZEOF_STRINGS];
	char subsystem[R_BIN_SIZEOF_STRINGS];
	char rpath[R_BIN_SIZEOF_STRINGS];
	const char *lang;
	int bits;
	int has_va;
	int has_pi; // pic/pie
	int big_endian;
	ut64 dbg_info;
	RBinHash sum[3];
} RBinInfo;

typedef struct r_bin_object_t {
	ut64 baddr;
	int size;
	RList/*<RBinSection>*/ *sections;
	RList/*<RBinImport>*/ *imports;
	RList/*<RBinSymbol>*/ *symbols;
	RList/*<??>*/ *entries;
	RList/*<??>*/ *fields;
	RList/*<??>*/ *libs;
	RList/*<??>*/ *relocs;
	RList/*<??>*/ *strings;
	RList/*<RBinClass>*/ *classes;
	RList/*<RBinDwarfRow>*/ *lines;
	RBinInfo *info;
	RBinAddr *binsym[R_BIN_SYM_LAST];
	int referenced;
	int lang;
} RBinObject;

// XXX: this is a copy of RBinObject
// TODO: rename RBinArch to RBinFile
typedef struct r_bin_arch_t {
	RBuffer *buf;
	char *file;
	int size;
	int rawstr;
	ut64 offset;
	RBinObject *o;
	void *bin_obj; // internal pointer used by formats
	struct r_bin_plugin_t *curplugin;
} RBinArch;

typedef struct r_bin_t {
	char *file;
	RBinArch cur;
	int narch;
	void *user;
	void *bin_obj;
	int minstrlen;
	struct r_bin_xtr_plugin_t *curxtr;
	RList *plugins;
	RList *binxtrs;
} RBin;

typedef struct r_bin_xtr_plugin_t {
	char *name;
	char *desc;
	int (*init)(void *user);
	int (*fini)(void *user);
	int (*check)(RBin *bin);
	int (*extract)(RBin *bin, int idx);
	int (*load)(RBin *bin);
	int (*size)(RBin *bin);
	int (*destroy)(RBin *bin);
} RBinXtrPlugin;

typedef struct r_bin_plugin_t {
	char *name;
	char *desc;
	int (*init)(void *user);
	int (*fini)(void *user);
	int (*load)(RBinArch *arch);
	int (*size)(RBinArch *bin);
	int (*destroy)(RBinArch *arch);
	int (*check)(RBinArch *arch);
	ut64 (*baddr)(RBinArch *arch);
	RBinAddr* (*binsym)(RBinArch *arch, int num);
	RList* (*entries)(RBinArch *arch);
	RList* (*sections)(RBinArch *arch);
	RList* (*lines)(RBinArch *arch);
	RList* (*symbols)(RBinArch *arch);
	RList* (*imports)(RBinArch *arch);
	RList* (*strings)(RBinArch *arch);
	RBinInfo* (*info)(RBinArch *arch);
	RList* (*fields)(RBinArch *arch);
	RList* (*libs)(RBinArch *arch);
	RList* (*relocs)(RBinArch *arch);
	RList* (*classes)(RBinArch *arch);
	int (*demangle_type)(const char *str);
	struct r_bin_meta_t *meta;
	struct r_bin_write_t *write;
	int (*get_offset)(RBinArch *arch, int type, int idx);
	RBuffer* (*create)(RBin *bin, const ut8 *code, int codelen, const ut8 *data, int datalen);
	int minstrlen;
} RBinPlugin;

typedef struct r_bin_section_t {
	char name[R_BIN_SIZEOF_STRINGS];
	ut64 size;
	ut64 vsize;
	ut64 rva;
	ut64 offset;
	ut64 srwx;
	// per section platform info
	const char *arch;
	int bits;
} RBinSection;

typedef struct r_bin_class_t {
	char *name;
	char *super;
	int index;
	RList *methods; // <RBinSymbol>
	RList *fields; // <RBinField>
	int visibility;
} RBinClass;

#define RBinSectionName r_offsetof(RBinSection, name)
#define RBinSectionOffset r_offsetof(RBinSection, offset)
// usage:
// r_list_get_by_name(bin->sections, RBinSectionName, ".text");
// bin.sections.get_by_name(SectionName, ".text");

typedef struct r_bin_symbol_t {
	char name[R_BIN_SIZEOF_STRINGS];
	char forwarder[R_BIN_SIZEOF_STRINGS];
	char bind[R_BIN_SIZEOF_STRINGS];
	char type[R_BIN_SIZEOF_STRINGS];
	const char *classname;
	ut64 rva;
	ut64 offset;
	ut64 size;
	ut64 ordinal;
	ut32 visibility;
} RBinSymbol;

typedef struct r_bin_import_t {
	char name[R_BIN_SIZEOF_STRINGS];
	char bind[R_BIN_SIZEOF_STRINGS];
	char type[R_BIN_SIZEOF_STRINGS];
	ut64 ordinal;
	ut32 visibility;
} RBinImport;

typedef struct r_bin_reloc_t {
	ut8 type;
	ut8 additive;
	RBinImport *import;
	st64 addend;
	ut64 rva;
	ut64 offset;
	ut32 visibility;
} RBinReloc;

typedef struct r_bin_string_t {
	// TODO: rename string->name (avoid colisions)
	char string[R_BIN_SIZEOF_STRINGS];
	ut64 rva;
	ut64 offset;
	ut64 ordinal;
	ut64 size;
} RBinString;

typedef struct r_bin_field_t {
	char name[R_BIN_SIZEOF_STRINGS];
	ut64 rva;
	ut64 offset;
	ut32 visibility;
} RBinField;

typedef struct r_bin_meta_t {
	int (*get_line)(RBinArch *arch, ut64 addr, char *file, int len, int *line);
} RBinMeta;

typedef struct r_bin_write_t {
	ut64 (*scn_resize)(RBinArch *arch, const char *name, ut64 size);
	int (*rpath_del)(RBinArch *arch);
} RBinWrite;

// TODO: deprecate r_bin_is_big_endian
// TODO: has_dbg_syms... maybe flags?

typedef int (*RBinGetOffset)(RBin *bin, int type, int idx);
typedef const char *(*RBinGetName)(RBin *bin, int off);

typedef struct r_bin_bind_t {
	RBin *bin;
	RBinGetOffset get_offset;
	RBinGetName get_name;
	ut32 visibility;
} RBinBind;

#ifdef R_API

#define r_bin_class_free(x) { free(x->name);free(x->super);free (x); }

/* bin.c */
R_API void r_bin_bind(RBin *b, RBinBind *bnd);
R_API int r_bin_add(RBin *bin, RBinPlugin *foo);
R_API int r_bin_xtr_add(RBin *bin, RBinXtrPlugin *foo);
R_API void* r_bin_free(RBin *bin);
R_API int r_bin_list(RBin *bin);
R_API int r_bin_load(RBin *bin, const char *file, int dummy);
R_API RBinObject *r_bin_get_object(RBin *bin);
R_API ut64 r_bin_get_baddr(RBin *bin);
R_API RBinAddr* r_bin_get_sym(RBin *bin, int sym);

R_API char* r_bin_demangle(RBin *bin, const char *str);
R_API int r_bin_demangle_type (const char *str);
R_API char *r_bin_demangle_java(const char *str);
R_API char *r_bin_demangle_cxx(const char *str);
R_API char *r_bin_demangle_objc(RBin *bin, const char *sym);
R_API int r_bin_lang_objc(RBin *a);
R_API int r_bin_lang_cxx(RBin *a);

R_API RList* r_bin_get_entries(RBin *bin);
R_API RList* r_bin_get_fields(RBin *bin);
R_API RList* r_bin_get_imports(RBin *bin);
R_API RBinInfo* r_bin_get_info(RBin *bin);
R_API RList* r_bin_get_libs(RBin *bin);
R_API ut64 r_bin_get_size (RBin *bin);
R_API RList* r_bin_get_relocs(RBin *bin);
R_API RList* r_bin_get_sections(RBin *bin);
R_API RList* /*<RBinClass>*/r_bin_get_classes(RBin *bin);

R_API RBinClass *r_bin_class_get (RBin *bin, const char *name);
R_API RBinClass *r_bin_class_new (RBin *bin, const char *name, const char *super, int view);
R_API int r_bin_class_add_method (RBin *bin, const char *classname, const char *name, int nargs);
R_API void r_bin_class_add_field (RBin *bin, const char *classname, const char *name);

R_API RBinSection* r_bin_get_section_at(RBin *bin, ut64 off, int va);
R_API RList* r_bin_get_strings(RBin *bin);
R_API RList* r_bin_get_symbols(RBin *bin);
R_API int r_bin_is_big_endian (RBin *bin);
R_API int r_bin_is_stripped (RBin *bin);
R_API int r_bin_is_static (RBin *bin);
R_API int r_bin_has_dbg_linenums (RBin *bin);
R_API int r_bin_has_dbg_syms (RBin *bin);
R_API int r_bin_has_dbg_relocs (RBin *bin);
R_API RBin* r_bin_new();
R_API int r_bin_use_arch(RBin *bin, const char *arch, int bits, const char *name);
R_API int r_bin_select(RBin *bin, const char *arch, int bits, const char *name);
R_API int r_bin_select_idx(RBin *bin, int idx);
R_API void r_bin_list_archs(RBin *bin);
R_API void r_bin_set_user_ptr(RBin *bin, void *user);
R_API RBuffer *r_bin_create (RBin *bin, const ut8 *code, int codelen, const ut8 *data, int datalen);
/* bin_meta.c */
R_API int r_bin_meta_get_line(RBin *bin, ut64 addr, char *file, int len, int *line);
R_API char *r_bin_meta_get_source_line(RBin *bin, ut64 addr);
R_API ut64 r_bin_get_offset (RBin *bin);
/* bin_write.c */
R_API ut64 r_bin_wr_scn_resize(RBin *bin, const char *name, ut64 size);
R_API int r_bin_wr_rpath_del(RBin *bin);
R_API int r_bin_wr_output(RBin *bin, const char *filename);
R_API int r_bin_dwarf_parse_info(RBin *a);
R_API RList *r_bin_dwarf_parse_line(RBin *a);

/* plugin pointers */
extern RBinPlugin r_bin_plugin_any;
extern RBinPlugin r_bin_plugin_fs;
extern RBinPlugin r_bin_plugin_elf;
extern RBinPlugin r_bin_plugin_elf64;
extern RBinPlugin r_bin_plugin_p9;
extern RBinPlugin r_bin_plugin_pe;
extern RBinPlugin r_bin_plugin_mz;
extern RBinPlugin r_bin_plugin_pe64;
extern RBinPlugin r_bin_plugin_bios;
extern RBinPlugin r_bin_plugin_te;
extern RBinPlugin r_bin_plugin_mach0;
extern RBinPlugin r_bin_plugin_mach064;
extern RBinPlugin r_bin_plugin_java;
extern RBinPlugin r_bin_plugin_dex;
extern RBinPlugin r_bin_plugin_dummy;
extern RBinPlugin r_bin_plugin_rar;
extern RBinXtrPlugin r_bin_xtr_plugin_zip;
extern RBinXtrPlugin r_bin_xtr_plugin_fatmach0;
extern RBinXtrPlugin r_bin_xtr_plugin_dyldcache;

#ifdef __cplusplus
}
#endif

#endif
#endif
