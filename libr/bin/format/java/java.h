#include <r_types.h>

#define R_BIN_JAVA_MAXSTR 256

#define R_BIN_JAVA_USHORT(x,y) (unsigned short)((x[y+1]|(x[y]<<8)) & 0xffff)
#define R_BIN_JAVA_UINT(x,y) (unsigned int)((x[y]<<24)|(x[y+1]<<16)|(x[y+2]<<8)|x[y+3])
#define R_BIN_JAVA_SWAPUSHORT(x) (unsigned short)((x<<8)|((x>>8)&0x00FF))

enum {
	R_BIN_JAVA_TYPE_FIELD,
	R_BIN_JAVA_TYPE_CODE,
	R_BIN_JAVA_TYPE_LINENUM,
	R_BIN_JAVA_TYPE_CONST
};

struct r_bin_java_classfile_t {
	ut8 cafebabe[4];
	ut8 minor[2];
	ut8 major[2];
	unsigned short cp_count;
};

struct r_bin_java_classfile2_t {
	unsigned short access_flags;
	unsigned short this_class;
	unsigned short super_class;
};

struct r_bin_java_cp_item_t {
	int tag;
	char name[32];
	char *value;
	ut8 bytes[5];
	unsigned short length;
	unsigned short ord;
	unsigned short off;
};

struct r_bin_java_attr_code_t {
	unsigned short max_stack;
	unsigned short max_locals;
	unsigned short code_length;
	unsigned short code_offset;
	unsigned int exception_table_length;
	unsigned short start_pc;
	unsigned short end_pc;
	unsigned short handler_pc;
	unsigned short catch_type;
};

struct r_bin_java_attr_linenum_t {
	unsigned int table_length;
	unsigned short start_pc;
	unsigned short line_number;
};

struct r_bin_java_attr_t {
	int type;
	char *name;
	unsigned short name_idx;
	unsigned int length;
	union {
		struct r_bin_java_attr_code_t code;
		struct r_bin_java_attr_linenum_t linenum;
		unsigned short const_value_idx;
	} info;
	struct r_bin_java_attr_t *attributes;
};

struct r_bin_java_fm_t {
	unsigned short flags;
	char *name;
	unsigned short name_idx;
	char *descriptor;
	unsigned short descriptor_idx;
	unsigned short attr_count;
	struct r_bin_java_attr_t *attributes;
};

struct r_bin_java_obj_t {
	struct r_bin_java_classfile_t cf;
	struct r_bin_java_classfile2_t cf2;
	struct r_bin_java_cp_item_t *cp_items;
	unsigned int fields_count;
	struct r_bin_java_fm_t *fields;
	unsigned int methods_count;
	struct r_bin_java_fm_t *methods;
	int size;
    const char* file;
	struct r_buf_t* b;
};

struct r_bin_java_sym_t {
	char name[R_BIN_JAVA_MAXSTR];
	ut64 offset;
	ut64 size;
	int last;
};

struct r_bin_java_str_t {
	char str[R_BIN_JAVA_MAXSTR];
	ut64 offset;
	ut64 ordinal;
	ut64 size;
	int last;
};

char* r_bin_java_get_version(struct r_bin_java_obj_t* bin);
ut64 r_bin_java_get_entrypoint(struct r_bin_java_obj_t* bin);
struct r_bin_java_sym_t* r_bin_java_get_symbols(struct r_bin_java_obj_t* bin);
struct r_bin_java_str_t* r_bin_java_get_strings(struct r_bin_java_obj_t* bin);
void* r_bin_java_free(struct r_bin_java_obj_t* bin);
struct r_bin_java_obj_t* r_bin_java_new(const char* file);
