/* radare - GPL3 - Copyright 2009-2010 nibble<.ds@gmail.com> */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <r_types.h>
#include <r_util.h>
#include <r_lib.h>
#include <r_asm.h>
#include "dis-asm.h"
#include "../arch/arm/arm.h"

static int arm_mode = 0;
static unsigned long Offset = 0;
static char *buf_global = NULL;
static unsigned char bytes[4];

static int arm_buffer_read_memory (bfd_vma memaddr, bfd_byte *myaddr,
		unsigned int length, struct disassemble_info *info) {
	memcpy (myaddr, bytes, length);
	return 0;
}

static int symbol_at_address(bfd_vma addr, struct disassemble_info * info) {
	return 0;
}

static void memory_error_func(int status, bfd_vma memaddr, struct disassemble_info *info) {
	//--
}

static void print_address(bfd_vma address, struct disassemble_info *info) {
	char tmp[32];
	if (buf_global == NULL)
		return;
	sprintf (tmp, "0x%08"PFMT64x"", (ut64)address);
	strcat (buf_global, tmp);
}

static int buf_fprintf(void *stream, const char *format, ...) {
	va_list ap;
	char *tmp;
	if (buf_global == NULL || format == NULL)
		return R_FALSE;
	va_start (ap, format);
 	tmp = malloc (strlen (format)+strlen (buf_global)+2);
	if (tmp == NULL)
		return R_FALSE;
	sprintf (tmp, "%s%s", buf_global, format);
	vsprintf (buf_global, tmp, ap);
	va_end (ap);
	free (tmp);
	return R_TRUE;
}

static int disassemble(struct r_asm_t *a, struct r_asm_aop_t *aop, ut8 *buf, ut64 len) {
	static struct disassemble_info disasm_obj;

	buf_global = aop->buf_asm;
	Offset = a->pc;
	memcpy (bytes, buf, 4); // TODO handle thumb

	/* prepare disassembler */
	memset(&disasm_obj,'\0', sizeof(struct disassemble_info));
	arm_mode = a->bits;
	//info.arch = ARM_EXT_V1|ARM_EXT_V4T|ARM_EXT_V5;
	disasm_obj.buffer = bytes;
	disasm_obj.read_memory_func = &arm_buffer_read_memory;
	disasm_obj.symbol_at_address_func = &symbol_at_address;
	disasm_obj.memory_error_func = &memory_error_func;
	disasm_obj.print_address_func = &print_address;
	disasm_obj.endian = !a->big_endian;
	disasm_obj.fprintf_func = &buf_fprintf;
	disasm_obj.stream = stdout;
	disasm_obj.bytes_per_chunk =
	disasm_obj.bytes_per_line = (a->bits/8);

	aop->buf_asm[0]='\0';
	aop->inst_len = print_insn_arm((bfd_vma)Offset, &disasm_obj);
	if (aop->inst_len == -1)
		strncpy(aop->buf_asm, " (data)", R_ASM_BUFSIZE);
	return aop->inst_len; //(a->bits/8); //aop->inst_len;
}

static int assemble(RAsm *a, RAsmAop *aop, const char *buf) {
	int op = armass_assemble(buf, a->pc, (a->bits==16)?1:0);
	if (op==-1)
		return -1;
	if (a->bits==32)
		r_mem_copyendian (aop->buf, (void *)&op, 4, a->big_endian);
	else r_mem_copyendian (aop->buf, (void *)&op, 2, a->big_endian);
	return (a->bits/8);
}

RAsmPlugin r_asm_plugin_arm = {
	.name = "arm",
	.arch = "arm",
	.bits = (int[]){ 16, 32, 0 },
	.desc = "ARM disassembly plugin",
	.init = NULL,
	.fini = NULL,
	.disassemble = &disassemble,
	.assemble = &assemble 
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_ASM,
	.data = &r_asm_plugin_arm
};
#endif
