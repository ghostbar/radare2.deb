/* radare - LGPL - Copyright 2009-2016 - pancake, defragger */

#include <r_asm.h>
#include <r_debug.h>
#include <libgdbr.h>

typedef struct {
	libgdbr_t desc;
} RIOGdb;

#define UNKNOWN -1
#define UNSUPPORTED 0
#define SUPPORTED 1

static libgdbr_t *desc = NULL;
static ut8* reg_buf = NULL;
static int buf_size = 0;
static int support_sw_bp = UNKNOWN;
static int support_hw_bp = UNKNOWN;

static int r_debug_gdb_attach(RDebug *dbg, int pid);
static void check_connection (RDebug *dbg) {
	if (!desc) {
		r_debug_gdb_attach (dbg, -1);
	}
}

static int r_debug_gdb_step(RDebug *dbg) {
	check_connection (dbg);
	gdbr_step (desc, -1); // TODO handle thread specific step?
	return true;
}

static int r_debug_gdb_reg_read(RDebug *dbg, int type, ut8 *buf, int size) {
	int copy_size;
	int buflen = 0;
	check_connection (dbg);
	gdbr_read_registers (desc);
	if (!desc) {
		return -1;
	}
	// read the len of the current area
	free (r_reg_get_bytes (dbg->reg, type, &buflen));
	if (size < desc->data_len) {
		eprintf ("r_debug_gdb_reg_read: small buffer %d vs %d\n",
			(int)size, (int)desc->data_len);
		//	return -1;
	}
	copy_size = R_MIN (desc->data_len, size);
	buflen = R_MAX (desc->data_len, buflen);
	if (reg_buf) {
		// if (buf_size < copy_size) { //desc->data_len) {
		if (buflen > buf_size) { //copy_size) {
			ut8* new_buf = realloc (reg_buf, buflen);
			if (!new_buf) {
				return -1;
			}
			reg_buf = new_buf;
			buf_size = buflen;
		}
	} else {
		reg_buf = calloc (buflen, 1);
		if (!reg_buf) {
			return -1;
		}
		buf_size = buflen;
	}
	memset ((void*)(volatile void*)buf, 0, size);
	memcpy ((void*)(volatile void*)buf, desc->data, R_MIN (copy_size, size));
	memset ((void*)(volatile void*)reg_buf, 0, buflen);
	memcpy ((void*)(volatile void*)reg_buf, desc->data, copy_size);
#if 0
	int i;
	//for(i=0;i<168;i++) {
	for(i=0;i<copy_size;i++) {
		if (!(i%16)) printf ("\n0x%08x  ", i);
		printf ("%02x ", buf[i]); //(ut8)desc->data[i]);
	}
	printf("\n");
#endif
	return desc->data_len;
}

static RList *r_debug_gdb_map_get(RDebug* dbg) { //TODO
	check_connection (dbg);
	//TODO
	return NULL;
}

static int r_debug_gdb_reg_write(RDebug *dbg, int type, const ut8 *buf, int size) {
	check_connection (dbg);
	if (!reg_buf) {
		// we cannot write registers before we once read them
		return -1;
	}
	int buflen = 0;
	int bits = dbg->anal->bits;
	const char *pcname = r_reg_get_name (dbg->anal->reg, R_REG_NAME_PC);
	RRegItem *reg = r_reg_get (dbg->anal->reg, pcname, 0);
	if (reg) {
		if (dbg->anal->bits != reg->size)
			bits = reg->size;
	}
	free (r_reg_get_bytes (dbg->reg, type, &buflen));
	// some implementations of the gdb protocol are acting weird.
	// so winedbg is not able to write registers through the <G> packet
	// and also it does not return the whole gdb register profile after
	// calling <g>
	// so this workaround resizes the small register profile buffer
	// to the whole set and fills the rest with 0
	if (buf_size < buflen) {
		ut8* new_buf = realloc (reg_buf, buflen * sizeof (ut8));
		if (!new_buf) {
			return -1;
		}
		reg_buf = new_buf;
		memset (new_buf + buf_size, 0, buflen - buf_size);
	}

	RRegItem* current = NULL;
	for (;;) {
		current = r_reg_next_diff (dbg->reg, type, reg_buf, buflen, current, bits);
		if (!current) break;
		ut64 val = r_reg_get_value (dbg->reg, current);
		int bytes = bits / 8;
		gdbr_write_reg (desc, current->name, (char*)&val, bytes);
	}
	return true;
}

static int r_debug_gdb_continue(RDebug *dbg, int pid, int tid, int sig) {
	check_connection (dbg);
	gdbr_continue (desc, -1);
	return true;
}

static int r_debug_gdb_wait(RDebug *dbg, int pid) {
	check_connection (dbg);
	/* do nothing */
	return true;
}

static int r_debug_gdb_attach(RDebug *dbg, int pid) {
	RIODesc *d = dbg->iob.io->desc;
	// TODO: the core must update the dbg.swstep config var when this var is changed
	dbg->swstep = false;
	//eprintf ("XWJSTEP TOFALSE\n");
	if (d && d->plugin && d->plugin->name && d->data) {
		if (!strcmp ("gdb", d->plugin->name)) {
			RIOGdb *g = d->data;
			support_sw_bp = UNKNOWN;
			support_hw_bp = UNKNOWN;
			int arch = r_sys_arch_id (dbg->arch);
			int bits = dbg->anal->bits;
			if (( desc = &g->desc ))
			switch (arch) {
			case R_SYS_ARCH_X86:
				if (bits == 16 || bits == 32) {
					gdbr_set_architecture (&g->desc, X86_32);
				} else if (bits == 64) {
					gdbr_set_architecture (&g->desc, X86_64);
				} else {
					eprintf ("Not supported register %s %d profile\n", dbg->arch, bits);
					return false;
				}
				break;
			case R_SYS_ARCH_SH:
				// TODO
				break;
			case R_SYS_ARCH_ARM:
				if (bits == 16 || bits == 32) {
					gdbr_set_architecture (&g->desc, ARM_32);
				} else if (bits == 64) {
					gdbr_set_architecture (&g->desc, ARM_64);
				} else {
					eprintf ("Not supported register %s %d profile\n", dbg->arch, bits);
					return false;
				}
				break;
			case R_SYS_ARCH_LM32:
				if (bits == 32) {
					gdbr_set_architecture(&g->desc, LM32);
				} else {
					eprintf ("Not supported register %s %d profile\n", dbg->arch, bits);
					return false;
				}
				break;
			case R_SYS_ARCH_MIPS:
				if (bits == 32 || bits == 64) {
					gdbr_set_architecture (&g->desc, MIPS);
				} else {
					eprintf ("Not supported register %s %d profile\n", dbg->arch, bits);
					return false;
				}
				break;
			case R_SYS_ARCH_AVR:
				if (bits == 16) {
					gdbr_set_architecture (&g->desc, AVR);
				} else {
					gdbr_set_architecture (&g->desc, AVR);
					//eprintf ("Not supported register profile\n");
					//return false;
				}
				break;
			}
		} else {
			eprintf ("ERROR: Underlaying IO descriptor is not a GDB one..\n");
		}
	}
	return true;
}

static int r_debug_gdb_detach(RDebug *dbg, int pid) {
	gdbr_disconnect (desc);
	free (reg_buf);
	return true;
}

static const char *r_debug_gdb_reg_profile(RDebug *dbg) {
	int arch = r_sys_arch_id (dbg->arch);
	int bits = dbg->anal->bits;
	check_connection (dbg);
	switch (arch) {
	case R_SYS_ARCH_X86:
		if (bits == 16 || bits == 32) {
			return strdup (
				"=PC	eip\n"
				"=SP	esp\n"
				"=BP	ebp\n"
				"=A0	eax\n"
				"=A1	ebx\n"
				"=A2	ecx\n"
				"=A3	edi\n"
				"=SN	oeax\n"
				"gpr	eax	.32	0	0\n"
				"gpr	ecx	.32	4	0\n"
				"gpr	edx	.32	8	0\n"
				"gpr	ebx	.32	12	0\n"
				"gpr	esp	.32	16	0\n"
				"gpr	ebp	.32	20	0\n"
				"gpr	esi	.32	24	0\n"
				"gpr	edi	.32	28	0\n"
				"gpr	eip	.32	32	0\n"
				"gpr	eflags	.32	36	0\n"
				"seg	cs	.32	40	0\n"
				"seg	ss	.32	44	0\n"
				"seg	ds	.32	48	0\n"
				"seg	es	.32	52	0\n"
				"seg	fs	.32	56	0\n"
				"seg	gs	.32	60	0\n"
				"gpr	st0	.80	64	0\n"
				"gpr	st1	.80	74	0\n"
				"gpr	st2	.80	84	0\n"
				"gpr	st3	.80	94	0\n"
				"gpr	st4	.80	104	0\n"
				"gpr	st5	.80	114	0\n"
				"gpr	st6	.80	124	0\n"
				"gpr	st7	.80	134	0\n"
				"gpr	fctrl	.32	144	0\n"
				"gpr	fstat	.32	148	0\n"
				"gpr	ftag	.32	152	0\n"
				"gpr	fiseg	.32	156	0\n"
				"gpr	fioff	.32	160	0\n"
				"gpr	foseg	.32	164	0\n"
				"gpr	fooff	.32	168	0\n"
				"gpr	fop	.32	172	0\n"
			/* Commented until the long registers will be implemented
				"gpr	xmm0	.128	176	0\n"
				"gpr	xmm1	.128	192	0\n"
				"gpr	xmm2	.128	208	0\n"
				"gpr	xmm3	.128	224	0\n"
				"gpr	xmm4	.128	240	0\n"
				"gpr	xmm5	.128	256	0\n"
				"gpr	xmm6	.128	272	0\n"
				"gpr	xmm7	.128	288	0\n"
				"gpr	mxcsr	.32	304	0\n"
			*/
				);
		} else if (dbg->anal->bits == 64) {
			return strdup (
				"=PC	rip\n"
				"=SP	rsp\n"
				"=BP	rbp\n"
				"=A0	rax\n"
				"=A1	rbx\n"
				"=A2	rcx\n"
				"=A3	rdx\n"
				"=SN	orax\n"
				"gpr	fake	.64	795	0\n"
				"gpr	rax	.64	0	0\n"
				"gpr	rbx	.64	8	0\n"
				"gpr	rcx	.64	16	0\n"
				"gpr	rdx	.64	24	0\n"
				"gpr	rsi	.64	32	0\n"
				"gpr	rdi	.64	40	0\n"
				"gpr	rbp	.64	48	0\n"
				"gpr	rsp	.64	56	0\n"
				"gpr	r8	.64	64	0\n"
				"gpr	r9	.64	72	0\n"
				"gpr	r10	.64	80	0\n"
				"gpr	r11	.64	88	0\n"
				"gpr	r12	.64	96	0\n"
				"gpr	r13	.64	104	0\n"
				"gpr	r14	.64	112	0\n"
				"gpr	r15	.64	120	0\n"
				"gpr	rip	.64	128	0\n"
				"gpr	eflags	.32	136	0\n"
				"seg	cs	.32	140	0\n"
				"seg	ss	.32	144	0\n"
				"seg	ds	.32	148	0\n"
				"seg	es	.32	152	0\n"
				"seg	fs	.32	156	0\n"
				"seg	gs	.32	160	0\n"
				"gpr	st0	.80	164	0\n"
				"gpr	st1	.80	174	0\n"
				"gpr	st2	.80	184	0\n"
				"gpr	st3	.80	194	0\n"
				"gpr	st4	.80	204	0\n"
				"gpr	st5	.80	214	0\n"
				"gpr	st6	.80	224	0\n"
				"gpr	st7	.80	234	0\n"
				"gpr	fctrl	.32	244	0\n"
				"gpr	fstat	.32	248	0\n"
				"gpr	ftag	.32	252	0\n"
				"gpr	fiseg	.32	256	0\n"
				"gpr	fioff	.32	260	0\n"
				"gpr	foseg	.32	264	0\n"
				"gpr	fooff	.32	268	0\n"
				"gpr	fop	.32	272	0\n"
			/* Commented until the long registers will be implemented
				"gpr	xmm0	.128	276	0\n"
				"gpr	xmm1	.128	292	0\n"
				"gpr	xmm2	.128	308	0\n"
				"gpr	xmm3	.128	324	0\n"
				"gpr	xmm4	.128	340	0\n"
				"gpr	xmm5	.128	356	0\n"
				"gpr	xmm6	.128	372	0\n"
				"gpr	xmm7	.128	388	0\n"
				"gpr	xmm8	.128	404	0\n"
				"gpr	xmm9	.128	420	0\n"
				"gpr	xmm10	.128	436	0\n"
				"gpr	xmm11	.128	452	0\n"
				"gpr	xmm12	.128	468	0\n"
				"gpr	xmm13	.128	484	0\n"
				"gpr	xmm14	.128	500	0\n"
				"gpr	xmm15	.128	516	0\n"
				"gpr	mxcsr	.32	532	0\n"
			*/
			);
		} else {
			return strdup (
			"=PC	eip\n"
			"=SP	esp\n"
			"=BP	ebp\n"
			"=A0	eax\n"
			"=A1	ebx\n"
			"=A2	ecx\n"
			"=A3	edi\n"
			"gpr	eax	.32	0	0\n"
			"gpr	ecx	.32	4	0\n"
			"gpr	edx	.32	8	0\n"
			"gpr	ebx	.32	12	0\n"
			"gpr	esp	.32	16	0\n"
			"gpr	ebp	.32	20	0\n"
			"gpr	esi	.32	24	0\n"
			"gpr	edi	.32	28	0\n"
			"gpr	eip	.32	32	0\n"
			"gpr	eflags	.32	36	0\n"
			"seg	cs	.32	40	0\n"
			"seg	ss	.32	44	0\n"
			"seg	ds	.32	48	0\n"
			"seg	es	.32	52	0\n"
			"seg	fs	.32	56	0\n"
			"seg	gs	.32	60	0\n"
			);
		}
		break;
	case R_SYS_ARCH_ARM:
		if (bits == 64) {
			return strdup (
			"=PC	pc\n"
			"=SP	sp\n"
			"=A0	r0\n"
			"=A1	r1\n"
			"=A2	r2\n"
			"=A3	r3\n"
			"gpr	x0	.64	0	0\n"
			"gpr	x1	.64	8	0\n"
			"gpr	x2	.64	16	0\n"
			"gpr	x3	.64	24	0\n"
			"gpr	x4	.64	32	0\n"
			"gpr	x5	.64	40	0\n"
			"gpr	x6	.64	48	0\n"
			"gpr	x7	.64	56	0\n"
			"gpr	x8	.64	64	0\n"
			"gpr	x9	.64	72	0\n"
			"gpr	x10	.64	80	0\n"
			"gpr	x11	.64	88	0\n"
			"gpr	x12	.64	96	0\n"
			"gpr	x13	.64	104	0\n"
			"gpr	x14	.64	112	0\n"
			"gpr	x15	.64	120	0\n"
			"gpr	x16	.64	128	0\n"
			"gpr	x17	.64	136	0\n"
			"gpr	x18	.64	144	0\n"
			"gpr	x19	.64	152	0\n"
			"gpr	x20	.64	160	0\n"
			"gpr	x21	.64	168	0\n"
			"gpr	x22	.64	176	0\n"
			"gpr	x23	.64	184	0\n"
			"gpr	x24	.64	192	0\n"
			"gpr	x25	.64	200	0\n"
			"gpr	x26	.64	208	0\n"
			"gpr	x27	.64	216	0\n"
			"gpr	x28	.64	224	0\n"
			"gpr	x29	.64	232	0\n"
			"gpr	x30	.64	240	0\n"
			"gpr	sp	.64	248	0\n"
			"gpr	pc	.64	256	0\n"
			"gpr	pstate	.64	264	0\n"
			);
		} else {
			return strdup (
#if 0
			"=PC	r15\n"
			"=SP	r14\n" // XXX
			"=A0	r0\n"
			"=A1	r1\n"
			"=A2	r2\n"
			"=A3	r3\n"
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
			"gpr	f0	.96	64	0\n"
			"gpr	f1	.96	76	0\n"
			"gpr	f2	.96	88	0\n"
			"gpr	f3	.96	100	0\n"
			"gpr	f4	.96	112	0\n"
			"gpr	f5	.96	124	0\n"
			"gpr	f6	.96	136	0\n"
			"gpr	f7	.96	148	0\n"
			"gpr	fps	.96	160	0\n"
			"gpr	cpsr	.32	172	0\n"
#else
			"=PC	r15\n"
			"=SP	r14\n" // XXX
			"=A0	r0\n"
			"=A1	r1\n"
			"=A2	r2\n"
			"=A3	r3\n"
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
			"gpr	sp	.32	52	0\n" // r13
			"gpr	lr	.32	56	0\n" // r14
			"gpr	pc	.32	60	0\n" // r15
			"gpr	r13	.32	52	0\n"
			"gpr	r14	.32	56	0\n"
			"gpr	r15	.32	60	0\n"
			"gpr	cpsr	.96	64	0\n"
			"mmx	d0	.64	68	0\n" // neon
			"mmx	d1	.64	76	0\n" // neon
			"mmx	d2	.64	84	0\n" // neon
			"mmx	d3	.64	92	0\n" // neon
			"mmx	d4	.64	100	0\n" // neon
			"mmx	d5	.64	108	0\n" // neon
			"mmx	d6	.64	116	0\n" // neon
			"mmx	d7	.64	124	0\n" // neon
			"mmx	d8	.64	132	0\n" // neon
			"mmx	d9	.64	140	0\n" // neon
			"mmx	d10	.64	148	0\n" // neon
			"mmx	d11	.64	156	0\n" // neon
			"mmx	d12	.64	164	0\n" // neon
			"mmx	d13	.64	172	0\n" // neon
			"mmx	d14	.64	180	0\n" // neon
			"mmx	d15	.64	188	0\n" // neon
			"mmx	d16	.64	196	0\n" // neon
			"mmx	d17	.64	204	0\n" // neon
			"mmx	d18	.64	212	0\n" // neon
			"mmx	d19	.64	220	0\n" // neon
			"mmx	d20	.64	228	0\n" // neon
			"mmx	d21	.64	236	0\n" // neon
			"mmx	d22	.64	244	0\n" // neon
			"mmx	d23	.64	252	0\n" // neon
			"mmx	d24	.64	260	0\n" // neon
			"mmx	d25	.64	268	0\n" // neon
			"mmx	d26	.64	276	0\n" // neon
			"mmx	d27	.64	284	0\n" // neon
			"mmx	d28	.64	292	0\n" // neon
			"mmx	d29	.64	300	0\n" // neon
			"mmx	d30	.64	308	0\n" // neon
			"mmx	d31	.64	316	0\n" // neon
			"mmx	fpscr	.32	324	0\n" // neon
#endif
			);
		}
	case R_SYS_ARCH_SH:
		return strdup (
			"=PC    pc\n"
			"=SP    r15\n"
			"=BP    r14\n"
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
			"gpr	pc	.32	64	0\n"
			"gpr	pr	.32	68	0\n"
			"gpr	sr	.32	72	0\n"
			"gpr	gbr	.32	76	0\n"
			"gpr	mach	.32	80	0\n"
			"gpr	macl	.32	84	0\n"
		);
	case R_SYS_ARCH_LM32:
		return strdup (
			"=PC    PC\n"
			"=SP    sp\n"
			"=BP    gp\n"
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
			"gpr	r17	.32	68	0\n"
			"gpr	r18	.32	72	0\n"
			"gpr	r19	.32	76	0\n"
			"gpr	r20	.32	80	0\n"
			"gpr	r21	.32	84	0\n"
			"gpr	r22	.32	88	0\n"
			"gpr	r23	.32	92	0\n"
			"gpr	r24	.32	96	0\n"
			"gpr	r25	.32	100	0\n"
			"gpr	gp	.32	104	0\n"
			"gpr	fp	.32	108	0\n"
			"gpr	sp	.32	112	0\n"
			"gpr	ra	.32	116	0\n"
			"gpr	ea	.32	120	0\n"
			"gpr	ba	.32	124	0\n"
			"gpr	PC	.32	128	0\n"
			"gpr	EID	.32	132	0\n"
			"gpr	EBA	.32	136	0\n"
			"gpr	DEBA	.32	140	0\n"
			"gpr	IE	.32	144	0\n"
			"gpr	IM	.32	148	0\n"
			"gpr	IP	.32	152	0\n"
		);
	case R_SYS_ARCH_MIPS:
		return strdup (
			"=PC    pc\n"
			"=SP    sp\n"
			"=BP    gp\n"
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
			"gpr	t2	.32	40	0\n"
			"gpr	t3	.32	44	0\n"
			"gpr	t4	.32	48	0\n"
			"gpr	t5	.32	52	0\n"
			"gpr	t6	.32	56	0\n"
			"gpr	t7	.32	60	0\n"
			"gpr	s0	.32	64	0\n"
			"gpr	s1	.32	68	0\n"
			"gpr	s2	.32	72	0\n"
			"gpr	s3	.32	76	0\n"
			"gpr	s4	.32	80	0\n"
			"gpr	s5	.32	84	0\n"
			"gpr	s6	.32	88	0\n"
			"gpr	s7	.32	92	0\n"
			"gpr	t8	.32	96	0\n"
			"gpr	t9	.32	100	0\n"
			"gpr	k0	.32	104	0\n"
			"gpr	k1	.32	108	0\n"
			"gpr	gp	.32	112	0\n"
			"gpr	sp	.32	116	0\n"
			"gpr	s8	.32	120	0\n"
			"gpr	ra	.32	124	0\n"
			"gpr	sr	.32	128	0\n"
			"gpr	lo	.32	132	0\n"
			"gpr	hi	.32	134	0\n"
			"gpr	bad	.32	140	0\n"
			"gpr	cause	.32	144	0\n"
			"gpr	pc	.32	148	0\n"
			"gpr	f0	.32	152	0\n"
			"gpr	f1	.32	156	0\n"
			"gpr	f2	.32	160	0\n"
			"gpr	f3	.32	164	0\n"
			"gpr	f4	.32	168	0\n"
			"gpr	f5	.32	172	0\n"
			"gpr	f6	.32	176	0\n"
			"gpr	f7	.32	180	0\n"
			"gpr	f8	.32	184	0\n"
			"gpr	f9	.32	188	0\n"
			"gpr	f10	.32	192	0\n"
			"gpr	f11	.32	196	0\n"
			"gpr	f12	.32	200	0\n"
			"gpr	f13	.32	204	0\n"
			"gpr	f14	.32	208	0\n"
			"gpr	f15	.32	212	0\n"
			"gpr	f16	.32	216	0\n"
			"gpr	f17	.32	220	0\n"
			"gpr	f18	.32	224	0\n"
			"gpr	f19	.32	228	0\n"
			"gpr	f20	.32	232	0\n"
			"gpr	f21	.32	236	0\n"
			"gpr	f22	.32	240	0\n"
			"gpr	f23	.32	244	0\n"
			"gpr	f24	.32	248	0\n"
			"gpr	f25	.32	252	0\n"
			"gpr	f26	.32	256	0\n"
			"gpr	f27	.32	260	0\n"
			"gpr	f28	.32	264	0\n"
			"gpr	f29	.32	268	0\n"
			"gpr	f30	.32	272	0\n"
			"gpr	f31	.32	276	0\n"
			"gpr	fsr	.32	280	0\n"
			"gpr	fir	.32	284	0\n"
			"gpr	unknw	.32	288	0\n" //Not documented what this part of the register packet is
		);
	case R_SYS_ARCH_AVR:
		return strdup (
			"=PC    pc\n"
			"=SP    sp\n"
			"gpr	r0	.8	0	0\n"
			"gpr	r1	.8	1	0\n"
			"gpr	r2	.8	2	0\n"
			"gpr	r3	.8	3	0\n"
			"gpr	r4	.8	4	0\n"
			"gpr	r5	.8	5	0\n"
			"gpr	r6	.8	6	0\n"
			"gpr	r7	.8	7	0\n"
			"gpr	r8	.8	8	0\n"
			"gpr	r9	.8	9	0\n"
			"gpr	r10	.8	10	0\n"
			"gpr	r11	.8	11	0\n"
			"gpr	r12	.8	12	0\n"
			"gpr	r13	.8	13	0\n"
			"gpr	r14	.8	14	0\n"
			"gpr	r15	.8	15	0\n"
			"gpr	r16	.8	16	0\n"
			"gpr	r17	.8	17	0\n"
			"gpr	r18	.8	18	0\n"
			"gpr	r19	.8	19	0\n"
			"gpr	r20	.8	20	0\n"
			"gpr	r21	.8	21	0\n"
			"gpr	r22	.8	22	0\n"
			"gpr	r23	.8	23	0\n"
			"gpr	r24	.8	24	0\n"
			"gpr	r25	.8	25	0\n"
			"gpr	r26	.8	26	0\n"
			"gpr	r27	.8	27	0\n"
			"gpr	r28	.8	28	0\n"
			"gpr	r29	.8	29	0\n"
			"gpr	r30	.8	30	0\n"
			"gpr	r31	.8	31	0\n"
			"gpr	sreg	.8	32	0\n"
			"gpr	sp	.16	33	0\n"
			"gpr	pc2	.32	34	0\n"
			"gpr	pc	.32	35	0\n"
	/*		"gpr	pc	.32	39	0\n" */
	);

	}
	return NULL;
}

static int r_debug_gdb_breakpoint (RBreakpointItem *bp, int set, void *user) {
	int ret;
	if (!bp) return false;
	// TODO handle rwx and conditions
	if (set)
		ret = bp->hw?
			gdbr_set_hwbp (desc, bp->addr, ""):
			gdbr_set_bp (desc, bp->addr, "");
	else
		ret = bp->hw?
			gdbr_remove_hwbp (desc, bp->addr):
			gdbr_remove_bp (desc, bp->addr);
	return !ret;
}

struct r_debug_plugin_t r_debug_plugin_gdb = {
	.name = "gdb",
	/* TODO: Add support for more architectures here */
	.license = "LGPL3",
	.arch = "x86,arm,sh,mips,avr,lm32",
	.bits = R_SYS_BITS_16 | R_SYS_BITS_32 | R_SYS_BITS_64,
	.step = r_debug_gdb_step,
	.cont = r_debug_gdb_continue,
	.attach = &r_debug_gdb_attach,
	.detach = &r_debug_gdb_detach,
	.canstep = 1,
	.wait = &r_debug_gdb_wait,
	.map_get = r_debug_gdb_map_get,
	.breakpoint = &r_debug_gdb_breakpoint,
	.reg_read = &r_debug_gdb_reg_read,
	.reg_write = &r_debug_gdb_reg_write,
	.reg_profile = (void *)r_debug_gdb_reg_profile,
	//.bp_write = &r_debug_gdb_bp_write,
	//.bp_read = &r_debug_gdb_bp_read,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_DBG,
	.data = &r_debug_plugin_gdb,
	.version = R2_VERSION
};
#endif
