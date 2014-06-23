/* radare - LGPL - Copyright 2012 - pancake<nopcode.org>
			     2014 - condret

	this file was based on anal_i8080.c */

#include <string.h>
#include <r_types.h>
#include <r_asm.h>
#include <r_anal.h>
#include <r_reg.h>
#include "../../asm/arch/gb/gbdis.c"
#include "../arch/gb/meta_gb_cmt.c"
#include "../arch/gb/gb_makros.h"

static ut8 gb_op_calljump(RAnal *a, RAnalOp *op, const ut8 *data, ut64 addr){
	if (GB_IS_RAM_DST (data[1],data[2])) {
		op->jump = GB_SOFTCAST (data[1], data[2]);
		r_meta_set_string (a, R_META_TYPE_COMMENT, addr, "--> unpredictable");
		return R_FALSE;
	}
	if (!GB_IS_VBANK_DST (data[1], data[2]))
		op->jump = GB_SOFTCAST(data[1], data[2]);
	else
		op->jump = GB_IB_DST (data[1], data[2], addr);
	return R_TRUE;
}

static int gb_anop(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *data, int len){
	int ilen = gbOpLength (gb_op[data[0]].type);
	if (ilen > len)
		ilen=0;
	memset (op, '\0', sizeof (RAnalOp));
	op->addr = addr;
	op->type = R_ANAL_OP_TYPE_UNK;
	op->size = ilen;
	op->nopcode = 1;
	switch (data[0])
	{
		case 0x00:
		case 0x40:
		case 0x49:
		case 0x52:
		case 0x5b:
		case 0x64:
		case 0x6d:
		case 0x7f:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_NOP;
			break;
		case 0x01:
		case 0x11:
		case 0x21:
		case 0x31:
		case 0xf8:
			op->cycles = 12;
			op->type = R_ANAL_OP_TYPE_MOV;
			break;
		case 0x06:
		case 0x0e:
		case 0x16:
		case 0x1e:
		case 0x26:
		case 0x2e:
		case 0x36:
		case 0x3e:
		case 0xf9:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_MOV;		// LD
			break;
		case 0x03:
		case 0x13:
		case 0x23:
		case 0x33:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_ADD;
			break;
		case 0x04:
		case 0x0c:
		case 0x14:
		case 0x1c:
		case 0x24:
		case 0x2c:
		case 0x3c:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_ADD;		// INC
			break;
		case 0x34:
			op->cycles = 12;
			op->type = R_ANAL_OP_TYPE_ADD;
			break;
		case 0xea:
			op->cycles = 16;
		case 0x08:
			meta_gb_bankswitch_cmt (anal, addr, GB_SOFTCAST (data[1], data[2]));
			op->type = R_ANAL_OP_TYPE_STORE;
			break;
		case 0x02:
		case 0x12:
		case 0x22:
		case 0x32:
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x77:
		case 0xe2:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_STORE;	//LD
			break;
		case 0xe0:
			meta_gb_hardware_cmt (anal, data[1], addr);
			op->cycles = 12;
			op->type = R_ANAL_OP_TYPE_STORE;
			break;
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x47:
		case 0x48:
		case 0x4a:
		case 0x4b:
		case 0x4c:
		case 0x4d:
		case 0x4f:
		case 0x50:
		case 0x51:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5a:
		case 0x5c:
		case 0x5d:
		case 0x5f:
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x65:
		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6a:
		case 0x6b:
		case 0x6c:
		case 0x6f:
		case 0x78:
		case 0x79:
		case 0x7a:
		case 0x7b:
		case 0x7c:
		case 0x7d:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_MOV;		// LD
			break;
		case 0x0a:
		case 0x1a:
		case 0x2a:
		case 0x3a:
		case 0x46:
		case 0x4e:
		case 0x56:
		case 0x5e:
		case 0x66:
		case 0x6e:
		case 0x7e:
		case 0xf2:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_LOAD;
			break;
		case 0xf0:
			meta_gb_hardware_cmt (anal, data[1], addr);
			op->cycles = 12;
			op->type = R_ANAL_OP_TYPE_LOAD;
			break;
		case 0xfa:
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_LOAD;
			break;
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8a:
		case 0x8b:
		case 0x8c:
		case 0x8d:
		case 0x8f:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_ADD;
			break;
		case 0x09:
		case 0x19:
		case 0x29:
		case 0x39:
		case 0x86:
		case 0x8e:
		case 0xc6:
		case 0xce:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_ADD;
			break;
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9a:
		case 0x9b:
		case 0x9c:
		case 0x9d:
		case 0x9f:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_SUB;
			break;
		case 0x96:
		case 0x9e:
		case 0xd6:
		case 0xde:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_SUB;
			break;
		case 0xa0:
		case 0xa1:
		case 0xa2:
		case 0xa3:
		case 0xa4:
		case 0xa5:
		case 0xa7:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_AND;
			break;
		case 0xa6:
		case 0xe6:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_AND;
			break;
		case 0x07:
		case 0x17:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_ROL;
			break;
		case 0x0f:
		case 0x1f:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_ROR;
			break;
		case 0x2f:					//cpl
		case 0x3f:					//ccf
		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
		case 0xac:
		case 0xad:
		case 0xaf:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_XOR;
			break;
		case 0xae:
		case 0xee:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_XOR;
			break;
		case 0xb0:
		case 0xb1:
		case 0xb2:
		case 0xb3:
		case 0xb4:
		case 0xb5:
		case 0xb7:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_OR;
			break;
		case 0xb6:
		case 0xf6:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_OR;
			break;
		case 0xb8:
		case 0xb9:
		case 0xba:
		case 0xbb:
		case 0xbc:
		case 0xbd:
		case 0xbf:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_CMP;
			break;
		case 0xbe:
		case 0xfe:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_CMP;
			break;
		case 0xc0:
		case 0xc8:
		case 0xd0:
		case 0xd8:
			op->eob = 1;
			op->cycles = 20;
			op->failcycles = 8;
			op->type = R_ANAL_OP_TYPE_CRET;
			break;
		case 0xc9:
		case 0xd9:
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_RET;
			break;
		case 0x0b:
		case 0x1b:
		case 0x2b:
		case 0x3b:
			op->cycles = 8;
			op->type = R_ANAL_OP_TYPE_SUB;
			break;
		case 0x05:
		case 0x0d:
		case 0x15:
		case 0x1d:
		case 0x25:
		case 0x2d:
		case 0x3d:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_SUB;		// DEC
			break;
		case 0x35:
			op->cycles = 12;
			op->type = R_ANAL_OP_TYPE_SUB;
			break;
		case 0xc5:
		case 0xd5:
		case 0xe5:
		case 0xf5:
			op->cycles = 16;
			op->stackop = R_ANAL_STACK_INC;
			op->stackptr = 2;
			op->type = R_ANAL_OP_TYPE_PUSH;
			break;
		case 0xc1:
		case 0xd1:
		case 0xe1:
		case 0xf1:
			op->cycles = 12;
			op->stackop = R_ANAL_STACK_INC;
			op->stackptr = -2;
			op->type = R_ANAL_OP_TYPE_POP;
			break;
		case 0xc3:
			if( gb_op_calljump (anal, op, data, addr)) {
				op->type = R_ANAL_OP_TYPE_JMP;
			} else {
				op->type = R_ANAL_OP_TYPE_UJMP;
				op->eob = 1;
			}
			op->cycles = 16;
			op->fail = addr + ilen;
			break;
		case 0x18:					// JR
			op->jump = addr + ilen + (st8)data[1];
			op->fail = addr + ilen;
			op->cycles = 12;
			op->type = R_ANAL_OP_TYPE_JMP;
			break;
		case 0x20:
		case 0x28:
		case 0x30:
		case 0x38:					//JR cond
			op->jump = addr + ilen + (st8)data[1];
			op->fail = addr + ilen;
			op->cycles = 12;
			op->failcycles = 8;
			op->type = R_ANAL_OP_TYPE_CJMP;
			break;
		case 0xc2:
		case 0xca:
		case 0xd2:
		case 0xda:
			if( gb_op_calljump (anal, op, data, addr)) {
				op->type = R_ANAL_OP_TYPE_CJMP;
			} else {
				op->type = R_ANAL_OP_TYPE_UJMP;
				op->eob = 1;
			}
			op->cycles = 16;
			op->failcycles = 12;
			op->fail = addr+ilen;
			break;
		case 0xe9:
			op->cycles = 4;
			op->type = R_ANAL_OP_TYPE_UJMP;
			break;
		case 0x76:
			op->type = R_ANAL_OP_TYPE_CJMP;
			op->eob = 1;				//halt migth wait for interrupts
			op->fail = addr + ilen;
			if(len > 1)
				op->jump = addr + gbOpLength (gb_op[data[1]].type) + ilen;
			break;
		case 0xcd:
			if ( gb_op_calljump (anal, op, data, addr))
				op->type = R_ANAL_OP_TYPE_CALL;
			else	op->type = R_ANAL_OP_TYPE_UCALL;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 24;
			break;
		case 0xc4:
		case 0xcc:
		case 0xd4:
		case 0xdc:
			if( gb_op_calljump (anal, op, data, addr))
				op->type = R_ANAL_OP_TYPE_CCALL;
			else	op->type = R_ANAL_OP_TYPE_UCALL;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 24;
			op->failcycles = 12;
			break;
                case 0xc7:				//rst 0
			op->jump = 0x00;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0xcf:				//rst 8
                        op->jump = 0x08;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
                        break;
		case 0xd7:				//rst 16
			op->jump = 0x10;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0xdf:				//rst 24
			op->jump = 0x18;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0xe7:				//rst 32
			op->jump = 0x20;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0xef:				//rst 40
			op->jump = 0x28;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0xf7:				//rst 48
			op->jump = 0x30;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0xff:				//rst 56
			op->jump = 0x38;
			op->fail = addr + ilen;
			op->eob = 1;
			op->cycles = 16;
			op->type = R_ANAL_OP_TYPE_CALL;
			break;
		case 0x27:				//daa
		case 0x37:				//scf
		case 0xf3:				//di
		case 0xfb:				//ei
			op->cycles = 4;
		case 0x10:				//stop
			op->type = R_ANAL_OP_TYPE_NULL;
			break;
		case 0xcb:
			op->nopcode = 2;
			switch (data[1]/8)
			{
				case 0:
				case 2:
				case 4:
				case 6:				//swap
					if (data[1]%8 == 6)
						op->cycles = 16;
					else	op->cycles = 8;
					op->type = R_ANAL_OP_TYPE_ROL;
					break;
				case 1:
				case 3:
				case 5:
				case 7:
					if (data[1]%8 == 6)
						op->cycles = 16;
					else	op->cycles = 8;
					op->type = R_ANAL_OP_TYPE_ROR;
					break;
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
					if (data[1]%8 == 6)
						op->cycles = 12;
					else	op->cycles = 8;
					op->type = R_ANAL_OP_TYPE_AND;
					break;			//bit
				case 16:
				case 17:
				case 18:
				case 19:
				case 20:
				case 21:
				case 22:
				case 23:
					if (data[1]%8 == 6)
						op->cycles = 16;
					else	op->cycles = 8;
					op->type = R_ANAL_OP_TYPE_NULL;
					break;			//set
				case 24:
				case 25:
				case 26:
				case 27:
				case 28:
				case 29:
				case 30:
				case 31:
					if (data[1]%8 == 6)
						op->cycles = 16;
					else	op->cycles = 8;
					op->type = R_ANAL_OP_TYPE_NULL;
					break;			//res
			}
	}
	return op->size;
}

/*
	The reg-profile below does not represent the real gameboy registers.
		->There is no such thing like m, mpc or mbc. there is only pc.
	m and mbc should make it easier to inspect the current mbc-state, because
	the mbc can be seen as a register but it isn't. For the Gameboy the mbc is invisble.
*/

static int set_reg_profile(RAnal *anal) {
	return r_reg_set_profile_string (anal->reg,
		"=pc	mpc\n"
		"=sp	sp\n"
		"=a0	af\n"
		"=a1	bc\n"
		"=a2	de\n"
		"=a3	hl\n"

		"gpr	mpc	.32	0	0\n"
		"gpr	pc	.16	0	0\n"
		"gpr	m	.16	2	0\n"

		"gpr	sp	.16	4	0\n"

		"gpr	af	.16	6	0\n"
		"gpr	f	.8	6	0\n"
		"gpr	a	.8	7	0\n"
		"gpr	Z	.1	.55	0\n"
		"gpr	N	.1	.54	0\n"
		"gpr	H	.1	.53	0\n"
		"gpr	C	.1	.52	0\n"

		"gpr	bc	.16	8	0\n"
		"gpr	c	.8	8	0\n"
		"gpr	b	.8	9	0\n"

		"gpr	de	.16	10	0\n"
		"gpr	e	.8	10	0\n"
		"gpr	d	.8	11	0\n"

		"gpr	hl	.16	12	0\n"
		"gpr	l	.8	12	0\n"
		"gpr	h	.8	13	0\n"

		"gpr	mbcrom	.16	14	0\n"
		"gpr	mbcram	.16	16	0\n"

		"gpr	ime	.1	18	0\n");
}

struct r_anal_plugin_t r_anal_plugin_gb = {
	.name = "gb",
	.desc = "Gameboy CPU code analysis plugin",
	.license = "LGPL3",
	.arch = R_SYS_ARCH_Z80,
	.bits = 16,
	.init = NULL,
	.fini = NULL,
	.op = &gb_anop,
	.set_reg_profile = &set_reg_profile,
	.fingerprint_bb = NULL,
	.fingerprint_fcn = NULL,
	.diff_bb = NULL,
	.diff_fcn = NULL,
	.diff_eval = NULL
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_ANAL,
	.data = &r_anal_plugin_gb
};
#endif
