/* radare - LGPL - Copyright 2013-2015 pancake */
// r2 -Desil ls

#include <r_asm.h>
#include <r_debug.h>

#if 0
static int is_io_esil(RDebug *dbg) {
	RIODesc *d = dbg->iob.io->desc;
	if (d && d->plugin && d->plugin->name)
		if (!strcmp ("esil", d->plugin->name))
			return true;
	return false;
}
#endif

static int __esil_step_over(RDebug *dbg) {
	eprintf ("TODO: ESIL STEP OVER\n");
	return true;
}

static int __esil_step(RDebug *dbg) {
	int oplen;
	ut8 buf[64];
	ut64 pc = 0LL; // getreg("pc")
	RAnalOp op;

	pc = r_debug_reg_get (dbg, "pc");
/// XXX. hack to trick vaddr issue
//pc = 0x100001478;
memset (buf, 0, sizeof (buf));
	dbg->iob.read_at (dbg->iob.io, pc, buf, 64);
eprintf ("READ 0x%08"PFMT64x" %02x %02x %02x\n", pc, buf[0], buf[1], buf[2]);
	oplen = r_anal_op (dbg->anal, &op, pc, buf, sizeof (buf));
	if (oplen>0) {
		if (*R_STRBUF_SAFEGET (&op.esil)) {
			eprintf ("ESIL: %s\n", R_STRBUF_SAFEGET (&op.esil));
		}
	}
	eprintf ("TODO: ESIL STEP\n");
	return true;
}

static int __esil_init(RDebug *dbg) {
	dbg->tid = dbg->pid = 1;
	eprintf ("TODO: ESIL INIT\n");
	return true;
}

static int __esil_continue(RDebug *dbg, int pid, int tid, int sig) {
	eprintf ("ESIL continue\n");
	return true;
}

static int __esil_continue_syscall(RDebug *dbg, int pid, int num) {
	eprintf ("ESIL continue until syscall\n");
	return true;
}

static int __esil_wait(RDebug *dbg, int pid) {
	/* do nothing */
	return true;
}

static int __esil_attach(RDebug *dbg, int pid) {
	eprintf ("OK attach\n");
	return true;
#if 0
	if (!is_io_esil (dbg))
		return false;
#endif
#if 0
	RIOBdescbg *o;
	o = dbg->iob.io->desc->data;
eprintf ("base = %llx\n", o->bfvm->base);
eprintf ("screen = %llx\n", o->bfvm->screen);
eprintf ("input = %llx\n", o->bfvm->input);
#endif
	return true;
}

static int __esil_detach(int pid) {
	// reset vm?
	return true;
}

static char *__esil_reg_profile(RDebug *dbg) {
	eprintf ("TODO: esil %s\n", dbg->arch);
	if (!strcmp (dbg->arch, "bf")) {
		return strdup (
			"=PC	pc\n"
			"=SP	esp\n"
			"=BP	ptr\n"
			"gpr	rax	.32	0	0\n"
			"gpr	pc	.32	0	0\n"
			"gpr	ptr	.32	4	0\n"
			"gpr	esp	.32	8	0\n"
			"gpr	scr	.32	12	0\n"
			"gpr	scri	.32	16	0\n"
			"gpr	inp	.32	20	0\n"
			"gpr	inpi	.32	24	0\n"
			"gpr	mem	.32	28	0\n"
			"gpr	memi	.32	32	0\n"
		      );
	} else if (!strcmp (dbg->arch, "x86")) {
		eprintf ("[DEBUGESIL] Missing regprofile for x86\n");
	}
	return NULL;
}

static int __esil_breakpoint (RBreakpointItem *bp, int set, void *user) {
	//r_io_system (dbg->iob.io, "db");
	return false;
}

static int __esil_kill(RDebug *dbg, int pid, int tid, int sig) {
	// TODO: ESIL reset
	return true;
}

static int __esil_stop(RDebug *dbg) {
	eprintf ("ESIL: stop\n");
	return true;
}

RDebugPlugin r_debug_plugin_esil = {
	.name = "esil",
	.keepio = 1,
	.license = "LGPL3",
	.arch = "any", // TODO: exception!
	.bits = R_SYS_BITS_32 | R_SYS_BITS_64,
	.init = __esil_init,
	.step = __esil_step,
	.step_over = __esil_step_over,
	.cont = __esil_continue,
	.contsc = __esil_continue_syscall,
	.attach = &__esil_attach,
	.detach = &__esil_detach,
	.wait = &__esil_wait,
	.stop = __esil_stop,
	.kill = __esil_kill,
	.breakpoint = &__esil_breakpoint,
	.reg_profile = __esil_reg_profile,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_DBG,
	.data = &r_debug_plugin_esil,
	.version = R2_VERSION
};
#endif
