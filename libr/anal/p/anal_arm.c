/* radare - LGPL - Copyright 2007-2011 */
/*   pancake<nopcode.org> */

#include <string.h>
#include <r_types.h>
#include <r_lib.h>
#include <r_asm.h>
#include <r_anal.h>

/* DEPRECATE ?? */
#include "arm/arm.h"

static unsigned int disarm_branch_offset (unsigned int pc, unsigned int insoff) {
	unsigned int add = insoff << 2;
	/* zero extend if higher is 1 (0x02000000) */
	if ((add & 0x02000000) == 0x02000000)
		add |= 0xFC000000;
	return add + pc + 8;
}

#define IS_BRANCH(x) \
	((x&ARM_BRANCH_I_MASK) == ARM_BRANCH_I)

#define IS_BRANCHL(x) \
	(IS_BRANCH(x) && (x&ARM_BRANCH_LINK) == ARM_BRANCH_LINK)

#define IS_RETURN(x) \
	((x&(ARM_DTM_I_MASK|ARM_DTM_LOAD|(1<<15))) == (ARM_DTM_I|ARM_DTM_LOAD|(1<<15)))

//if ( (inst & ( ARM_DTX_I_MASK | ARM_DTX_LOAD  | ( ARM_DTX_RD_MASK ) ) ) == ( ARM_DTX_LOAD | ARM_DTX_I | ( ARM_PC << 12 ) ) )
#define IS_UNKJMP(x) \
	(( (( ARM_DTX_RD_MASK ) ) ) == ( ARM_DTX_LOAD | ARM_DTX_I | ( ARM_PC << 12 ) ))

#define IS_LOAD(x) \
	((x&ARM_DTX_LOAD) == (ARM_DTX_LOAD))

#define IS_CONDAL(x) \
	((x&ARM_COND_MASK)==ARM_COND_AL)

#define IS_EXITPOINT(x) \
	(IS_BRANCH (x) || IS_RETURN (x) || IS_UNKJMP (x))

#define API static
#include "../../asm/arch/arm/armthumb.c"

static int op_thumb(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *data, int len) {
	int op_code;
	ut16 *_ins = (ut16*)data;
	ut16 ins = *_ins;
	op->length = armthumb_length (ins);
	// TODO: handle 32bit instructions (branches are not correctly decoded //

	/* CMP */
	if (((ins & _(B1110,0,0,0)) == _(B0010,0,0,0) )
                && (1 == (ins & _(1,B1000,0,0)) >> 11)) { // dp3
		op->type = R_ANAL_OP_TYPE_CMP;
		return op->length;
	}
        if ( (ins & _(B1111,B1100,0,0)) == _(B0100,0,0,0) ) {
                op_code = (ins & _(0,B0011,B1100,0)) >> 6;
                if (op_code == 8 || op_code == 10) { // dp5
			op->type = R_ANAL_OP_TYPE_CMP;
			return op->length;
		}
	}
        if ( (ins & _(B1111,B1100,0,0)) == _(B0100,B0100,0,0) ) {
                op_code = (ins & _(0,B0011,0,0)) >> 8; // dp8
		if (op_code== 1) {
			op->type = R_ANAL_OP_TYPE_CMP;
			return op->length;
		}
	}

	if (ins == 0xbf) {
		// TODO: add support for more NOP instructions
		op->type = R_ANAL_OP_TYPE_NOP;
        } else if (((op_code = ((ins & _(B1111,B1000,0,0)) >> 11)) >= 12 && op_code <= 17 )) {
		if (op_code%2)
			op->type = R_ANAL_OP_TYPE_LOAD;
		else op->type = R_ANAL_OP_TYPE_STORE;
        } else if ( (ins & _(B1111,0,0,0)) == _(B0101,0,0,0) ) {
                op_code = (ins & _(0,B1110,0,0)) >> 9;
		if (op_code%2)
			op->type = R_ANAL_OP_TYPE_LOAD;
		else op->type = R_ANAL_OP_TYPE_STORE;
	} else if ( (ins & _(B1111,0,0,0)) == _(B1101,0,0,0) ) {
		// BNE..
		int delta = (ins & _(0,0,B1111,B1111));
		op->type = R_ANAL_OP_TYPE_CJMP;
		op->jump = addr+4+(delta<<1);
		op->fail = addr+4;
        } else if ( (ins & _(B1110,B1000,0,0)) == _(B1110,0,0,0) ) {
		// B
		int delta = (ins & _(0,0,B1111,B1111));
		op->type = R_ANAL_OP_TYPE_JMP;
		op->jump = addr+4+(delta<<1);
		op->fail = addr+4;
        } else if ( (ins & _(B1111,B1111,0,0)) == _(B0100,B0111,0,0) ) {
		// BLX
		op->type = R_ANAL_OP_TYPE_UJMP;
		op->jump = addr+4+(ut32)((ins & _(0,0,B0111,B1000)) >> 3);
		op->fail = addr+4;
        } else if ( (ins & _(B1111,B1111,0,0)) == _(B1011,B1110,0,0) ) {
		op->type = R_ANAL_OP_TYPE_TRAP;
		op->value = (ut64)(ins>>8);
        } else if ( (ins & _(B1111,B1111,0,0)) == _(B1101,B1111,0,0)) {
		op->type = R_ANAL_OP_TYPE_SWI;
		op->value = (ut64)(ins>>8);
	}
	return op->length;
}

static int arm_op(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *data, int len) {
	ut32 branch_dst_addr, i = 0;
	ut32* code = (ut32 *)data;
	const ut8 *b = (ut8 *)data;

	if (data == NULL)
		return 0;
	memset (op, '\0', sizeof (RAnalOp));
	op->addr = addr;
	op->type = R_ANAL_OP_TYPE_UNK;
	op->jump = op->fail = -1;
	op->ref = op->value = -1;
	if (anal->bits==16)
		return op_thumb(anal, op, addr, data, len);
	op->length = 4;
#if 0
	fprintf(stderr, "CODE %02x %02x %02x %02x\n",
		codeA[0], codeA[1], codeA[2], codeA[3]);
#endif
    	// 0x000037b8  00:0000   0             800000ef  svc 0x00000080
	if (b[3]==0xef) {
		op->type = R_ANAL_OP_TYPE_SWI;
		op->value = (b[0] | (b[1]<<8) | (b[2]<<2));
	} else
	if (b[3]==0xe5) {
		if (b[2]==0x9f) {
			/* STORE */
			op->type = R_ANAL_OP_TYPE_STORE;
			op->stackop = R_ANAL_STACK_SET;

//printf ("FUCKING PT Rpc AT 0x%08llx + %d\n", addr, b[0]);
			//op->ref = 4+addr+b[0]+(b[1]&4<<8);
			op->ref = 8+addr+b[0]+((b[1]&0xf)<<8);
			op->refptr = R_TRUE;
		} else
		if ((b[1]&0xf0) == 0xf0) {
			//ldr pc, [pc, #1] ; 
			op->type = R_ANAL_OP_TYPE_UJMP;
			op->type = R_ANAL_OP_TYPE_RET; // FAKE FOR FUN
			//op->stackop = R_ANAL_STACK_SET;
			op->jump = 1234;
			//op->ref = 4+addr+b[0]; // sure? :)
			//op->refptr = R_TRUE;
		}
	} else
//eprintf("0x%08x\n", code[i] & ARM_DTX_LOAD);
	// 0x0001B4D8,           1eff2fe1        bx    lr
	if (b[3]==0xe2 && b[2]==0x8d && b[1]==0xd0) {
		// ADD SP, SP, ...
		op->type = R_ANAL_OP_TYPE_ADD;
		op->stackop = R_ANAL_STACK_INCSTACK;
		op->value = -b[0];
	} else
	if (b[3]==0xe2 && b[2]==0x4d && b[1]==0xd0) {
		// SUB SP, SP, ..
		op->type = R_ANAL_OP_TYPE_SUB;
		op->stackop = R_ANAL_STACK_INCSTACK;
		op->value = b[0];
	} else
	if (b[3]==0xe2 && b[2]==0x4c && b[1]==0xb0) {
		// SUB SP, FP, ..
		op->type = R_ANAL_OP_TYPE_SUB;
		op->stackop = R_ANAL_STACK_INCSTACK;
		op->value = -b[0];
	} else
	if (b[3]==0xe2 && b[2]==0x4b && b[1]==0xd0) {
		// SUB SP, IP, ..
		op->type = R_ANAL_OP_TYPE_SUB;
		op->stackop = R_ANAL_STACK_INCSTACK;
		op->value = -b[0];
	} else
	if( (code[i] == 0x1eff2fe1) ||(code[i] == 0xe12fff1e)) { // bx lr
		op->type = R_ANAL_OP_TYPE_RET;
		op->eob = 1;
	} else
	if ((code[i] & ARM_DTX_LOAD)) { //IS_LOAD(code[i])) {
		ut32 ptr = 0;
		op->type = R_ANAL_OP_TYPE_MOV;
		if (b[2]==0x1b) {
			/* XXX pretty incomplete */
			op->stackop = R_ANAL_STACK_GET;
			op->ref = b[0];
			//var_add_access(addr, -b[0], 1, 0); // TODO: set/get (the last 0)
		} else {
			//ut32 oaddr = addr+8+b[0];
			//XXX TODO ret = radare_read_at(oaddr, (ut8*)&ptr, 4);
			if (anal->bits == 32) {
				b = (ut8*)&ptr;
				op->ref = b[0] + (b[1]<<8) + (b[2]<<16) + (b[3]<<24);
				//XXX data_xrefs_add(oaddr, op->ref, 1);
				//TODO change data type to pointer
			} else op->ref = 0;
		}
	}

	if (IS_EXITPOINT (code[i])) {
		branch_dst_addr = disarm_branch_offset (addr, code[i]&0x00FFFFFF);
		op->ref = 0;
		if (IS_BRANCHL (code[i])) {
			if (IS_BRANCH (code[i])) {
				op->type = R_ANAL_OP_TYPE_CALL;
				op->jump = branch_dst_addr;
				op->fail = addr + 4 ;
				op->eob  = 1;
			} else {
				op->type = R_ANAL_OP_TYPE_RET;
				op->eob = 1;
			}
		} else if (IS_BRANCH (code[i])) {
			if (IS_CONDAL (code[i])) {
				op->type = R_ANAL_OP_TYPE_JMP;
				op->jump = branch_dst_addr;
				op->eob = 1;
			} else {
				op->type = R_ANAL_OP_TYPE_CJMP;
				op->jump = branch_dst_addr;
				op->fail = addr + 4;
				op->eob  = 1;
			}
		} else {
			//unknown jump o return
			op->type = R_ANAL_OP_TYPE_UJMP;
			op->eob = 1;
		}
	}
	return op->length;
}
static int set_reg_profile(RAnal *anal) {
	/* XXX Dupped Profiles */
	return r_reg_set_profile_string (anal->reg,
			"=pc	r15\n"
			"=sp	r14\n" // XXX
			"=a0	r0\n"
			"=a1	r1\n"
			"=a2	r2\n"
			"=a3	r3\n"
			"gpr	lr	.32	56	0\n" // r14
			"gpr	pc	.32	60	0\n" // r15

			"gpr	r0	.32	0	0\n"
			"gpr	r1	.32	4	0\n"
			"gpr	r2	.32	8	0\n"
			"gpr	r3	.32	12	0\n"
			"gpr	r4	.32	16	0\n"
			"gpr	r5	.32	20	0\n"
			"gpr	r6	.32	24	0\n"
			"gpr	r7	.32	28	0\n"
			"gpr	r8	.32	32	0\n"
			"gpr	r9	.32	36	0\n"
			"gpr	r10	.32	40	0\n"
			"gpr	r11	.32	44	0\n"
			"gpr	r12	.32	48	0\n"
			"gpr	r13	.32	52	0\n"
			"gpr	r14	.32	56	0\n"
			"gpr	r15	.32	60	0\n"
			"gpr	r16	.32	64	0\n"
			"gpr	r17	.32	68	0\n");
}

struct r_anal_plugin_t r_anal_plugin_arm = {
	.name = "arm",
	.arch = R_SYS_ARCH_ARM,
	.bits = 32,
	.desc = "ARM code analysis plugin",
	.init = NULL,
	.fini = NULL,
	.op = &arm_op,
	.set_reg_profile = set_reg_profile,
	.fingerprint_bb = NULL,
	.fingerprint_fcn = NULL,
	.diff_bb = NULL,
	.diff_fcn = NULL,
	.diff_eval = NULL
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_ANAL,
	.data = &r_anal_plugin_arm
};
#endif
