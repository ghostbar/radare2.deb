/* radare2 - LGPL - Copyright 2013-2016 - pancake */

#include <r_asm.h>
#include <r_lib.h>
#include <capstone/capstone.h>
#include <capstone/mips.h>

// http://www.mrc.uidaho.edu/mrc/people/jff/digital/MIPSir.html

#define OPERAND(x) insn->detail->mips.operands[x]
#define REGID(x) insn->detail->mips.operands[x].reg
#define REG(x) cs_reg_name (*handle, insn->detail->mips.operands[x].reg)
#define IMM(x) insn->detail->mips.operands[x].imm
#define MEMBASE(x) cs_reg_name(*handle, insn->detail->mips.operands[x].mem.base)
#define MEMINDEX(x) insn->detail->mips.operands[x].mem.index
#define MEMDISP(x) insn->detail->mips.operands[x].mem.disp
#define OPCOUNT() insn->detail->mips.op_count
// TODO scale and disp

#define SET_VAL(op,i) \
	if (i<OPCOUNT() && OPERAND(i).type == MIPS_OP_IMM) {\
		op->val = OPERAND(i).imm;\
	}

#define CREATE_SRC_DST_3(op) \
	(op)->src[0] = r_anal_value_new ();\
	(op)->src[1] = r_anal_value_new ();\
	(op)->dst = r_anal_value_new ();

#define CREATE_SRC_DST_2(op) \
	(op)->src[0] = r_anal_value_new ();\
	(op)->dst = r_anal_value_new ();

#define SET_SRC_DST_3_REGS(op) \
	CREATE_SRC_DST_3 (op);\
	(op)->dst->reg = r_reg_get (anal->reg, REG (0), R_REG_TYPE_GPR);\
	(op)->src[0]->reg = r_reg_get (anal->reg, REG (1), R_REG_TYPE_GPR);\
	(op)->src[1]->reg = r_reg_get (anal->reg, REG (2), R_REG_TYPE_GPR);

#define SET_SRC_DST_3_IMM(op) \
	CREATE_SRC_DST_3 (op);\
	(op)->dst->reg = r_reg_get (anal->reg, REG (0), R_REG_TYPE_GPR);\
	(op)->src[0]->reg = r_reg_get (anal->reg, REG (1), R_REG_TYPE_GPR);\
	(op)->src[1]->imm = IMM (2);

#define SET_SRC_DST_2_REGS(op) \
	CREATE_SRC_DST_2 (op);\
	(op)->dst->reg = r_reg_get (anal->reg, REG (0), R_REG_TYPE_GPR);\
	(op)->src[0]->reg = r_reg_get (anal->reg, REG (1), R_REG_TYPE_GPR);

#define SET_SRC_DST_3_REG_OR_IMM(op) \
	if (OPERAND(2).type == MIPS_OP_IMM) {\
		SET_SRC_DST_3_IMM (op);\
	} else if (OPERAND(2).type == MIPS_OP_REG) {\
		SET_SRC_DST_3_REGS (op);\
	}


// ESIL macros:

// put the sign bit on the stack
#define ES_IS_NEGATIVE(arg) "1,"arg",<<<,1,&"

// jump to address
#define ES_J(addr) addr",$jt,=,1,$ds,="

// call with delay slot
#define ES_CALL_DR(ra, addr) "pc,4,+,"ra",=,"ES_J(addr)
#define ES_CALL_D(addr) ES_CALL_DR("ra", addr)

// call without delay slot
#define ES_CALL_NDR(ra, addr) "pc,"ra",=,"ES_J(addr)
#define ES_CALL_ND(addr) ES_CALL_NDR("ra", addr)

// emit ERR trap if executed in a delay slot
#define ES_TRAP_DS() "0,$ds,>,?{,$$,1,TRAP,BREAK,}"

// sign extend 32 -> 64
#define ES_SIGN_EXT64(arg) \
	arg",0x80000000,&,0,<,?{,"\
		"0xffffffff00000000,"arg",|=,"\
	"}"

#define PROTECT_ZERO() \
	if (REG(0)[0]=='z'){\
		r_strbuf_appendf (&op->esil, ",");\
	} else

#define ESIL_LOAD(size) \
	PROTECT_ZERO () {\
		r_strbuf_appendf (&op->esil, "%s,["size"],%s,=",\
			ARG(1), REG(0));\
	}

static const char *arg(csh *handle, cs_insn *insn, char *buf, int n) {
	*buf = 0;
	switch (insn->detail->mips.operands[n].type) {
	case MIPS_OP_INVALID:
		break;
	case MIPS_OP_REG:
		sprintf (buf, "%s",
			cs_reg_name (*handle,
				insn->detail->mips.operands[n].reg));
		break;
	case MIPS_OP_IMM:
		{
			st64 x = (st64)insn->detail->mips.operands[n].imm;
			sprintf (buf, "%"PFMT64d, x);
		}
		break;
	case MIPS_OP_MEM:
		{
			int disp = insn->detail->mips.operands[n].mem.disp;
		if (disp<0) {
		sprintf (buf, "%"PFMT64d",%s,-",
			(ut64)-insn->detail->mips.operands[n].mem.disp,
			cs_reg_name (*handle,
				insn->detail->mips.operands[n].mem.base));
		} else {
		sprintf (buf, "0x%"PFMT64x",%s,+",
			(ut64)insn->detail->mips.operands[n].mem.disp,
			cs_reg_name (*handle,
				insn->detail->mips.operands[n].mem.base));
		}
		}
		break;
	}
	return buf;
}

#define ARG(x) (*str[x]!=0)?str[x]:arg(handle, insn, str[x], x)

static int analop_esil(RAnal *a, RAnalOp *op, ut64 addr, const ut8 *buf, int len, csh *handle, cs_insn *insn) {
	char str[8][32];
	int i;
	r_strbuf_init (&op->esil);
	r_strbuf_set (&op->esil, "");

	if (insn) {
		// caching operands
		for (i=0; i<insn->detail->mips.op_count && i<8; i++) {
			*str[i]=0;
			ARG (i);
		}
	}

	if (insn)
	switch (insn->id) {
	case MIPS_INS_NOP:
		r_strbuf_setf (&op->esil, ",");
		break;
	case MIPS_INS_BREAK:
		r_strbuf_setf (&op->esil, "%s,%s,TRAP", ARG (0), ARG (0));
		break;
	case MIPS_INS_SW:
	case MIPS_INS_SWL:
	case MIPS_INS_SWR:
		r_strbuf_appendf (&op->esil, "%s,%s,=[4]",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_SH:
		r_strbuf_appendf (&op->esil, "%s,%s,=[2]",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_SWC1:
	case MIPS_INS_SWC2:
		r_strbuf_setf (&op->esil, "%s,$", ARG (1));
		break;
	case MIPS_INS_SB:
		r_strbuf_appendf (&op->esil, "%s,%s,=[1]",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_CMP:
	case MIPS_INS_CMPU:
	case MIPS_INS_CMPGU:
	case MIPS_INS_CMPGDU:
	case MIPS_INS_CMPI:
		r_strbuf_appendf (&op->esil, "%s,%s,==", ARG (1), ARG (0));
		break;
	case MIPS_INS_SHRAV:
	case MIPS_INS_SHRAV_R:
	case MIPS_INS_SHRA:
	case MIPS_INS_SHRA_R:
	case MIPS_INS_SRA:
		r_strbuf_appendf (&op->esil, "%s,%s,>>,31,%s,>>,?{,32,%s,-,%s,1,<<,1,-,<<,}{,0,},|,%s,=,",
			ARG (2), ARG (1), ARG (1), ARG (2), ARG (2), ARG (0));
		break;
	case MIPS_INS_SHRL:
		// suffix 'S' forces conditional flag to be updated
	case MIPS_INS_SRLV:
	case MIPS_INS_SRL:
		r_strbuf_appendf (&op->esil, "%s,%s,>>,%s,=", ARG (2), ARG (1), ARG (0));
		break;
	case MIPS_INS_SLLV:
	case MIPS_INS_SLL:
		r_strbuf_appendf (&op->esil, "%s,%s,<<,%s,=", ARG (2), ARG (1), ARG (0));
		break;
	case MIPS_INS_BAL:
	case MIPS_INS_JAL:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () "," ES_CALL_D ("%s"), ARG (0));
		break;
	case MIPS_INS_JALR:
	case MIPS_INS_JALRS:
		if (OPCOUNT () < 2) {
			r_strbuf_appendf (&op->esil, ES_TRAP_DS () "," ES_CALL_D ("%s"), ARG (0));
		} else {
			PROTECT_ZERO () {
				r_strbuf_appendf (&op->esil, ES_TRAP_DS () "," ES_CALL_DR ("%s","%s"), ARG (0), ARG (1));
			}
		}
		break;
	case MIPS_INS_JALRC: // no delay
		if (OPCOUNT () < 2) {
			r_strbuf_appendf (&op->esil, ES_TRAP_DS () "," ES_CALL_ND ("%s"), ARG (0));
		} else {
			PROTECT_ZERO () {
				r_strbuf_appendf (&op->esil, ES_TRAP_DS () "," ES_CALL_NDR ("%s","%s"), ARG (0), ARG (1));
			}
		}
		break;
	case MIPS_INS_JRADDIUSP:
		// increment stackpointer in X and jump to %ra
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",%d,sp,+=,"ES_J ("ra"), ARG (0));
		break;
	case MIPS_INS_JR:
	case MIPS_INS_JRC:
	case MIPS_INS_J:
	case MIPS_INS_B: // ???
		// jump to address with conditional
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () "," ES_J ("%s"), ARG (0));
		break;
	case MIPS_INS_BNE:  // bne $s, $t, offset
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",%s,%s,==,$z,!,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1), ARG (2));
		break;
	case MIPS_INS_BEQ:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",%s,%s,==,$z,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1), ARG (2));
		break;
	case MIPS_INS_BZ:
	case MIPS_INS_BEQZ:
	case MIPS_INS_BEQZC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",%s,0,==,$z,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BNEZ:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",%s,0,==,$z,!,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BEQZALC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",%s,0,==,$z,?{,"ES_CALL_ND ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BLEZ:
	case MIPS_INS_BLEZC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,%s,==,$z,?{,"ES_J ("%s")",BREAK,},",
			ARG (0), ARG (1));
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",1,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BGEZ:
	case MIPS_INS_BGEZC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BGEZAL:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_CALL_D ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BGEZALC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_CALL_ND ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BGTZALC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,%s,==,$z,?{,BREAK,},", ARG(0));
		r_strbuf_appendf (&op->esil, "0,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_CALL_ND ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BLTZAL:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",1,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_CALL_D ("%s")",}", ARG(0), ARG(1));
		break;
	case MIPS_INS_BLTZ:
	case MIPS_INS_BLTZC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",1,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_J ("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BGTZ:
	case MIPS_INS_BGTZC:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,%s,==,$z,?{,BREAK,},", ARG (0));
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,"ES_IS_NEGATIVE ("%s")",==,$z,?{,"ES_J("%s")",}",
			ARG (0), ARG (1));
		break;
	case MIPS_INS_BTEQZ:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,t,==,$z,?{,"ES_J ("%s")",}", ARG (0));
		break;
	case MIPS_INS_BTNEZ:
		r_strbuf_appendf (&op->esil, ES_TRAP_DS () ",0,t,==,$z,!,?{,"ES_J ("%s")",}", ARG (0));
		break;
	case MIPS_INS_MOV:
	case MIPS_INS_MOVE:
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "%s,%s,=", ARG (1), REG (0));
		}
		break;
	case MIPS_INS_MOVZ:
	case MIPS_INS_MOVF:
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "0,%s,==,$z,?{,%s,%s,=,}",
				ARG (2), ARG (1), REG (0));
		}
		break;
	case MIPS_INS_MOVT:
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "1,%s,==,$z,?{,%s,%s,=,}",
				ARG (2), ARG (1), REG (0));
		}
		break;
	case MIPS_INS_FSUB:
	case MIPS_INS_SUB:
	case MIPS_INS_SUBU:
	case MIPS_INS_DSUB:
	case MIPS_INS_DSUBU:
		PROTECT_ZERO () {
			r_strbuf_appendf(&op->esil, "%s,%s,-,%s,=",
				ARG (2), ARG (1), ARG (0));
		}
		break;
	case MIPS_INS_NEG:
	case MIPS_INS_NEGU:
		r_strbuf_appendf (&op->esil, "%s,0,-,%s,=,",
			ARG (1), ARG (0));
		break;

	/** signed -- sets overflow flag */
	case MIPS_INS_ADD:
		{
		PROTECT_ZERO () {
			r_strbuf_appendf(&op->esil, "%s,%s,+,%s,=",
				ARG (1), ARG (2), ARG (0));
#if 0
			r_strbuf_appendf (&op->esil,
				"0,32,%s,%s,+,>>,>,?{,1,TRAP,}{,%s,%s,+,%s,=,}",
				ARG(2), ARG(1), ARG(2), ARG(1), ARG(0));
#endif
		}
		}
		break;
	case MIPS_INS_ADDI:
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "30,0x80000000,%s,%s,^,&,>>,31,0x80000000,%s,&,0x80000000,%s,%s,+,&,^,>>,|,1,==,?{,$$,1,TRAP,}{,%s,%s,+,%s,=,}",
				ARG(2), ARG(1), ARG(2), ARG(2), ARG(1), ARG(2), ARG(1), ARG(0));
		}
		break;
	case MIPS_INS_DADD:
	case MIPS_INS_DADDI:
	/** unsigned */
	case MIPS_INS_ADDU:
	case MIPS_INS_ADDIU:
	case MIPS_INS_DADDIU:
		{
		const char *arg0 = ARG(0);
		const char *arg1 = ARG(1);
		const char *arg2 = ARG(2);
		PROTECT_ZERO () {
			if (*arg2 == '-') {
				r_strbuf_appendf (&op->esil, "%s,%s,-,%s,=",
						arg2+1, arg1, arg0);
			} else {
				r_strbuf_appendf (&op->esil, "%s,%s,+,%s,=",
						arg2, arg1, arg0);
			}
		}
		}
		break;
	case MIPS_INS_LI:
		r_strbuf_appendf (&op->esil, "0x%"PFMT64x",%s,=", IMM(1), ARG(0));
		break;
	case MIPS_INS_LUI:
		r_strbuf_appendf (&op->esil, "0x%"PFMT64x"0000,%s,=", IMM(1), ARG(0));
		break;
	case MIPS_INS_LB:
	case MIPS_INS_LBU:
		//one of these is wrong
		ESIL_LOAD ("1");
		break;
	case MIPS_INS_LW:
	case MIPS_INS_LWC1:
	case MIPS_INS_LWC2:
	case MIPS_INS_LWL:
	case MIPS_INS_LWR:
	case MIPS_INS_LWU:
	case MIPS_INS_LL:
	case MIPS_INS_LLD:
	case MIPS_INS_LD:
	case MIPS_INS_LDI:
	case MIPS_INS_LDL:
	case MIPS_INS_LDC1:
	case MIPS_INS_LDC2:
		ESIL_LOAD ("4");
		break;

	case MIPS_INS_LWX:
	case MIPS_INS_LH:
	case MIPS_INS_LHU:
	case MIPS_INS_LHX:
		ESIL_LOAD ("2");
		break;

	case MIPS_INS_AND:
	case MIPS_INS_ANDI:
		{
		const char *arg0 = ARG(0);
		const char *arg1 = ARG(1);
		const char *arg2 = ARG(2);
		if (!strcmp (arg0, arg1)) {
			r_strbuf_appendf (&op->esil, "%s,%s,&=", arg2, arg1);
		} else {
			r_strbuf_appendf (&op->esil, "%s,%s,&,%s,=", arg2, arg1, arg0);
		}
		}
		break;
	case MIPS_INS_OR:
	case MIPS_INS_ORI:
		{
		const char *arg0 = ARG(0);
		const char *arg1 = ARG(1);
		const char *arg2 = ARG(2);
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "%s,%s,|,%s,=",
				arg2, arg1, arg0);
		}
		}
		break;
	case MIPS_INS_XOR:
	case MIPS_INS_XORI:
		{
		const char *arg0 = ARG(0);
		const char *arg1 = ARG(1);
		const char *arg2 = ARG(2);
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "%s,%s,^,%s,=",
				arg2, arg1, arg0);
		}
		}
		break;
	case MIPS_INS_NOR:
		{
		const char *arg0 = ARG(0);
		const char *arg1 = ARG(1);
		const char *arg2 = ARG(2);
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "%s,%s,|,0xffffffff,^,%s,=",
				arg2, arg1, arg0);
		}
		}
		break;
	case MIPS_INS_SLT:
	case MIPS_INS_SLTI:
		if (OPCOUNT () < 3) {
			r_strbuf_appendf (&op->esil, "%s,%s,<,t,=", ARG(1), ARG(0));
		} else {
			r_strbuf_appendf (&op->esil, "%s,%s,<,%s,=", ARG(2), ARG(1), ARG(0));
		}
		break;
	case MIPS_INS_SLTU:
	case MIPS_INS_SLTIU:
		if (OPCOUNT () < 3) {
			r_strbuf_appendf (&op->esil, "%s,0xffffffff,&,%s,0xffffffff,&,<,t,=",
				ARG (1), ARG (0));
		} else {
			r_strbuf_appendf (&op->esil, "%s,0xffffffff,&,%s,0xffffffff,&,<,%s,=",
				ARG (2), ARG (1), ARG (0));
		}
		break;
	case MIPS_INS_MUL:
		r_strbuf_appendf (&op->esil,
			"%s,%s,*,0xffffffff,&,lo,=,"
			ES_SIGN_EXT64 ("lo")
			",32,%s,%s,*,>>,0xffffffff,&,hi,=,"
			ES_SIGN_EXT64 ("hi")
			",lo,%s,=",
			ARG (1), ARG (2), ARG (1), ARG (2), REG (0));
		break;
	case MIPS_INS_MULT:
	case MIPS_INS_MULTU:
		r_strbuf_appendf (&op->esil,
			"%s,%s,*,0xffffffff,&,lo,=,"
			ES_SIGN_EXT64 ("lo")
			",32,%s,%s,*,>>,0xffffffff,&,hi,=,"
			ES_SIGN_EXT64 ("hi"),

			ARG (0), ARG (1), ARG (0), ARG (1));
		break;
	case MIPS_INS_MFLO:
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "lo,%s,=", REG (0));
		}
		break;
	case MIPS_INS_MFHI:
		PROTECT_ZERO () {
			r_strbuf_appendf (&op->esil, "hi,%s,=", REG (0));
		}
		break;
	case MIPS_INS_MTLO:
		r_strbuf_appendf (&op->esil, "%s,lo,=,"ES_SIGN_EXT64 ("lo"), REG (0));
		break;
	case MIPS_INS_MTHI:
		r_strbuf_appendf (&op->esil, "%s,hi,=,"ES_SIGN_EXT64 ("hi"), REG (0));
		break;
#if 0
	// could not test div
	case MIPS_INS_DIV:
	case MIPS_INS_DIVU:
	case MIPS_INS_DDIV:
	case MIPS_INS_DDIVU:
		PROTECT_ZERO () {
			// 32 bit needs sign extend
			r_strbuf_appendf (&op->esil, "%s,%s,/,lo,=,%s,%s,%%,hi,=", REG(1), REG(0), REG(1), REG(0));
		}
		break;
#endif
	default:
		return -1;
	}
	return 0;
}

static int analop(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *buf, int len) {
	int n, ret, opsize = -1;
	static csh hndl = 0;
	static csh *handle = &hndl;
	static int omode = -1;
	static int obits = 32;
	cs_insn* insn;
	int mode = anal->big_endian? CS_MODE_BIG_ENDIAN: CS_MODE_LITTLE_ENDIAN;

	mode |= (anal->bits==64)? CS_MODE_64: CS_MODE_32;
	if (mode != omode || anal->bits != obits) {
		cs_close (&hndl);
		hndl = 0;
		omode = mode;
		obits = anal->bits;
	}
// XXX no arch->cpu ?!?! CS_MODE_MICRO, N64
	op->delay = 0;
	op->type = R_ANAL_OP_TYPE_ILL;
	if (len<4)
		return -1;
	op->size = 4;
	if (hndl == 0) {
		ret = cs_open (CS_ARCH_MIPS, mode, &hndl);
		if (ret != CS_ERR_OK) goto fin;
		cs_option (hndl, CS_OPT_DETAIL, CS_OPT_ON);
	}
	n = cs_disasm (hndl, (ut8*)buf, len, addr, 1, &insn);
	if (n<1 || insn->size<1)
		goto beach;
	op->type = R_ANAL_OP_TYPE_NULL;
	op->delay = 0;
	op->jump = UT64_MAX;
	op->fail = UT64_MAX;
	opsize = op->size = insn->size;
	switch (insn->id) {
	case MIPS_INS_INVALID:
		op->type = R_ANAL_OP_TYPE_ILL;
		break;
	case MIPS_INS_LB:
	case MIPS_INS_LBU:
	case MIPS_INS_LBUX:
	case MIPS_INS_LW:
	case MIPS_INS_LWC1:
	case MIPS_INS_LWC2:
	case MIPS_INS_LWL:
	case MIPS_INS_LWR:
	case MIPS_INS_LWXC1:
	case MIPS_INS_LD:
	case MIPS_INS_LDC1:
	case MIPS_INS_LDC2:
	case MIPS_INS_LDL:
	case MIPS_INS_LDR:
	case MIPS_INS_LDXC1:
		op->type = R_ANAL_OP_TYPE_LOAD;
		op->refptr = 4;
		switch (OPERAND(1).type) {
		case MIPS_OP_MEM:
			if (OPERAND(1).mem.base == MIPS_REG_GP) {
				op->ptr = anal->gp + OPERAND(1).mem.disp;
				op->refptr = 4;
			}
			break;
		case MIPS_OP_IMM:
			op->ptr = OPERAND(1).imm;
			break;
		case MIPS_OP_REG:
			// wtf?
			break;
		default:
			break;
		}
		// TODO: fill
		break;
	case MIPS_INS_SW:
	case MIPS_INS_SWC1:
	case MIPS_INS_SWC2:
	case MIPS_INS_SWL:
	case MIPS_INS_SWR:
	case MIPS_INS_SWXC1:
		op->type = R_ANAL_OP_TYPE_STORE;
		break;
	case MIPS_INS_NOP:
		op->type = R_ANAL_OP_TYPE_NOP;
		break;
	case MIPS_INS_SYSCALL:
		op->type = R_ANAL_OP_TYPE_SWI;
		break;
	case MIPS_INS_BREAK:
		op->type = R_ANAL_OP_TYPE_TRAP;
		break;
	case MIPS_INS_JALR:
		op->type = R_ANAL_OP_TYPE_UCALL;
		op->delay = 1;
		break;
	case MIPS_INS_JAL:
	case MIPS_INS_JALS:
	case MIPS_INS_JALX:
	case MIPS_INS_JRADDIUSP:
	case MIPS_INS_BAL:
	// (no blezal/bgtzal or blezall/bgtzall, only blezalc/bgtzalc)
	case MIPS_INS_BLTZAL: // Branch on <0 and link
	case MIPS_INS_BGEZAL: // Branch on >=0 and link
	case MIPS_INS_BLTZALL: // "likely" versions
	case MIPS_INS_BGEZALL:
	case MIPS_INS_BLTZALC: // compact versions
	case MIPS_INS_BLEZALC:
	case MIPS_INS_BGEZALC:
	case MIPS_INS_BGTZALC:
	case MIPS_INS_JIALC:
	case MIPS_INS_JIC:
		op->type = R_ANAL_OP_TYPE_CALL;
		op->jump = IMM(0);

		switch (insn->id) {
		case MIPS_INS_JIALC:
		case MIPS_INS_JIC:
		case MIPS_INS_BLTZALC:
		case MIPS_INS_BLEZALC:
		case MIPS_INS_BGEZALC:
		case MIPS_INS_BGTZALC:
			// compact vesions (no delay)
			op->delay = 0;
			op->fail = addr+4;
			break;
		default:
			op->delay = 1;
			op->fail = addr+8;
			break;
		}
		break;
	case MIPS_INS_LUI:
	case MIPS_INS_MOVE:
		op->type = R_ANAL_OP_TYPE_MOV;
		SET_SRC_DST_2_REGS (op);
		break;
	case MIPS_INS_ADD:
	case MIPS_INS_ADDI:
	case MIPS_INS_ADDU:
	case MIPS_INS_ADDIU:
	case MIPS_INS_DADD:
	case MIPS_INS_DADDI:
	case MIPS_INS_DADDIU:
		SET_VAL (op, 2);
		SET_SRC_DST_3_REG_OR_IMM (op);
		op->type = R_ANAL_OP_TYPE_ADD;
		if (REGID(0) == MIPS_REG_SP) {
			op->stackop = R_ANAL_STACK_INC;
			op->stackptr = -IMM(2);
		}
		break;
	case MIPS_INS_SUB:
	case MIPS_INS_SUBV:
	case MIPS_INS_SUBVI:
	case MIPS_INS_DSUBU:
	case MIPS_INS_FSUB:
	case MIPS_INS_FMSUB:
	case MIPS_INS_SUBU:
	case MIPS_INS_DSUB:
	case MIPS_INS_SUBS_S:
	case MIPS_INS_SUBS_U:
	case MIPS_INS_SUBUH:
	case MIPS_INS_SUBUH_R:
		SET_VAL (op,2);
		SET_SRC_DST_3_REG_OR_IMM (op);
		op->type = R_ANAL_OP_TYPE_SUB;
		break;
	case MIPS_INS_MULV:
	case MIPS_INS_MULT:
	case MIPS_INS_MULSA:
	case MIPS_INS_FMUL:
	case MIPS_INS_MUL:
	case MIPS_INS_DMULT:
	case MIPS_INS_DMULTU:
		op->type = R_ANAL_OP_TYPE_MUL;
		break;
	case MIPS_INS_XOR:
	case MIPS_INS_XORI:
		SET_VAL (op,2);
		SET_SRC_DST_3_REG_OR_IMM (op);
		op->type = R_ANAL_OP_TYPE_XOR;
		break;
	case MIPS_INS_AND:
	case MIPS_INS_ANDI:
		SET_VAL (op,2);
		SET_SRC_DST_3_REG_OR_IMM (op);
		op->type = R_ANAL_OP_TYPE_AND;
		if (REGID(0) == MIPS_REG_SP) {
			op->stackop = R_ANAL_STACK_ALIGN;
		}
		break;
	case MIPS_INS_NOT:
		op->type = R_ANAL_OP_TYPE_NOT;
		break;
	case MIPS_INS_OR:
	case MIPS_INS_ORI:
		SET_VAL (op,2);
		SET_SRC_DST_3_REG_OR_IMM (op);
		op->type = R_ANAL_OP_TYPE_OR;
		break;
	case MIPS_INS_DIV:
	case MIPS_INS_DIVU:
	case MIPS_INS_DDIV:
	case MIPS_INS_DDIVU:
	case MIPS_INS_FDIV:
	case MIPS_INS_DIV_S:
	case MIPS_INS_DIV_U:
		SET_SRC_DST_3_REGS (op);
		op->type = R_ANAL_OP_TYPE_DIV;
		break;
	case MIPS_INS_CMPGDU:
	case MIPS_INS_CMPGU:
	case MIPS_INS_CMPU:
	case MIPS_INS_CMPI:
		op->type = R_ANAL_OP_TYPE_CMP;
		break;
	case MIPS_INS_J:
	case MIPS_INS_B:
	case MIPS_INS_BZ:
	case MIPS_INS_BEQ:
	case MIPS_INS_BNZ:
	case MIPS_INS_BNE:
	case MIPS_INS_BEQZ:
	case MIPS_INS_BNEG:
	case MIPS_INS_BNEGI:
	case MIPS_INS_BNEZ:
	case MIPS_INS_BTEQZ:
	case MIPS_INS_BTNEZ:
	case MIPS_INS_BLTZ:
	case MIPS_INS_BLTZL:
	case MIPS_INS_BLEZ:
	case MIPS_INS_BLEZL:
	case MIPS_INS_BGEZ:
	case MIPS_INS_BGEZL:
	case MIPS_INS_BGTZ:
	case MIPS_INS_BGTZL:
	case MIPS_INS_BLEZC:
	case MIPS_INS_BGEZC:
	case MIPS_INS_BLTZC:
	case MIPS_INS_BGTZC:
		if (insn->id == MIPS_INS_J || insn->id == MIPS_INS_B ) {
			op->type = R_ANAL_OP_TYPE_JMP;
		} else {
			op->type = R_ANAL_OP_TYPE_CJMP;
		}

		if (OPERAND(0).type == MIPS_OP_IMM) {
			op->jump = IMM(0);
		} else if (OPERAND(1).type == MIPS_OP_IMM) {
			op->jump = IMM(1);
		} else if (OPERAND(2).type == MIPS_OP_IMM) {
			op->jump = IMM(2);
		}

		switch (insn->id) {
		case MIPS_INS_BLEZC:
		case MIPS_INS_BGEZC:
		case MIPS_INS_BLTZC:
		case MIPS_INS_BGTZC:
			// compact vesions (no delay)
			op->delay = 0;
			op->fail = addr+4;
			break;
		default:
			op->delay = 1;
			op->fail = addr+8;
			break;
		}

		break;
	case MIPS_INS_JR:
	case MIPS_INS_JRC:
		op->type = R_ANAL_OP_TYPE_JMP;
		op->delay = 1;
        // register is $ra, so jmp is a return
        if (insn->detail->mips.operands[0].reg == MIPS_REG_RA) {
            op->type = R_ANAL_OP_TYPE_RET;
        }
		break;
	case MIPS_INS_SLTI:
	case MIPS_INS_SLTIU:
		SET_SRC_DST_3_IMM (op);
		SET_VAL (op,2);
		break;

	case MIPS_INS_SHRAV:
	case MIPS_INS_SHRAV_R:
	case MIPS_INS_SHRA:
	case MIPS_INS_SHRA_R:
	case MIPS_INS_SRA:
		op->type = R_ANAL_OP_TYPE_SAR;
		SET_SRC_DST_3_REG_OR_IMM (op);
		SET_VAL (op,2);
		break;
	case MIPS_INS_SHRL:
	case MIPS_INS_SRLV:
	case MIPS_INS_SRL:
		op->type = R_ANAL_OP_TYPE_SHR;
		SET_SRC_DST_3_REG_OR_IMM (op);
		SET_VAL (op,2);
		break;
	case MIPS_INS_SLLV:
	case MIPS_INS_SLL:
		op->type = R_ANAL_OP_TYPE_SHL;
		SET_SRC_DST_3_REG_OR_IMM (op);
		SET_VAL (op,2);
		break;
	}
	beach:
	if (anal->decode) {
		if (analop_esil (anal, op, addr, buf, len, &hndl, insn) != 0)
			r_strbuf_fini (&op->esil);
	}
	cs_free (insn, n);
	//cs_close (&handle);
	fin:
	return opsize;
}

static char *get_reg_profile(RAnal *anal) {
	// XXX : 64bit profile
	const char *p =
		"=PC    pc\n"
		"=SP    sp\n"
		"=A0    a0\n"
		"=A1    a1\n"
		"=A2    a2\n"
		"=A3    a3\n"
		"=R0    v0\n"
		"=R1    v1\n"
		"gpr	zero	.32	0	0\n"
		"gpr	at	.32	4	0\n"
		"gpr	v0	.32	8	0\n"
		"gpr	v1	.32	12	0\n"
		"gpr	a0	.32	16	0\n"
		"gpr	a1	.32	20	0\n"
		"gpr	a2	.32	24	0\n"
		"gpr	a3	.32	28	0\n"
		"gpr	t0	.32	32	0\n"
		"gpr	t1	.32	36	0\n"
		"gpr	t2 	.32	40	0\n"
		"gpr	t3 	.32	44	0\n"
		"gpr	t4 	.32	48	0\n"
		"gpr	t5 	.32	52	0\n"
		"gpr	t6 	.32	56	0\n"
		"gpr	t7 	.32	60	0\n"
		"gpr	s0	.32	64	0\n"
		"gpr	s1	.32	68	0\n"
		"gpr	s2	.32	72	0\n"
		"gpr	s3	.32	76	0\n"
		"gpr	s4 	.32	80	0\n"
		"gpr	s5 	.32	84	0\n"
		"gpr	s6 	.32	88	0\n"
		"gpr	s7 	.32	92	0\n"
		"gpr	t8 	.32	96	0\n"
		"gpr	t9 	.32	100	0\n"
		"gpr	k0 	.32	104	0\n"
		"gpr	k1 	.32	108	0\n"
		"gpr	gp 	.32	112	0\n"
		"gpr	sp	.32	116	0\n"
		"gpr	fp	.32	120	0\n"
		"gpr	ra	.32	124	0\n"
		"gpr	pc	.32	128	0\n"
		"gpr	hi	.64	132	0\n"
		"gpr	lo	.64	140	0\n"
		"gpr	t	.32	148	0\n";
	return strdup (p);
}

static int archinfo(RAnal *anal, int q) {
	return 4;
}

RAnalPlugin r_anal_plugin_mips_cs = {
	.name = "mips",
	.desc = "Capstone MIPS analyzer",
	.license = "BSD",
	.esil = true,
	.arch = "mips",
	.get_reg_profile = get_reg_profile,
	.archinfo = archinfo,
	.bits = 16|32|64,
	.op = &analop,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_ANAL,
	.data = &r_anal_plugin_mips_cs,
	.version = R2_VERSION
};
#endif
