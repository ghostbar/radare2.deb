/* radare - Apache 2.0 - Copyright 2007-2013 - pancake
   class.h rewrite: Adam Pridgen <dso@rice.edu || adam.pridgen@thecoverofnight.com>
 */

#ifndef _INCLUDE_JAVA_CLASS_H_
#define _INCLUDE_JAVA_CLASS_H_

#include <r_types.h>
#include <r_list.h>
#include <r_bin.h>
#include <sdb.h>

#define USHORT(x,y) ((ut16)(x[y+1]|(x[y]<<8)))
#define UINT(x,y) ((ut32)((x[y]<<24)|(x[y+1]<<16)|(x[y+2]<<8)|x[y+3]))

#define R_BIN_JAVA_MAXSTR 256

#define R_BIN_JAVA_USHORT(x,y) ((ut16)(((0xff&x[y+1])|((x[y]&0xff)<<8)) & 0xffff))

#define R_BIN_JAVA_UINT(x,y) ((ut32)(((x[y]&0xff)<<24)|((x[y+1]&0xff)<<16)|((x[y+2]&0xff)<<8)|(x[y+3]&0xff)))
#define R_BIN_JAVA_FLOAT(x,y) ((float)R_BIN_JAVA_UINT(x,y))

#define RBIN_JAVA_LONG(x,y) ( ((ut64) R_BIN_JAVA_UINT (x, y) << 32) | ((ut64)R_BIN_JAVA_UINT (x, y+4) & 0xffffffff))
//#define R_BIN_JAVA_DOUBLE(x,y) ((double)RBIN_JAVA_LONG(x,y))
//#define R_BIN_JAVA_SWAPUSHORT(x) ((ut16)((x<<8)|((x>>8)&0x00FF)))

// Intentionally not R_API
void copy_type_info_to_stack_frame_list(RList *type_list, RList *sf_list);
void copy_type_info_to_stack_frame_list_up_to_idx(RList *type_list, RList *sf_list, ut64 idx);

R_API double rbin_java_raw_to_double(ut8* raw, ut64 offset);
R_API ut64 rbin_java_raw_to_long(ut8* raw, ut64 offset);


#define R_BIN_JAVA_DOUBLE(x,y) rbin_java_raw_to_double(x, y)

typedef enum {
	R_BIN_JAVA_METHOD_ACC_PUBLIC= 0x0001,
	R_BIN_JAVA_METHOD_ACC_PRIVATE = 0x0002,
	R_BIN_JAVA_METHOD_ACC_PROTECTED = 0x0004,
	R_BIN_JAVA_METHOD_ACC_STATIC = 0x0008,

	R_BIN_JAVA_METHOD_ACC_FINAL = 0x0010,
	R_BIN_JAVA_METHOD_ACC_SYNCHRONIZED = 0x0020,
	R_BIN_JAVA_METHOD_ACC_BRIDGE = 0x0040,
	R_BIN_JAVA_METHOD_ACC_VARARGS = 0x0080,

	R_BIN_JAVA_METHOD_ACC_NATIVE = 0x0100,
	R_BIN_JAVA_METHOD_ACC_INTERFACE = 0x0200,
	R_BIN_JAVA_METHOD_ACC_ABSTRACT = 0x0400,
	R_BIN_JAVA_METHOD_ACC_STRICT= 0x0800,

	R_BIN_JAVA_METHOD_ACC_SYNTHETIC = 0x1000,
	R_BIN_JAVA_METHOD_ACC_ANNOTATION = 0x2000,
	R_BIN_JAVA_METHOD_ACC_ENUM = 0x4000
} R_BIN_JAVA_METHOD_ACCESS;

typedef enum {
	R_BIN_JAVA_FIELD_ACC_PUBLIC= 0x0001,
	R_BIN_JAVA_FIELD_ACC_PRIVATE = 0x0002,
	R_BIN_JAVA_FIELD_ACC_PROTECTED = 0x0004,
	R_BIN_JAVA_FIELD_ACC_STATIC = 0x0008,

	R_BIN_JAVA_FIELD_ACC_FINAL = 0x0010,
	R_BIN_JAVA_FIELD_ACC_VOLATILE = 0x0040,
	R_BIN_JAVA_FIELD_ACC_TRANSIENT = 0x0080,

	R_BIN_JAVA_FIELD_ACC_SYNTHETIC = 0x1000,
	R_BIN_JAVA_FIELD_ACC_ENUM = 0x4000
} R_BIN_JAVA_FIELD_ACCESS;

typedef enum {
	R_BIN_JAVA_CLASS_ACC_PUBLIC= 0x0001,
	R_BIN_JAVA_CLASS_ACC_PRIVATE = 0x0002,
	R_BIN_JAVA_CLASS_ACC_PROTECTED = 0x0004,
	R_BIN_JAVA_CLASS_ACC_STATIC = 0x0008,

	R_BIN_JAVA_CLASS_ACC_FINAL = 0x0010,
	R_BIN_JAVA_CLASS_ACC_SUPER = 0x0020,
	R_BIN_JAVA_CLASS_ACC_BRIDGE = 0x0040,
	R_BIN_JAVA_CLASS_ACC_VARARGS = 0x0080,

	R_BIN_JAVA_CLASS_ACC_NATIVE = 0x0100,
	R_BIN_JAVA_CLASS_ACC_INTERFACE = 0x0200,
	R_BIN_JAVA_CLASS_ACC_ABSTRACT = 0x0400,
	R_BIN_JAVA_CLASS_ACC_STRICT= 0x0800,

	R_BIN_JAVA_CLASS_ACC_SYNTHETIC = 0x1000,
	R_BIN_JAVA_CLASS_ACC_ANNOTATION = 0x2000,
	R_BIN_JAVA_CLASS_ACC_ENUM = 0x4000
} R_BIN_JAVA_CLASS_ACCESS;

typedef struct {
	char *str;
	ut16 value;
	ut8 len;
} RBinJavaAccessFlags;

typedef enum {
	R_BIN_JAVA_REF_UNKNOWN = 0,
	R_BIN_JAVA_REF_GETFIELD = 1,
	R_BIN_JAVA_REF_GETSTATIC = 2,
	R_BIN_JAVA_REF_PUTFIELD = 3,
	R_BIN_JAVA_REF_PUTSTATIC = 4,
	R_BIN_JAVA_REF_INVOKEVIRTUAL = 5,
	R_BIN_JAVA_REF_INVOKESTATIC = 6,
	R_BIN_JAVA_REF_INVOKESPECIAL = 7,
	R_BIN_JAVA_REF_NEWINVOKESPECIAL = 8,
	R_BIN_JAVA_REF_INVOKEINTERFACE = 9
} R_BIN_JAVA_REF_TYPE;


typedef enum {
	R_BIN_JAVA_CP_NULL = 0,
	R_BIN_JAVA_CP_UTF8 = 1,
	R_BIN_JAVA_CP_UNKNOWN = 2,
	R_BIN_JAVA_CP_INTEGER = 3,
	R_BIN_JAVA_CP_FLOAT = 4,
	R_BIN_JAVA_CP_LONG = 5,
	R_BIN_JAVA_CP_DOUBLE = 6,
	R_BIN_JAVA_CP_CLASS = 7,
	R_BIN_JAVA_CP_STRING = 8,
	R_BIN_JAVA_CP_FIELDREF = 9,
	R_BIN_JAVA_CP_METHODREF = 10,
	R_BIN_JAVA_CP_INTERFACEMETHOD_REF = 11,
	R_BIN_JAVA_CP_NAMEANDTYPE = 12,
	R_BIN_JAVA_CP_NOTHIN_13 = 13,
	R_BIN_JAVA_CP_NOTHIN_14 = 14,
	R_BIN_JAVA_CP_METHODHANDLE = 15,
	R_BIN_JAVA_CP_METHODTYPE = 16,
	R_BIN_JAVA_CP_NOTHIN_17 = 17,
	R_BIN_JAVA_CP_INVOKEDYNAMIC = 18,
} R_BIN_JAVA_CP_TYPE;

typedef enum {
	R_BIN_JAVA_STACK_FRAME_IMPLICIT = 0,
	R_BIN_JAVA_STACK_FRAME_SAME,
	R_BIN_JAVA_STACK_FRAME_SAME_LOCALS_1,
	R_BIN_JAVA_STACK_FRAME_CHOP,
	R_BIN_JAVA_STACK_FRAME_SAME_FRAME_EXTENDED,
	R_BIN_JAVA_STACK_FRAME_APPEND,
	R_BIN_JAVA_STACK_FRAME_FULL_FRAME,
	R_BIN_JAVA_STACK_FRAME_RESERVED
} R_BIN_JAVA_STACK_FRAME_TYPE;


typedef enum {
	R_BIN_JAVA_ATTR_TYPE_ANNOTATION_DEFAULT_ATTR = 0,
	R_BIN_JAVA_ATTR_TYPE_BOOTSTRAP_METHODS_ATTR,
	R_BIN_JAVA_ATTR_TYPE_CODE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_CONST_VALUE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_DEPRECATED_ATTR,
	R_BIN_JAVA_ATTR_TYPE_ENCLOSING_METHOD_ATTR,
	R_BIN_JAVA_ATTR_TYPE_EXCEPTIONS_ATTR,
	R_BIN_JAVA_ATTR_TYPE_INNER_CLASSES_ATTR,
	R_BIN_JAVA_ATTR_TYPE_LINE_NUMBER_TABLE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_LOCAL_VARIABLE_TABLE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_LOCAL_VARIABLE_TYPE_TABLE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_RUNTIME_INVISIBLE_ANNOTATION_ATTR,
	R_BIN_JAVA_ATTR_TYPE_RUNTIME_INVISIBLE_PARAMETER_ANNOTATION_ATTR,
	R_BIN_JAVA_ATTR_TYPE_RUNTIME_VISIBLE_ANNOTATION_ATTR,
	R_BIN_JAVA_ATTR_TYPE_RUNTIME_VISIBLE_PARAMETER_ANNOTATION_ATTR,
	R_BIN_JAVA_ATTR_TYPE_SIGNATURE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_SOURCE_DEBUG_EXTENTSION_ATTR,
	R_BIN_JAVA_ATTR_TYPE_SOURCE_FILE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_STACK_MAP_TABLE_ATTR,
	R_BIN_JAVA_ATTR_TYPE_SYNTHETIC_ATTR,
	R_BIN_JAVA_ATTR_TYPE_UNKNOWN_ATTR,
	R_BIN_JAVA_ATTR_TYPE_FAILED_ATTR
} R_BIN_JAVA_ATTR_TYPE;

typedef enum {
	R_BIN_JAVA_STACKMAP_TOP = 0,
	R_BIN_JAVA_STACKMAP_INTEGER,
	R_BIN_JAVA_STACKMAP_FLOAT,
	R_BIN_JAVA_STACKMAP_DOUBLE,
	R_BIN_JAVA_STACKMAP_LONG,
	R_BIN_JAVA_STACKMAP_NULL,
	R_BIN_JAVA_STACKMAP_THIS,
	R_BIN_JAVA_STACKMAP_OBJECT,
	R_BIN_JAVA_STACKMAP_UNINIT,
	R_BIN_JAVA_STACKMAP_UNKNOWN
} R_BIN_JAVA_STACKMAP_TYPE;

typedef enum {
	R_BIN_JAVA_FIELD_TYPE_FIELD = 0,
	R_BIN_JAVA_FIELD_TYPE_METHOD,
	R_BIN_JAVA_FIELD_TYPE_INTERFACE
} R_BIN_JAVA_FIELD_TYPE;

typedef struct r_bin_java_meta{
	ut64  file_offset;
	void *type_info;
	ut32 ord;
}  RBinJavaMetaInfo;

/* Class Reference Informations */

//struct r_bin_R_BIN_JAVA_CP_ant_t;

typedef struct  r_bin_java_class_info_t {
	ut16 name_idx;
} RBinJavaCPTypeClass;

typedef struct  r_bin_java_fieldref_info_t {
	ut16 class_idx;
	ut16 name_and_type_idx;
} RBinJavaCPTypeFieldRef;

typedef struct  r_bin_java_methodref_info_t {
	ut16 class_idx;
	ut16 name_and_type_idx;
} RBinJavaCPTypeMethodRef;


typedef struct  r_bin_java_interfacemethodref_info_t {
	ut16 class_idx;
	ut16 name_and_type_idx;
} RBinJavaCPTypeInterfaceMethodRef;

typedef struct  r_bin_java_methodhandle_info_t {
	ut8 reference_kind;
	ut16 reference_index;
} RBinJavaCPTypeMethodHandle;

typedef struct  r_bin_java_methodtype_info_t {
	ut16 descriptor_index;
} RBinJavaCPTypeMethodType;

typedef struct  r_bin_java_invokedynamic_info_t {
	ut16 bootstrap_method_attr_index;
	ut16 name_and_type_index;
} RBinJavaCPTypeInvokeDynamic;

/* Primitive Type Informations */

typedef struct  r_bin_java_string_info_t {
	ut16 string_idx;
} RBinJavaCPTypeString;

typedef struct  r_bin_java_integer_info_t {
	union {
		ut8 raw[4];
		ut32 dword;
	} bytes;

} RBinJavaCPTypeInteger;

typedef struct  r_bin_java_float_info_t {
	union {
		ut8 raw[4];
		ut32 dword;
	} bytes;
} RBinJavaCPTypeFloat;


typedef struct  r_bin_java_long_info_t {
	union {
		ut8 raw[8];
		ut64 qword;
		struct{
			ut32 high;
			ut32 low;
		} dwords;
	} bytes;
} RBinJavaCPTypeLong;

typedef struct  r_bin_java_double_info_t {
	union {
		ut8 raw[8];
		struct{
			ut32 high;
			ut32 low;
		} dwords;
	} bytes;
} RBinJavaCPTypeDouble;


/* Meta-data Info */

typedef struct  r_bin_java_name_and_type_info_t {
	ut16 name_idx;
	ut16 descriptor_idx;
} RBinJavaCPTypeNameAndType;

typedef struct  r_bin_java_utf8_info_t {
	ut16 length;
	ut8 *bytes;
} RBinJavaCPTypeUtf8;

typedef struct  r_bin_java_cp_object_t {
	RBinJavaMetaInfo *metas;
	ut64 file_offset;
	ut8 tag;
	union {
		RBinJavaCPTypeClass cp_class;
		RBinJavaCPTypeMethodRef cp_method;
		RBinJavaCPTypeFieldRef cp_field;
		RBinJavaCPTypeInterfaceMethodRef cp_interface;

		RBinJavaCPTypeString cp_string;
		RBinJavaCPTypeInteger cp_integer;
		RBinJavaCPTypeFloat cp_float;
		RBinJavaCPTypeLong cp_long;
		RBinJavaCPTypeDouble cp_double;
		RBinJavaCPTypeNameAndType cp_name_and_type;
		RBinJavaCPTypeUtf8 cp_utf8;

		RBinJavaCPTypeMethodHandle cp_method_handle;
		RBinJavaCPTypeMethodType cp_method_type;
		RBinJavaCPTypeInvokeDynamic cp_invoke_dynamic;
	} info;
	char* name;
	ut8* value;
	ut64 loadaddr;
} RBinJavaCPTypeObj;

typedef struct r_bin_java_stack_map_frame_t { // attribute StackMap
	ut64 file_offset;
	ut64 size;
	ut32 code_offset;
	ut8 tag;
	ut8 type;

	struct r_bin_java_stack_map_frame_t *p_stack_frame;

	RBinJavaMetaInfo *metas;
	ut32 offset_delta;
	ut32 number_of_locals;
	// list of verification objects;
	RList* local_items;
	ut32 number_of_stack_items;
	// list of verification objects;
	RList* stack_items;

} RBinJavaStackMapFrame;

typedef struct r_bin_java_source_debugging_extension_attr_t {
	ut8* debug_extension;
} RBinJavaSourceDebugExtensionAttr;

typedef struct r_bin_java_enclosing_method_attr_t {
	ut16 class_idx;
	ut16 method_idx;
	char *class_name;
	char *method_name;
	char *method_descriptor;
} RBinJavaEnclosingMethodAttr;

typedef struct r_bin_java_boot_strap_arg_t{
	ut64 file_offset;
	ut64 size;
	ut16 argument_info_idx;
	// additional informations?
	RBinJavaCPTypeObj *argument_info_cp_obj;
} RBinJavaBootStrapArgument;

typedef struct r_bin_java_boot_strap_method_t{
	ut64 file_offset;
	ut64 size;
	ut16 bootstrap_method_ref;
	ut16 num_bootstrap_arguments;
	// List of RBinJavaCodeAttribute
	RList *bootstrap_arguments;
} RBinJavaBootStrapMethod;


typedef struct r_bin_java_boot_strap_methods_t{
	ut16 num_bootstrap_methods;
	RList *bootstrap_methods;
} RBinJavaBootstrapMethodsAttr;


typedef struct {
	ut16 type_name_idx;
	ut16 const_name_idx;
	RBinJavaCPTypeObj *const_name_cp_obj;
	RBinJavaCPTypeObj *type_name_cp_obj;
} RBinJavaEnumConstValue;

typedef struct {
	ut16 const_value_idx;
	RBinJavaCPTypeObj *const_value_cp_obj;
} RBinJavaConstValue;

typedef struct {
	ut16 class_info_idx;
	RBinJavaCPTypeObj *class_info_cp_obj;;
} RBinJavaClassInfoValue;

typedef struct r_bin_java_element_value_ary_t{
	ut64 size;
	ut32 num_values;
	RList *values;
} RBinJavaElementValueArray;



typedef struct r_bin_java_annotation_t{
	ut64 size;
	ut16 type_idx;
	ut16 num_element_value_pairs;
	RList *element_value_pairs;
} RBinJavaAnnotation;

typedef enum {
	// Primitive Types
	R_BIN_JAVA_EV_TAG_BYTE = 'B',
	R_BIN_JAVA_EV_TAG_CHAR = 'C',
	R_BIN_JAVA_EV_TAG_DOUBLE = 'D',
	R_BIN_JAVA_EV_TAG_FLOAT = 'F',
	R_BIN_JAVA_EV_TAG_INT = 'I',
	R_BIN_JAVA_EV_TAG_LONG = 'J',
	R_BIN_JAVA_EV_TAG_SHORT = 'S',
	R_BIN_JAVA_EV_TAG_BOOLEAN = 'Z',
	// Other tags
	R_BIN_JAVA_EV_TAG_ARRAY = '[',
	R_BIN_JAVA_EV_TAG_STRING = 's',
	R_BIN_JAVA_EV_TAG_ENUM = 'e',
	R_BIN_JAVA_EV_TAG_CLASS = 'c',
	R_BIN_JAVA_EV_TAG_ANNOTATION = '@',
	//	R_BIN_JAVA_EV_TAG_CLASSNAME = 'L',
	R_BIN_JAVA_EV_TAG_UNKNOWN = 0xff,
} R_BIN_JAVA_EV_TAG;

typedef struct r_bin_java_element_value_t {
	RBinJavaMetaInfo *metas;
	ut64 size;
	ut8 tag;
	ut64 file_offset;
	union {
		RBinJavaConstValue const_value;
		RBinJavaEnumConstValue enum_const_value;
		RBinJavaClassInfoValue class_value;
		RBinJavaAnnotation annotation_value;
		RBinJavaElementValueArray array_value;
	} value;
} RBinJavaElementValue;

typedef struct r_bin_java_element_value_pair_t{
	ut64 file_offset;
	ut64 size;
	ut16 element_name_idx;
	char* name;
	RBinJavaElementValue *value;
} RBinJavaElementValuePair;



typedef struct r_bin_java_annotations_attr_t {
	ut64 size;
	ut16 num_annotations;
	RList *annotations;
} RBinJavaAnnotationsArray;

typedef RBinJavaAnnotationsArray RBinJavaRuntimeVisibleAnnotationsAttr;
typedef RBinJavaAnnotationsArray RBinJavaRuntimeInvisibleAnnotationsAttr;
typedef RBinJavaAnnotationsArray RBinJavaRuntimeParameterAnnotationsArray;

typedef struct r_bin_java_parameter_annotation_attr_t {
	ut8 num_parameters;
	// RBinJavaRuntimeParameterAnnotationsArray
	RList *parameter_annotations;
} RBinJavaRuntimeVisibleParameterAnnotationsAttr;

typedef RBinJavaRuntimeVisibleParameterAnnotationsAttr RBinJavaRuntimeInvisibleParameterAnnotationsAttr;

typedef struct r_bin_java_parameter_annotations_attr_t {
	RBinJavaElementValue *default_value;
} RBinJavaAnnotationDefaultAttr;

typedef struct r_bin_java_stack_map_table_attr_t { // attribute StackMap
	ut32 code_size;
	ut32 number_of_entries;
	RList* stack_map_frame_entries;
} RBinJavaStackMapTableAttr;


typedef struct r_bin_java_signature_attr_t {
	ut16 signature_idx;
	char *signature;
} RBinJavaSignatureAttr;

typedef struct r_bin_java_stack_verification_t{
	ut64 file_offset;
	ut64 size;
	ut8 tag;
	char *name;
	union {
		ut16 obj_val_cp_idx;
		ut16 uninit_offset;
	} info;
} RBinJavaVerificationObj;

typedef struct r_bin_java_fm_t {
	RBinJavaMetaInfo *metas;
	ut64 size;
	char *name;
	char *descriptor;
	char *class_name;
	char *flags_str;

	ut64 file_offset;
	R_BIN_JAVA_FIELD_TYPE type;
	ut16 flags;
	ut16 name_idx;
	ut16 descriptor_idx;

	RBinJavaCPTypeObj *field_ref_cp_obj;

	ut16 attr_count;
	RList *attributes;
	ut64 method_number;
} RBinJavaField;

typedef struct r_bin_java_interface_info_desc_t{
	char *name;
	ut64 size;
	ut64 file_offset;
	ut16 class_info_idx;
	RBinJavaCPTypeObj *cp_class;
} RBinJavaInterfaceInfo;

typedef struct r_bin_java_attr_constant_t {
	ut16 constantvalue_idx;
} RBinJavaConstantValueAttr;

typedef struct r_bin_java_attr_exceptions_t {
	ut16 number_of_exceptions;
	ut16* exception_idx_table;
} RBinJavaExceptionsAttr;

typedef struct r_bin_java_attr_exception_table_entry_t {
	ut64 file_offset;
	ut16 start_pc;
	ut16 end_pc;
	ut16 handler_pc;
	ut16 catch_type;
	ut64 size;
} RBinJavaExceptionEntry;

typedef struct r_bin_java_attr_code_t {
	ut64 file_offset;
	ut32 code_offset;
	ut16 max_stack;
	ut16 max_locals;
	ut16 code_length;
	ut8 *code;

	ut32 exception_table_length;
	RList *exception_table; // RBinJavaExceptionTableEntry *

	ut16 attributes_count;
	RList *attributes;
	RBinJavaStackMapFrame *implicit_frame;
} RBinJavaCodeAttr;

typedef struct r_bin_java_attr_inner_classes_t {
	ut16 number_of_classes;
	RList* classes;
} RBinJavaInnerClassesAttribute;

typedef struct r_bin_java_attr_source_file_t{
	ut16 sourcefile_idx;
} RBinJavaSourceFileAttribute;

typedef struct r_bin_java_line_number_table_t{
	ut64 file_offset;
	ut16 start_pc;
	ut16 line_number;
	ut64 size;
} RBinJavaLineNumberAttribute;

typedef struct r_bin_java_attr_linenum_t {
	ut64 file_offset;
	ut16 line_number_table_length;
	RList* line_number_table; // RBinJavaLineNumberTable*
} RBinJavaLineNumberTableAttribute;

typedef struct r_bin_java_attr_localvariabletype_t{
	char *name;
	char *signature;

	ut64 file_offset;
	ut16 start_pc;
	ut16 length;
	ut16 name_idx;
	ut16 signature_idx;
	ut16 index;
	ut64 size;
} RBinJavaLocalVariableTypeAttribute;

typedef struct r_bin_java_attr_localvariable_type_table_t {
	ut16 table_length;
	RList* local_variable_table; // RBinJavaLocalVariable
} RBinJavaLocalVariableTypeTableAttribute;

typedef struct r_bin_java_attr_localvariable_t{
	char *name;
	char *descriptor;

	ut64 file_offset;
	ut16 start_pc;
	ut16 length;
	ut16 name_idx;
	ut16 descriptor_idx;
	ut16 index;
	ut64 size;
} RBinJavaLocalVariableAttribute;


typedef struct r_bin_java_attr_localvariable_table_t {
	ut16 table_length;
	RList* local_variable_table; // RBinJavaLocalVariable
} RBinJavaLocalVariableTableAttribute;

typedef struct r_bin_java_attr_t {
	ut8 *bytes;
	ut64 pos;
	ut64 size;
	char *name;
	ut64 file_offset;
	RBinJavaMetaInfo *metas;
	int type;
	ut16 name_idx; //	ut16 attribute_name_idx;
	ut32 length;   //ut16 attribute_length;
	ut64 loadaddr;
	union {
		RBinJavaAnnotationDefaultAttr annotation_default_attr;
		RBinJavaBootstrapMethodsAttr bootstrap_methods_attr;
		RBinJavaCodeAttr code_attr;
		RBinJavaConstantValueAttr constant_value_attr;
		RBinJavaEnclosingMethodAttr enclosing_method_attr;
		RBinJavaExceptionsAttr exceptions_attr;
		RBinJavaLineNumberTableAttribute line_number_table_attr;
		RBinJavaLocalVariableTableAttribute local_variable_table_attr;
		RBinJavaLocalVariableTypeTableAttribute local_variable_type_table_attr;
		RBinJavaInnerClassesAttribute inner_classes_attr;

		RBinJavaAnnotationsArray annotation_array;
		RBinJavaRuntimeVisibleAnnotationsAttr rtv_annotations_attr;
		RBinJavaRuntimeInvisibleAnnotationsAttr rti_annotations_attr;

		RBinJavaRuntimeVisibleParameterAnnotationsAttr rtvp_annotations_attr;
		RBinJavaRuntimeInvisibleParameterAnnotationsAttr rtip_annotations_attr;
		RBinJavaSourceDebugExtensionAttr debug_extensions;
		RBinJavaSourceFileAttribute source_file_attr;
		RBinJavaStackMapTableAttr stack_map_table_attr;
		RBinJavaSignatureAttr signature_attr;

	} info;

} RBinJavaAttrInfo;

typedef struct r_bin_java_attr_classes_t {
	char *name;
	char *flags_str;
	ut64 file_offset;
	RBinJavaAttrInfo *clint_attr;
	RBinJavaField *clint_field;
	ut16 inner_class_info_idx;
	ut16 outer_class_info_idx;
	ut16 inner_name_idx;
	ut16 inner_class_access_flags;
	ut64 size;
} RBinJavaClassesAttribute;

typedef struct r_bin_java_classfile_t {
	ut8 cafebabe[4];
	ut8 minor[2];
	ut8 major[2];
	ut16 cp_count;
} RBinJavaClass;

typedef struct r_bin_java_classfile2_t {
	ut16 access_flags;
	char *flags_str;
	ut16 this_class;
	ut16 super_class;
	RBinJavaField *this_class_entrypoint;
	RBinJavaAttrInfo *this_class_entrypoint_code_attr;
} RBinJavaClass2;

typedef struct r_bin_java_lines_t {
	int count;
	int *addr;
	int *line;
} RBinJavaLines;

typedef struct r_bin_java_obj_t {
	struct r_bin_java_classfile_t cf;
	RBinJavaClass2* cf2;

	ut32 cp_offset, cp_size, cp_count;
	ut32 fields_offset, fields_size, fields_count;
	ut32 interfaces_offset, interfaces_size, interfaces_count;
	ut32 methods_offset, methods_size, methods_count;
	ut32 classes_offset, classes_size, classes_count;
	ut32 attributes_offset, attributes_size, attributes_count;

	ut64 loadaddr; // load address that is used to calc actual offset
				// when multiple bins are loaded at once
	int size;
	char* file;
	RBinJavaLines lines;
	struct r_buf_t* b;

	// These state variables are used for parsing the appropriate number of bytes
	// when readin uoffset, ustack, ulocalvar values
	ut8 ulocalvar_sz;
	ut8 ustack_sz;
	ut8 offset_sz;
	ut32 cur_method_code_length;
	RBinJavaAttrInfo *current_code_attr;

	ut32 attr_idx;
	ut32 method_idx;
	ut32 field_idx;
	ut32 cp_idx;
	ut32 interface_idx;
	ut32 attributes_idx;

	//ut32 classes_idx; //TODO: when classes list is being used, update this value

	int fsym;
	int fsymsz;

	RBinJavaField *main;
	RBinJavaAttrInfo *main_code_attr;

	RBinJavaField *entrypoint;
	RBinJavaAttrInfo *entrypoint_code_attr;

	//RList* classes_list; // TODO: Not sure if this is necessary.  it would be the inner classes info.
	RList* fields_list;
	RList* methods_list;
	RList* cp_list;
	RList* interfaces_list;
	RList* attributes;

	RList* functions;
	RList* disassembly;

	Sdb *kv;
	Sdb *AllJavaBinObjs;
	ut32 id;
} RBinJavaObj;

R_API RList * r_bin_java_get_interface_names(RBinJavaObj * bin);
R_API RList * r_bin_java_get_lib_names(RBinJavaObj * bin);
R_API RList* r_bin_java_get_sections(RBinJavaObj *bin);
R_API RList* r_bin_java_get_fields(RBinJavaObj *bin);
R_API char* r_bin_java_get_version(RBinJavaObj* bin);
R_API RBinAddr * r_bin_java_get_entrypoint(RBinJavaObj* bin, int sym);
R_API RList* r_bin_java_get_entrypoints(RBinJavaObj* bin);
R_API ut64 r_bin_java_get_main(RBinJavaObj* bin);
R_API RList* r_bin_java_get_symbols(RBinJavaObj* bin);
R_API RList* r_bin_java_get_strings(RBinJavaObj* bin);
R_API void* r_bin_java_free(RBinJavaObj* bin);
R_API RBinJavaObj* r_bin_java_new(const char* file, ut64 baddr, Sdb * kv);
R_API RBinJavaObj* r_bin_java_new_buf(struct r_buf_t* buf, ut64 baddr, Sdb * kv);

// Stuff used to manage Java Class File Constant Information
typedef struct r_bin_java_object_allocs_t {
	RBinJavaCPTypeObj *(*new_obj) (RBinJavaObj *bin, ut8* buffer, ut64 offset) ;
	void (*delete_obj) (RBinJavaCPTypeObj *obj);
	void (*print_summary) (RBinJavaCPTypeObj *obj);
	ut64 (*calc_size) (RBinJavaCPTypeObj *obj);
	char* (*stringify_obj) (RBinJavaCPTypeObj *obj);
} RBinJavaCPTypeObjectAllocs;

typedef struct r_bin_java_attr_allocs_t {
	//void (*new_obj) (RBinJavaObj *bin, RBinJavaAttrInfo *obj, ut64 offset) ;
	RBinJavaAttrInfo* (*new_obj)(ut8* buffer, ut64 sz, ut64 buf_offset);
	void (*delete_obj) (RBinJavaAttrInfo *obj);
	void (*print_summary) (RBinJavaAttrInfo *obj);
	ut64 (*calc_size)(RBinJavaAttrInfo *obj);
} RBinJavaAttrInfoObjectAllocs;

typedef struct r_bin_java_ver_allocs_t {
	void (*new_obj) (RBinJavaObj *bin, ut32 code_length, ut64 offset) ;
	void (*delete_obj) (RBinJavaAttrInfo *obj);
	void (*print_summary) (RBinJavaAttrInfo *obj);
} RBinJavaVerInfoObjectAllocs;

typedef struct r_bin_java_stack_frame_allocs_t {
	RBinJavaStackMapFrame* (*new_obj) (RBinJavaObj *bin, ut64 offset) ;
	void (*delete_obj) (RBinJavaStackMapFrame *obj);
	void (*print_summary) (RBinJavaStackMapFrame *obj);
} RBinJavaStackMapFrameObjectAllocs;

typedef struct {
	RBinJavaElementValue* (*new_obj) (RBinJavaObj *bin, ut64 offset) ;
	void (*delete_obj) (RBinJavaElementValue *obj);
	void (*print_summary) (RBinJavaElementValue *obj);
} RBinJavaElementValueObjectAllocs;

typedef struct r_bin_R_BIN_JAVA_CP_ant_meta_t {
	char *name;
	ut8 tag;
	ut32 len;
	RBinJavaCPTypeObjectAllocs *allocs;
} RBinJavaCPTypeMetas;

typedef struct r_bin_java_attr_meta_t {
	char *name;
	ut8 type;
	RBinJavaAttrInfoObjectAllocs *allocs;
} RBinJavaAttrMetas;

typedef struct r_bin_java_ver_meta_t {
	char *name;
	ut8 type;
} RBinJavaVerificationMetas;

typedef struct r_bin_java_stack_frame_meta_t {
	char *name;
	ut8 type;
	RBinJavaStackMapFrameObjectAllocs *allocs;
} RBinJavaStackMapFrameMetas;

typedef struct{
	char* name;
	ut8 tag;
	RBinJavaElementValueObjectAllocs* allocs;
} RBinJavaElementValueMetas;

typedef struct{
	char* name;
	ut8 tag;
} RBinJavaRefMetas;


R_API ut8* r_bin_java_get_attr_buf(RBinJavaObj *bin, ut64 offset, ut64 sz);
R_API char* r_bin_java_get_name_from_cp_item_list(RList *cp_list, ut64 idx);
R_API char* r_bin_java_get_utf8_from_cp_item_list(RList *cp_list, ut64 idx);
R_API ut32 r_bin_java_get_utf8_len_from_cp_item_list(RList *cp_list, ut64 idx);
R_API char* r_bin_java_get_desc_from_cp_item_list(RList *cp_list, ut64 idx);
R_API char* r_bin_java_get_item_name_from_cp_item_list(RList *cp_list, RBinJavaCPTypeObj *obj);
R_API char* r_bin_java_get_item_desc_from_cp_item_list(RList *cp_list, RBinJavaCPTypeObj *obj);
R_API RBinJavaCPTypeObj* r_bin_java_get_item_from_cp_item_list(RList *cp_list, ut64 idx);


R_API char* r_bin_java_get_name_from_bin_cp_list(RBinJavaObj *bin, ut64 idx);
R_API char* r_bin_java_get_utf8_from_bin_cp_list(RBinJavaObj *bin, ut64 idx);
R_API ut32 r_bin_java_get_utf8_len_from_bin_cp_list(RBinJavaObj *bin, ut64 idx);
R_API char* r_bin_java_get_desc_from_bin_cp_list(RBinJavaObj *bin, ut64 idx);
R_API char* r_bin_java_get_item_name_from_bin_cp_list(RBinJavaObj *bin, RBinJavaCPTypeObj *obj);
R_API char* r_bin_java_get_item_desc_from_bin_cp_list(RBinJavaObj *bin, RBinJavaCPTypeObj *obj);
R_API RBinJavaCPTypeObj* r_bin_java_get_item_from_bin_cp_list(RBinJavaObj *bin, ut64 idx);

// Allocs for objects
R_API RBinJavaCPTypeObj* r_bin_java_class_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_fieldref_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_methodref_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_interfacemethodref_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_name_and_type_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);

R_API RBinJavaCPTypeObj* r_bin_java_string_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_integer_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_float_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_long_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_double_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_utf8_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_do_nothing_new(RBinJavaObj *bin, ut8* buffer, ut64 sz);
R_API RBinJavaCPTypeObj* r_bin_java_clone_cp_item(RBinJavaCPTypeObj *obj);
R_API RBinJavaCPTypeObj* r_bin_java_clone_cp_idx(RBinJavaObj *bin, ut32 idx);

R_API RBinJavaCPTypeObj* r_bin_java_methodhandle_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 sz);
R_API RBinJavaCPTypeObj* r_bin_java_methodtype_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 sz);
R_API RBinJavaCPTypeObj* r_bin_java_invokedynamic_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 sz);

// Deallocs for type objects
R_API void r_bin_java_default_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_obj_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_class_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_fieldref_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_methodref_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_interfacemethodref_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_integer_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_string_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_float_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_long_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_double_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_utf8_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_name_and_type_info_free(RBinJavaCPTypeObj *obj);
R_API void r_bin_java_do_nothing_free(RBinJavaCPTypeObj *obj);

// handle freeing the lists

// handle the reading of the various field
R_API RBinJavaAttrInfo* r_bin_java_read_next_attr(RBinJavaObj *bin, ut64 offset);
R_API RBinJavaCPTypeObj* r_bin_java_read_next_constant_pool_item(RBinJavaObj *bin, ut64 offset);
R_API RBinJavaAttrInfo* r_bin_java_get_method_code_attribute(const RBinJavaField *method);
R_API RBinJavaAttrMetas* r_bin_java_get_attr_type_by_name(const char *name);
R_API RBinJavaCPTypeObj* r_bin_java_get_java_null_cp();
R_API RBinJavaClass2* r_bin_java_read_class_file2(RBinJavaObj *bin, ut64 offset);
R_API RBinJavaAttrInfo* r_bin_java_get_attr_from_field(RBinJavaField *field, R_BIN_JAVA_ATTR_TYPE attr_type, ut32 pos );

// free bin lists
R_API void r_bin_java_free_attribute_list(RList *attributes);
R_API void r_bin_java_constant_pool_list_free(RBinJavaObj* bin);
R_API void r_bin_java_methods_list_free(RBinJavaObj* bin);
R_API void r_bin_java_interfaces_list_free(RBinJavaObj* bin);
R_API void r_bin_java_fields_list_free(RBinJavaObj* bin);

R_API void r_bin_java_print_field_summary(RBinJavaField *field);
R_API void r_bin_java_print_interface_summary(RBinJavaField *field);
R_API void r_bin_java_print_method_summary(RBinJavaField *field);

R_API void r_bin_java_print_code_exceptions_attr_summary(RBinJavaExceptionEntry *exc_entry);
R_API void r_bin_java_print_code_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_constant_value_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_deprecated_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_exceptions_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_classes_attr_summary(RBinJavaClassesAttribute *icattr);
R_API void r_bin_java_print_inner_classes_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_line_number_table_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_local_variable_attr_summary(RBinJavaLocalVariableAttribute *lvattr);
R_API void r_bin_java_print_local_variable_table_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_source_code_file_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_synthetic_attr_summary(RBinJavaAttrInfo *attr);

R_API void r_bin_java_print_attr_summary(RBinJavaAttrInfo *attr);

R_API char * r_bin_java_print_utf8_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_name_and_type_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_double_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_long_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_float_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_integer_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_string_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_classref_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_fieldref_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_methodref_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_interfacemethodref_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_unknown_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_null_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_methodtype_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_invokedynamic_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_methodhandle_cp_stringify(RBinJavaCPTypeObj* obj);
R_API char * r_bin_java_print_class_cp_stringify(RBinJavaCPTypeObj* obj);

R_API void r_bin_java_print_utf8_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_name_and_type_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_double_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_long_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_float_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_integer_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_string_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_classref_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_fieldref_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_methodref_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_interfacemethodref_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_unknown_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_null_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_unknown_attr_summary(RBinJavaAttrInfo *attr);

R_API void r_bin_java_print_methodhandle_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_methodtype_cp_summary(RBinJavaCPTypeObj* obj);
R_API void r_bin_java_print_invokedynamic_cp_summary(RBinJavaCPTypeObj* obj);

R_API RBinJavaCPTypeObj* r_bin_java_unknown_cp_new(RBinJavaObj *bin, ut8* buffer, ut64 sz);

R_API RBinJavaInterfaceInfo* r_bin_java_interface_new(RBinJavaObj *bin, ut8 *buf, ut64 sz);
R_API RBinJavaInterfaceInfo* r_bin_java_read_next_interface_item(RBinJavaObj *bin, ut64 offset);

R_API void r_bin_java_interface_free(RBinJavaInterfaceInfo *obj);

R_API void r_bin_java_stack_frame_free(RBinJavaStackMapFrame* obj);
R_API void r_bin_java_stack_map_table_attr_free(RBinJavaAttrInfo* attr);
R_API void r_bin_java_verification_info_free(RBinJavaVerificationObj* obj);

R_API void r_bin_java_print_stack_map_table_attr_summary(RBinJavaAttrInfo *obj);
R_API void r_bin_java_print_stack_map_frame_summary(RBinJavaStackMapFrame *obj);
R_API void r_bin_java_print_verification_info_summary(RBinJavaVerificationObj *obj);

R_API RBinJavaStackMapFrame* r_bin_java_build_stack_frame_from_local_variable_table(RBinJavaObj *bin, RBinJavaAttrInfo *attr);
R_API RBinJavaStackMapFrame* r_bin_java_stack_frame_new (ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaVerificationObj* r_bin_java_read_next_verification_info_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaAnnotation* r_bin_java_annotation_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaElementValuePair* r_bin_java_element_pair_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaElementValue* r_bin_java_element_value_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaBootStrapArgument* r_bin_java_bootstrap_method_argument_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaBootStrapMethod* r_bin_java_bootstrap_method_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaAnnotationsArray* r_bin_java_annotation_array_new(ut8* buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaElementValueMetas* r_bin_java_get_ev_meta_from_tag(ut8 tag);
R_API RBinJavaCPTypeMetas* r_bin_java_get_cp_meta_from_tag(ut8 tag);

R_API void r_bin_java_annotation_default_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_enclosing_methods_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_local_variable_type_table_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_signature_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_source_debug_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_element_value_free(RBinJavaElementValue* element_value);
R_API void r_bin_java_element_pair_free(RBinJavaElementValuePair *ev_pair);
R_API void r_bin_java_annotation_free(RBinJavaAnnotation *annotation);
R_API void r_bin_java_rtv_annotations_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_rti_annotations_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_annotation_array_free(RBinJavaAnnotationsArray *annotation_array);
R_API void r_bin_java_bootstrap_methods_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_bootstrap_method_free(RBinJavaBootStrapMethod *bsm);
R_API void r_bin_java_bootstrap_method_argument_free(RBinJavaBootStrapArgument *bsm_arg);
R_API void r_bin_java_rtvp_annotations_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_rtip_annotations_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_unknown_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_code_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_constant_value_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_deprecated_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_exceptions_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_inner_classes_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_line_number_table_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_local_variable_table_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_source_code_file_attr_free(RBinJavaAttrInfo *attr);
R_API void r_bin_java_synthetic_attr_free(RBinJavaAttrInfo *attr);


R_API void r_bin_java_print_annotation_default_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_enclosing_methods_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_local_variable_type_attr_summary(RBinJavaLocalVariableTypeAttribute *lvattr);
R_API void r_bin_java_print_local_variable_type_table_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_signature_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_source_debug_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_element_value_summary(RBinJavaElementValue *element_value);
R_API void r_bin_java_print_annotation_summary(RBinJavaAnnotation *annotation);
R_API void r_bin_java_print_element_pair_summary(RBinJavaElementValuePair *ev_pair);
R_API void r_bin_java_print_bootstrap_methods_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_bootstrap_method_summary(RBinJavaBootStrapMethod *bsm);
R_API void r_bin_java_bootstrap_method_argument_summary(RBinJavaBootStrapArgument *bsm_arg);
R_API void r_bin_java_print_rtv_annotations_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_rti_annotations_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_annotation_array_summary(RBinJavaAnnotationsArray *annotation_array);
R_API void r_bin_java_print_rtvp_annotations_attr_summary(RBinJavaAttrInfo *attr);
R_API void r_bin_java_print_rtip_annotations_attr_summary(RBinJavaAttrInfo *attr);

R_API RBinJavaAttrInfo* r_bin_java_read_next_attr_from_buffer(ut8 *buffer, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_unknown_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_annotation_default_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_enclosing_methods_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_local_variable_type_table_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_signature_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_source_debug_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_bootstrap_methods_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_rtv_annotations_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_rti_annotations_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_rtvp_annotations_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_rtip_annotations_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_code_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_constant_value_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_deprecated_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_exceptions_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_inner_classes_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_line_number_table_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_local_variable_table_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_source_code_file_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_stack_map_table_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);
R_API RBinJavaAttrInfo* r_bin_java_synthetic_attr_new(ut8 *buf, ut64 sz, ut64 buf_offset);

R_API ut64 r_bin_java_unknown_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_annotation_default_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_enclosing_methods_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_local_variable_type_table_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_signature_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_source_debug_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_bootstrap_methods_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_rtv_annotations_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_rti_annotations_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_rtvp_annotations_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_rtip_annotations_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_code_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_constant_value_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_deprecated_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_exceptions_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_inner_classes_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_line_number_table_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_local_variable_table_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_source_code_file_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_stack_map_table_attr_calc_size(RBinJavaAttrInfo *attr);
R_API ut64 r_bin_java_synthetic_attr_calc_size(RBinJavaAttrInfo *attr);

R_API ut64 r_bin_java_bootstrap_method_calc_size(RBinJavaBootStrapMethod *bsm);
R_API ut64 r_bin_java_element_pair_calc_size(RBinJavaElementValuePair *ev_pair);
R_API ut64 r_bin_java_element_value_calc_size(RBinJavaElementValue *element_value);


R_API ut64 r_bin_java_unknown_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_class_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_fieldref_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_methodref_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_interfacemethodref_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_name_and_type_cp_calc_size(RBinJavaCPTypeObj* obj);

R_API ut64 r_bin_java_string_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_integer_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_float_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_long_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_double_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_utf8_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_do_nothing_calc_size(RBinJavaCPTypeObj* obj);

R_API ut64 r_bin_java_methodhandle_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_methodtype_cp_calc_size(RBinJavaCPTypeObj* obj);
R_API ut64 r_bin_java_invokedynamic_cp_calc_size(RBinJavaCPTypeObj* obj);

R_API RBinJavaStackMapFrame* r_bin_java_default_stack_frame();

R_API RBinSymbol* r_bin_java_allocate_symbol();
R_API RBinSymbol* r_bin_java_create_new_symbol_from_field_with_access_flags(RBinJavaField *fm_type);
R_API RBinSymbol* r_bin_java_create_new_symbol_from_cp_idx(ut32 cp_idx, ut64 baddr);
R_API RBinSymbol* r_bin_java_create_new_symbol_from_invoke_dynamic(RBinJavaCPTypeObj *obj, ut64 baddr);
R_API RBinSymbol* r_bin_java_create_new_symbol_from_ref(RBinJavaCPTypeObj *obj, ut64 baddr);
R_API RBinSymbol* r_bin_java_create_new_symbol_from_method(RBinJavaField *fm_type);

R_API ut64 r_bin_java_get_method_code_offset(RBinJavaField *fm_type);
R_API ut64 r_bin_java_get_method_code_size(RBinJavaField *fm_type);

R_API RBinJavaCPTypeObj *r_bin_java_find_cp_ref_info(ut16 name_and_typeidx);
R_API RBinJavaCPTypeObj *r_bin_java_find_cp_ref_info_from_name_and_type(ut16 name_idx, ut16 descriptor_idx);
R_API RBinJavaCPTypeObj *r_bin_java_find_cp_name_and_type_info(ut16 name_idx, ut16 descriptor_idx);

R_API RBinClass* r_bin_java_allocate_r_bin_class();
R_API RList *r_bin_java_get_classes(RBinJavaObj *bin);
R_API RList *r_bin_java_enum_class_methods(RBinJavaObj *bin, ut16 class_idx);
R_API RList *r_bin_java_enum_class_fields(RBinJavaObj *bin, ut16 class_idx);
R_API ut64 r_bin_java_find_method_offset(RBinJavaObj *bin, const char* method_name);

R_API RBinJavaField * r_bin_java_get_method_code_attribute_with_addr(RBinJavaObj *bin, ut64 addr);

R_API const RList* r_bin_java_get_methods_list(RBinJavaObj* bin);
R_API const RBinJavaObj* r_bin_java_get_bin_obj(const char *name);
R_API int r_bin_java_update_file (const char *key, RBinJavaObj *bin_obj);
R_API RBinJavaObj * r_bin_java_get_sdb_bin_obj(const char * filename);
R_API RList * r_bin_java_get_bin_obj_list();
R_API RList* r_bin_java_get_bin_obj_list_thru_obj(RBinJavaObj *bin_obj);
R_API char * r_bin_java_get_this_class_name(RBinJavaObj *bin_obj);
R_API char * r_bin_java_build_obj_key (RBinJavaObj *bin);

R_API RList * r_bin_java_extract_type_values( char *arg_str);
R_API int r_bin_java_extract_reference_name(const char * input_str, char ** ref_str, ut8 array_cnt);
R_API RList * r_bin_java_extract_all_bin_type_values( RBinJavaObj * bin_obj);

R_API RList * r_bin_java_get_method_definitions(RBinJavaObj *bin);
R_API char * r_bin_java_get_method_definition(RBinJavaField *fm_type);
R_API RList * r_bin_java_get_field_definitions(RBinJavaObj *bin);
R_API char * r_bin_java_get_field_definition(RBinJavaField *fm_type);
R_API RList * r_bin_java_get_import_definitions(RBinJavaObj *bin);
R_API RList * r_bin_java_get_field_offsets(RBinJavaObj *bin);
R_API RList * r_bin_java_get_method_offsets(RBinJavaObj *bin);

R_API ut16 r_bin_java_calculate_method_access_value(const char * access_flags_str);
R_API ut16 r_bin_java_calculate_field_access_value(const char * access_flags_str);
R_API ut16 r_bin_java_calculate_class_access_value(const char * access_flags_str);
R_API char * r_bin_java_resolve(RBinJavaObj *obj, int idx, ut8 space_bn_name_type);
R_API char * r_bin_java_resolve_with_space(RBinJavaObj *obj, int idx);
R_API char * r_bin_java_resolve_without_space(RBinJavaObj *BIN_OBJ, int idx);
R_API char * r_bin_java_resolve_cp_idx_type(RBinJavaObj *BIN_OBJ, int idx);
R_API char * r_bin_java_resolve_b64_encode(RBinJavaObj *BIN_OBJ, ut16 idx);

#endif
