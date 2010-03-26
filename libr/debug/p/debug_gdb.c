/* radare - LGPL - Copyright 2009-2010 pancake<nopcode.org> */

#include <r_debug.h>
#include <r_lib.h>
#include "libgdbwrap/include/gdbwrapper.h"

/* TODO: The IO stuff must be communicated with the r_dbg */
/* a transplant sometimes requires to change the IO */
/* so, for here, we need r_io_plugin_gdb */
/* TODO: rename to gdbwrap? */
static gdbwrap_t *desc = NULL;

static int r_debug_gdb_step(int pid) {
	gdbwrap_stepi (desc);
	return R_TRUE;
}

struct r_debug_regset_t * r_debug_gdb_reg_read(int pid) {
#if 0
	struct r_debug_regset *r = NULL;
	/* only for x86-32 */
	gdbwrap_gdbreg32 *reg = gdbwrap_readgenreg(desc);
	r = r_debug_regset_new(9);
	r_debug_regset_set(r, 0, "eax", reg->eax);
	r_debug_regset_set(r, 1, "ebx", reg->ebx);
	r_debug_regset_set(r, 2, "ecx", reg->ecx);
	r_debug_regset_set(r, 3, "edx", reg->edx);
	r_debug_regset_set(r, 4, "esi", reg->esi);
	r_debug_regset_set(r, 5, "edi", reg->edi);
	r_debug_regset_set(r, 6, "esp", reg->esp);
	r_debug_regset_set(r, 7, "ebp", reg->ebp);
	r_debug_regset_set(r, 8, "eip", reg->eip);
	return r;
#endif
	return NULL;
}

static int r_debug_gdb_reg_write(int pid, int type, const ut8 *buf, int size) {
	/* TODO */
	return R_TRUE;
}

static int r_debug_gdb_continue(int pid, int sig) {
	gdbwrap_continue(desc);
	return R_TRUE;
}

static int r_debug_gdb_wait(int pid) {
	/* do nothing */
	return R_TRUE;
}

static int r_debug_gdb_attach(int pid) {
// XXX TODO PID must be a socket here !!1
	desc = gdbwrap_init (pid);
	return R_TRUE;
}

static int r_debug_gdb_detach(int pid) {
// XXX TODO PID must be a socket here !!1
	close (pid);
	return R_TRUE;
}

struct r_debug_handle_t r_dbg_plugin_gdb = {
	.name = "gdb",
	.archs = { "x86", 0 }, //"x86-64", "arm", "powerpc", 0 },
	.step = &r_debug_gdb_step,
	.cont = &r_debug_gdb_continue,
	.attach = &r_debug_gdb_attach,
	.detach = &r_debug_gdb_detach,
	.wait = &r_debug_gdb_wait,
	.reg_read = &r_debug_gdb_reg_read,
	.reg_write = &r_debug_gdb_reg_write,
	//.bp_write = &r_debug_gdb_bp_write,
	//.bp_read = &r_debug_gdb_bp_read,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_DBG,
	.data = &r_debug_plugin_gdb
};
#endif
