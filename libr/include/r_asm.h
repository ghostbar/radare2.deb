/* radare - LGPL - Copyright 2009-2010 nibble<.ds@gmail.com> */

#ifndef _INCLUDE_R_ASM_H_
#define _INCLUDE_R_ASM_H_

#include <r_types.h>
#include <list.h>
#include <r_util.h>

#define R_ASM_BUFSIZE 1024
#define R_ASM_FASTCALL_ARGS 6

enum {
	R_ASM_ARCH_NONE = 0,
	R_ASM_ARCH_X86,
	R_ASM_ARCH_ARM,
	R_ASM_ARCH_PPC,
	R_ASM_ARCH_M68K,
	R_ASM_ARCH_JAVA,
	R_ASM_ARCH_MIPS,
	R_ASM_ARCH_SPARC,
	R_ASM_ARCH_CSR,
	R_ASM_ARCH_MSIL,
	R_ASM_ARCH_OBJD,
	R_ASM_ARCH_BF
};

enum {
	R_ASM_SYNTAX_NONE = 0,
	R_ASM_SYNTAX_INTEL,
	R_ASM_SYNTAX_ATT
};

enum {
	R_ASM_MOD_RAWVALUE = 'r',
	R_ASM_MOD_VALUE = 'v',
	R_ASM_MOD_DSTREG = 'd',
	R_ASM_MOD_SRCREG0 = '0',
	R_ASM_MOD_SRCREG1 = '1',
	R_ASM_MOD_SRCREG2 = '2'
};

typedef struct r_asm_fastcall_t {
	const char *arg[16];
} RAsmFastcall;

typedef struct r_asm_aop_t {
	int  inst_len;
	ut8  buf[R_ASM_BUFSIZE];
	char buf_asm[R_ASM_BUFSIZE];
	char buf_hex[R_ASM_BUFSIZE];
	char buf_err[R_ASM_BUFSIZE];
} RAsmAop;

typedef struct r_asm_code_t {
	int  len;
	ut8  *buf;
	char *buf_hex;
	char *buf_asm;
	RList *equs; // TODO: must be a hash
} RAsmCode;

// TODO: Must use Hashtable instead of this hack
typedef struct {
	char *key;
	char *value;
} RAsmEqu;

typedef struct r_asm_t {
	int  bits;
	int  big_endian;
	int  syntax;
	ut64 pc;
	void *user;
	struct r_asm_plugin_t *cur;
	RAsmFastcall *fastcall;
	RList *plugins;
} RAsm;

typedef int (*RAsmModifyCallback)(RAsm *a, ut8 *buf, int field, ut64 val);

typedef struct r_asm_plugin_t {
	char *name;
	char *arch;
	char *desc;
// TODO: bits -> renamed to bitmask
// use each bit to identify 4,8,16,32,64 bitsize it can be a mask, no need for pointers here
	int *bits;
	int (*init)(void *user);
	int (*fini)(void *user);
	int (*disassemble)(RAsm *a, struct r_asm_aop_t *aop, ut8 *buf, ut64 len);
	int (*assemble)(RAsm *a, struct r_asm_aop_t *aop, const char *buf);
	RAsmModifyCallback modify;
	int (*set_subarch)(RAsm *a, const char *buf);
	struct r_asm_fastcall_t *fastcall[R_ASM_FASTCALL_ARGS];
} RAsmPlugin;

#ifdef R_API
/* asm.c */
R_API RAsm *r_asm_new();
R_API const char *r_asm_fastcall(RAsm *a, int idx, int num);

R_API void r_asm_free(RAsm *a);
R_API int r_asm_modify(RAsm *a, ut8 *buf, int field, ut64 val);
R_API void r_asm_set_user_ptr(RAsm *a, void *user);
R_API int r_asm_add(RAsm *a, RAsmPlugin *foo);
R_API int r_asm_use(RAsm *a, const char *name);
R_API int r_asm_set_bits(RAsm *a, int bits);
R_API int r_asm_set_big_endian(RAsm *a, int boolean);
R_API int r_asm_set_syntax(RAsm *a, int syntax);
R_API int r_asm_set_pc(RAsm *a, ut64 pc);
R_API int r_asm_disassemble(RAsm *a, struct r_asm_aop_t *aop, ut8 *buf, ut64 len);
R_API int r_asm_assemble(RAsm *a, struct r_asm_aop_t *aop, const char *buf);
R_API struct r_asm_code_t* r_asm_mdisassemble(RAsm *a, ut8 *buf, ut64 len);
R_API RAsmCode* r_asm_mdisassemble_hexstr(RAsm *a, const char *hexstr);
R_API struct r_asm_code_t* r_asm_massemble(RAsm *a, const char *buf);

/* code.c */
R_API RAsmCode *r_asm_code_new();
R_API void* r_asm_code_free(struct r_asm_code_t *acode);
R_API int r_asm_code_set_equ (RAsmCode *code, const char *key, const char *value);
R_API char *r_asm_code_equ_replace (RAsmCode *code, char *str);

/* plugin pointers */
extern RAsmPlugin r_asm_plugin_dummy;
extern RAsmPlugin r_asm_plugin_bf;
extern RAsmPlugin r_asm_plugin_java;
extern RAsmPlugin r_asm_plugin_mips;
extern RAsmPlugin r_asm_plugin_x86;
extern RAsmPlugin r_asm_plugin_x86_olly;
extern RAsmPlugin r_asm_plugin_x86_nasm;
extern RAsmPlugin r_asm_plugin_arm;
extern RAsmPlugin r_asm_plugin_armthumb;
extern RAsmPlugin r_asm_plugin_csr;
extern RAsmPlugin r_asm_plugin_m68k;
extern RAsmPlugin r_asm_plugin_ppc;
extern RAsmPlugin r_asm_plugin_sparc;
extern RAsmPlugin r_asm_plugin_psosvm;
extern RAsmPlugin r_asm_plugin_avr;
#endif

#endif
