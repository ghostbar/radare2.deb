/* radare - LGPL - Copyright 2016 pancake */

#include <r_io.h>
#include <r_asm.h>
#include <r_debug.h>

static int __io_step(RDebug *dbg) {
	dbg->iob.system (dbg->iob.io, "ds");
	r_cons_reset ();
	return 0;
}

static int __io_step_over(RDebug *dbg) {
	dbg->iob.system (dbg->iob.io, "dso");
	r_cons_reset ();
	return true;
}

static RList *__io_maps(RDebug *dbg) {
	RList *list = r_list_new ();
	dbg->iob.system (dbg->iob.io, "dm");
	char *ostr, *str = strdup (r_cons_get_buffer ());
	ut64 map_start, map_end;
	char perm[32];
	char name[512];
	ostr = str;
	while (true) {
		char *nl = strchr (str, '\n');
		if (nl) {
			*nl = 0;
			*name = 0;
			*perm = 0;
			map_start = map_end = 0LL;
			if (!strncmp (str, "sys ", 4)) {
				char *sp = strchr (str + 4, ' ');
				if (sp) {
					str = sp + 1;
				} else {
					str += 4;
				}
			}
			char *_s_ = strstr (str, " s ");
			if (_s_) {
				memmove (_s_, _s_ + 2, strlen (_s_));
			}
			_s_ = strstr (str, " ? ");
			if (_s_) {
				memmove (_s_, _s_ + 2, strlen (_s_));
			}
			sscanf (str, "0x%"PFMT64x" - 0x%"PFMT64x" %s %s",
				&map_start, &map_end, perm, name);
			if (map_end != 0LL) {
				RDebugMap *map = r_debug_map_new (name, map_start, map_end, r_str_rwx (perm), 0);
				r_list_append (list, map);
			}
			str = nl + 1;
		} else {
			break;
		}
	}
	free (ostr);
	r_cons_reset();
	return list;
}

static int __io_wait(RDebug *dbg, int pid) {
	/* do nothing */
	return true;
}

static int curPid = -1;
static int __io_attach(RDebug *dbg, int pid) {
	curPid = pid;
	return true;
}

// "drp" register profile
static char *__io_reg_profile(RDebug *dbg) {
	r_cons_push ();
	dbg->iob.system (dbg->iob.io, "drp");
	const char *buf = r_cons_get_buffer ();
	if (buf && *buf) {
		char *ret = strdup (buf);
		r_cons_pop ();
		return ret;
	}
	return r_anal_get_reg_profile (dbg->anal);
}

// "dr8" read register state
static int __io_read(RDebug *dbg, int type, ut8 *buf, int size) {
	dbg->iob.system (dbg->iob.io, "dr8");
	char *regs = strdup (r_cons_get_buffer ());
	ut8 *bregs = calloc (1, strlen (regs));
	r_cons_reset ();
	int sz = r_hex_str2bin (regs, bregs);
	if (sz > 0) {
		memcpy (buf, bregs, R_MIN (size, sz));
		free (bregs);
		return size;
	} else {
		eprintf ("SIZE %d (%s)\n", sz, regs);
	}
	free (bregs);
	return -1;
}

// "dc" continue execution
static int __io_continue(RDebug *dbg, int pid, int tid, int sig) {
	dbg->iob.system (dbg->iob.io, "dc");
	r_cons_flush ();
	return true;
}

// "dk" send kill signal
static int __io_kill(RDebug *dbg, int pid, int tid, int sig) {
	const char *cmd = sdb_fmt (-1, "dk %d", sig);
	dbg->iob.system (dbg->iob.io, cmd);
	r_cons_flush ();
	return true;
}

RDebugPlugin r_debug_plugin_io = {
	.name = "io",
	.keepio = 1,
	.license = "MIT",
	.arch = "any", // TODO: exception!
	.bits = R_SYS_BITS_32 | R_SYS_BITS_64,
	.step = __io_step,
	.map_get = __io_maps,
	.attach = &__io_attach,
	.wait = &__io_wait,
	.reg_read = __io_read,
	.cont = __io_continue,
	.kill = __io_kill,
	.reg_profile = __io_reg_profile,
	.step_over = __io_step_over,
#if 0
	.init = __esil_init,
	.contsc = __esil_continue_syscall,
	.detach = &__esil_detach,
	.stop = __esil_stop,
	.breakpoint = &__esil_breakpoint,
#endif
};

#ifndef CORELIB
RLibStruct radare_plugin = {
	.type = R_LIB_TYPE_DBG,
	.data = &r_debug_plugin_io,
	.version = R2_VERSION
};
#endif
