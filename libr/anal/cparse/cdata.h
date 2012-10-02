struct Token {
	int dval;
	char* sval;
};

typedef struct Token Token;

//#define R_ANAL_TYPE_CHAR		0
//#define R_ANAL_TYPE_SHORT		1
//#define R_ANAL_TYPE_INT			2
//#define R_ANAL_TYPE_LONG		3
//#define R_ANAL_TYPE_LONGLONG	4
//#define R_ANAL_TYPE_FLOAT		5
//#define R_ANAL_TYPE_DOUBLE		6
//#define R_ANAL_TYPE_VOID		7
#define R_ANAL_TYPE_SIGNED		8
#define R_ANAL_TYPE_UNSIGNED	9

#define R_ANAL_UINT8_T			1
#define R_ANAL_UINT16_T			2
#define R_ANAL_UINT32_T			3
#define R_ANAL_UINT64_T			4

#define NONE_SIGN				11
#define NONE_QUALIFIER			12

#define R_ANAL_VAR_STATIC		0
#define R_ANAL_VAR_CONST		1
#define R_ANAL_VAR_REGISTER		2
#define R_ANAL_VAR_VOLATILE		3

RAnalType* new_variable_node(char* name, short type, short sign, short modifier, RAnalAttr *valattr);
RAnalType* new_pointer_node(char* name, short type, short sign, short modifier, RAnalAttr *valattr);
RAnalType* new_array_node(char* name, short type, short sign, short modifier, long size, RAnalAttr *valattr);
RAnalType* new_struct_node(char* name, RAnalType *defs, RAnalAttr *valattr);
RAnalType* new_union_node(char* name, RAnalType *defs, RAnalAttr *valattr);
RAnalType* new_alloca_node(long address, long size, RAnalType *defs);
RAnalLocals* new_locals_node(RAnalType *defs, RAnalAttr *valattr);
RAnalAttr* new_attribute(char* name, char* value);
RAnalType* new_function_node(char* name, short ret_type, RAnalType *args, short fmodifier, short callconvention, char* attributes, RList *locals, RAnalAttr *valattr);

