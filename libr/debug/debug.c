/* radare - LGPL - Copyright 2009-2016 - pancake, jduck, TheLemonMan, saucec0de */

#include <r_debug.h>
#include <r_core.h>
#include <signal.h>

#if __WINDOWS__
void w32_break_process(void *);
#endif

R_LIB_VERSION(r_debug);

// Size of the lookahead buffers used in r_debug functions
#define DBG_BUF_SIZE 512

R_API RDebugInfo *r_debug_info(RDebug *dbg, const char *arg) {
	if (!dbg || !dbg->h || !dbg->h->info) {
		return NULL;
	}
	return dbg->h->info (dbg, arg);
}

R_API void r_debug_info_free (RDebugInfo *rdi) {
	free (rdi->cwd);
	free (rdi->exe);
	free (rdi->cmdline);
	free (rdi->libname);
}


/*
 * Recoiling after a breakpoint has two stages:
 * 1. remove the breakpoint and fix the program counter.
 * 2. on resume, single step once and then replace the breakpoint.
 *
 * Thus, we have two functions to handle these situations.
 * r_debug_bp_hit handles stage 1.
 * r_debug_recoil handles stage 2.
 */
static int r_debug_bp_hit(RDebug *dbg, RRegItem *pc_ri, ut64 pc, RBreakpointItem **pb) {
	RBreakpointItem *b;

	if (!pb) {
		eprintf ("BreakpointItem is NULL!\n");
		return false;
	}
	/* initialize the output parameter */
	*pb = NULL;

	/* if we are tracing, update the tracing data */
	if (dbg->trace->enabled) {
		r_debug_trace_pc (dbg, pc);
	}

	/* remove all sw breakpoints for now. we'll set them back in stage 2
	 *
	 * this is necessary because while stopped we don't want any breakpoints in
	 * the code messing up our analysis.
	 */
	if (!r_bp_restore (dbg->bp, false)) { // unset sw breakpoints
		return false;
	}

	/* if we are recoiling, tell r_debug_step that we ignored a breakpoint
	 * event */
	if (!dbg->swstep && dbg->recoil_mode != R_DBG_RECOIL_NONE) {
		dbg->reason.bp_addr = 0;
		return true;
	}

	/* The MIPS ptrace has a different behaviour */
# if __mips__
	/* see if we really have a breakpoint here... */
	b = r_bp_get_at (dbg->bp, pc);
	if (!b) { /* we don't. nothing left to do */
		return true;
	}
# else
	/* see if we really have a breakpoint here... */
	b = r_bp_get_at (dbg->bp, pc - dbg->bpsize);
	if (!b) { /* we don't. nothing left to do */
		return true;
	}

	/* set the pc value back */
	pc -= b->size;
	if (!r_reg_set_value (dbg->reg, pc_ri, pc)) {
		eprintf ("failed to set PC!\n");
		return false;
	}
	if (!r_debug_reg_sync (dbg, R_REG_TYPE_GPR, true)) {
		eprintf ("cannot set registers!\n");
		return false;
	}
# endif

	*pb = b;

	/* if we are on a software stepping breakpoint, we hide what is going on... */
	if (b->swstep) {
		dbg->reason.bp_addr = 0;
		return true;
	}

	/* setup our stage 2 */
	dbg->reason.bp_addr = b->addr;

	/* inform the user of what happened */
	if (dbg->hitinfo) {
		eprintf ("hit %spoint at: %"PFMT64x "\n",
				b->trace ? "trace" : "break", pc);
	}

	/* now that we've cleaned up after the breakpoint, call the other
	 * potential breakpoint handlers
	 */
	if (dbg->corebind.core && dbg->corebind.bphit) {
		dbg->corebind.bphit (dbg->corebind.core, b);
	}
	return true;
}

/* enable all software breakpoints */
static int r_debug_bps_enable(RDebug *dbg) {
	/* restore all sw breakpoints. we are about to step/continue so these need
	 * to be in place. */
	if (!r_bp_restore (dbg->bp, true))
		return false;

	/* done recoiling... */
	dbg->recoil_mode = R_DBG_RECOIL_NONE;
	return true;
}

/*
 * replace breakpoints before we continue execution
 *
 * this is called from r_debug_step_hard or r_debug_continue_kill
 *
 * this is a trick process because of breakpoints/tracepoints.
 *
 * if a breakpoint was just hit, we need step over that instruction before
 * allowing the caller to proceed as desired.
 *
 * if the user wants to step, the single step here does the job.
 */
static int r_debug_recoil(RDebug *dbg, RDebugRecoilMode rc_mode) {
	/* if bp_addr is not set, we must not have actually hit a breakpoint */
	if (!dbg->reason.bp_addr) {
		return r_debug_bps_enable (dbg);
	}

	/* don't do anything if we already are recoiling */
	if (dbg->recoil_mode != R_DBG_RECOIL_NONE) {
		/* the first time recoil is called with swstep, we just need to
		 * look up the bp and step past it.
		 * the second time it's called, the new sw breakpoint should exist
		 * so we just restore all except what we originally hit and reset.
		 */
		if (dbg->swstep) {
			if (!r_bp_restore_except (dbg->bp, true, dbg->reason.bp_addr)) {
				return false;
			}
			return true;
		}

		/* otherwise, avoid recursion */
		return true;
	}

	/* we have entered recoil! */
	dbg->recoil_mode = rc_mode;

	/* step over the place with the breakpoint and let the caller resume */
	if (r_debug_step (dbg, 1) != 1) {
		return false;
	}

	/* when stepping away from a breakpoint during recoil in stepping mode,
	 * the r_debug_bp_hit function tells us that it was called
	 * innapropriately by setting bp_addr back to zero. however, recoil_mode
	 * is still set. we use this condition to know not to proceed but
	 * pretend as if we had.
	 */
	if (!dbg->reason.bp_addr && dbg->recoil_mode == R_DBG_RECOIL_STEP) {
		return true;
	}

	return r_debug_bps_enable (dbg);
}

static int get_bpsz_arch(RDebug *dbg) {
#define CMP_ARCH(x) strncmp (dbg->arch, (x), R_MIN (len_arch, strlen ((x))))
	int bpsz , len_arch = strlen (dbg->arch);
	if (!CMP_ARCH ("arm")) {
		//TODO add better handle arm/thumb
		bpsz = 4;
	} else if (!CMP_ARCH ("mips")) {
		bpsz = 4;
	} else if (!CMP_ARCH ("ppc")) {
		bpsz = 4;
	} else if (!CMP_ARCH ("sparc")) {
		bpsz = 4;
	} else if (!CMP_ARCH ("sh")) {
		bpsz = 2;
	} else {
		bpsz = 1;
	}
	return bpsz;
}

/* add a breakpoint with some typical values */
R_API RBreakpointItem *r_debug_bp_add(RDebug *dbg, ut64 addr, int hw, char *module, st64 m_delta) {
	int bpsz = get_bpsz_arch (dbg);
	RBreakpointItem *bpi;
	const char *module_name = module;
	RListIter *iter;
	RDebugMap *map;

	if (!addr && module) {
		bool detect_module, valid = false;
		int perm;

		if (m_delta) {
			detect_module = false;
			RList *list = r_debug_modules_list (dbg);
			r_list_foreach (list, iter, map) {
				if (strstr (map->file, module)) {
					addr = map->addr + m_delta;
					module_name = map->file;
					break;
				}
			}
		} else {
			//module holds the address
			addr = (ut64)r_num_math (dbg->num, module);
			if (!addr) return NULL;
			detect_module = true;
		}
		r_debug_map_sync (dbg);
		r_list_foreach (dbg->maps, iter, map) {
			if (addr >= map->addr && addr < map->addr_end) {
				valid = true;
				if (detect_module) {
					module_name = map->file;
					m_delta = addr - map->addr;
				}
				perm = ((map->perm & 1) << 2) | (map->perm & 2) | ((map->perm & 4) >> 2);
				if (!(perm & R_BP_PROT_EXEC))
					eprintf ("WARNING: setting bp within mapped memory without exec perm\n");
				break;
			}
		}
		if (!valid) {
			eprintf ("WARNING: module's base addr + delta is not a valid address\n");
			return NULL;
		}
	}
	if (!module) {
		//express db breakpoints as dbm due to ASLR when saving into project
		r_debug_map_sync (dbg);
		r_list_foreach (dbg->maps, iter, map) {
			if (addr >= map->addr && addr < map->addr_end) {
				module_name = map->file;
				m_delta = addr - map->addr;
				break;
			}
		}
	}
	bpi = hw
		? r_bp_add_hw (dbg->bp, addr, bpsz, R_BP_PROT_EXEC)
		: r_bp_add_sw (dbg->bp, addr, bpsz, R_BP_PROT_EXEC);
	if (bpi) {
		if (module_name) {
			bpi->module_name = strdup (module_name);
			bpi->name = r_str_newf ("%s+0x%" PFMT64x, module_name, m_delta);
		}
		bpi->module_delta = m_delta;
	}
	return bpi;
}

R_API RDebug *r_debug_new(int hard) {
	RDebug *dbg = R_NEW0 (RDebug);
	if (!dbg) return NULL;
	// R_SYS_ARCH
	dbg->arch = strdup (R_SYS_ARCH);
	dbg->bits = R_SYS_BITS;
	dbg->trace_forks = 1;
	dbg->forked_pid = -1;
	dbg->trace_clone = 0;
	R_FREE (dbg->btalgo);
	dbg->trace_execs = 0;
	dbg->anal = NULL;
	dbg->snaps = r_list_newf (r_debug_snap_free);
	dbg->pid = -1;
	dbg->bpsize = 1;
	dbg->tid = -1;
	dbg->tree = r_tree_new ();
	dbg->tracenodes = sdb_new0 ();
	dbg->swstep = 0;
	dbg->stop_all_threads = false;
	dbg->trace = r_debug_trace_new ();
	dbg->cb_printf = (void *)printf;
	dbg->reg = r_reg_new ();
	dbg->num = r_num_new (r_debug_num_callback, dbg);
	dbg->h = NULL;
	dbg->threads = NULL;
	dbg->hitinfo = 1;
	/* TODO: needs a redesign? */
	dbg->maps = r_debug_map_list_new ();
	dbg->maps_user = r_debug_map_list_new ();
	r_debug_signal_init (dbg);
	if (hard) {
		dbg->bp = r_bp_new ();
		r_debug_plugin_init (dbg);
		dbg->bp->iob.init = false;
	}
	return dbg;
}

static int free_tracenodes_entry (RDebug *dbg, const char *k, const char *v) {
	ut64 v_num = r_num_get (NULL, v);
	free((void *)(size_t)v_num);
	return true;
}

R_API void r_debug_tracenodes_reset (RDebug *dbg) {
	sdb_foreach (dbg->tracenodes, (SdbForeachCallback)free_tracenodes_entry, dbg);
	sdb_reset (dbg->tracenodes);
}

R_API RDebug *r_debug_free(RDebug *dbg) {
	if (dbg) {
		// TODO: free it correctly.. we must ensure this is an instance and not a reference..
		r_bp_free (dbg->bp);
		//r_reg_free(&dbg->reg);
		r_list_free (dbg->snaps);
		r_list_free (dbg->maps);
		r_list_free (dbg->maps_user);
		r_list_free (dbg->threads);
		r_num_free (dbg->num);
		sdb_free (dbg->sgnls);
		r_tree_free (dbg->tree);
		sdb_foreach (dbg->tracenodes, (SdbForeachCallback)free_tracenodes_entry, dbg);
		sdb_free (dbg->tracenodes);
		//r_debug_plugin_free();
		free (dbg->btalgo);
		r_debug_trace_free (dbg->trace);
		dbg->trace = NULL;
		free (dbg->arch);
		free (dbg->glob_libs);
		free (dbg->glob_unlibs);
		free (dbg);
	}
	return NULL;
}

R_API int r_debug_attach(RDebug *dbg, int pid) {
	int ret = false;
	if (dbg && dbg->h && dbg->h->attach) {
		ret = dbg->h->attach (dbg, pid);
		if (ret != -1) {
			//eprintf ("Attached debugger to pid = %d, tid = %d\n", pid, ret);
			r_debug_select (dbg, pid, ret); //dbg->pid, dbg->tid);
		}
	}
	return ret;
}

/* stop execution of child process */
R_API int r_debug_stop(RDebug *dbg) {
	if (dbg && dbg->h && dbg->h->stop) {
		return dbg->h->stop (dbg);
	}
	return false;
}

R_API bool r_debug_set_arch(RDebug *dbg, const char *arch, int bits) {
	if (arch && dbg && dbg->h) {
		bool rc = r_sys_arch_match (dbg->h->arch, arch);
		if (rc) {
			switch (bits) {
			case 32:
				if (dbg->h->bits & R_SYS_BITS_32) {
					dbg->bits = R_SYS_BITS_32;
				}
				break;
			case 64:
				dbg->bits = R_SYS_BITS_64;
				break;
			}
			if (!dbg->h->bits) {
				dbg->bits = dbg->h->bits;
			} else if (!(dbg->h->bits & dbg->bits)) {
				dbg->bits = dbg->h->bits & R_SYS_BITS_64;
				if (!dbg->bits) {
					dbg->bits = dbg->h->bits & R_SYS_BITS_32;
				}
				if (!dbg->bits) {
					dbg->bits = R_SYS_BITS_32;
				}
			}
			free (dbg->arch);
			dbg->arch = strdup (arch);
			return true;
		}
	}
	return false;
}

/*
 * Save 4096 bytes from %esp
 * TODO: Add support for reverse stack architectures
 * Also known as r_debug_inject()
 */
R_API ut64 r_debug_execute(RDebug *dbg, const ut8 *buf, int len, int restore) {
	int orig_sz;
	ut8 stackbackup[4096];
	ut8 *backup, *orig = NULL;
	RRegItem *ri, *risp, *ripc;
	ut64 rsp, rpc, ra0 = 0LL;
	if (r_debug_is_dead (dbg))
		return false;
	ripc = r_reg_get (dbg->reg, dbg->reg->name[R_REG_NAME_PC], R_REG_TYPE_GPR);
	risp = r_reg_get (dbg->reg, dbg->reg->name[R_REG_NAME_SP], R_REG_TYPE_GPR);
	if (ripc) {
		r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false);
		orig = r_reg_get_bytes (dbg->reg, -1, &orig_sz);
		if (!orig) {
			eprintf ("Cannot get register arena bytes\n");
			return 0LL;
		}
		rpc = r_reg_get_value (dbg->reg, ripc);
		rsp = r_reg_get_value (dbg->reg, risp);

		backup = malloc (len);
		if (!backup) {
			free (orig);
			return 0LL;
		}
		dbg->iob.read_at (dbg->iob.io, rpc, backup, len);
		dbg->iob.read_at (dbg->iob.io, rsp, stackbackup, len);

		r_bp_add_sw (dbg->bp, rpc+len, dbg->bpsize, R_BP_PROT_EXEC);

		/* execute code here */
		dbg->iob.write_at (dbg->iob.io, rpc, buf, len);
		//r_bp_add_sw (dbg->bp, rpc+len, 4, R_BP_PROT_EXEC);
		r_debug_continue (dbg);
		//r_bp_del (dbg->bp, rpc+len);
		/* TODO: check if stopped in breakpoint or not */

		r_bp_del (dbg->bp, rpc+len);
		dbg->iob.write_at (dbg->iob.io, rpc, backup, len);
		if (restore) {
			dbg->iob.write_at (dbg->iob.io, rsp, stackbackup, len);
		}

		r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false);
		ri = r_reg_get (dbg->reg, dbg->reg->name[R_REG_NAME_A0], R_REG_TYPE_GPR);
		ra0 = r_reg_get_value (dbg->reg, ri);
		if (restore) {
			r_reg_read_regs (dbg->reg, orig, orig_sz);
		} else {
			r_reg_set_value (dbg->reg, ripc, rpc);
		}
		r_debug_reg_sync (dbg, R_REG_TYPE_GPR, true);
		free (backup);
		free (orig);
		eprintf ("ra0=0x%08"PFMT64x"\n", ra0);
	} else eprintf ("r_debug_execute: Cannot get program counter\n");
	return (ra0);
}

R_API int r_debug_startv(struct r_debug_t *dbg, int argc, char **argv) {
	/* TODO : r_debug_startv unimplemented */
	return false;
}

R_API int r_debug_start(RDebug *dbg, const char *cmd) {
	/* TODO: this argc/argv parser is done in r_io */
	// TODO: parse cmd and generate argc and argv
	return false;
}

R_API int r_debug_detach(RDebug *dbg, int pid) {
	if (dbg->h && dbg->h->detach)
		return dbg->h->detach (dbg, pid);
	return false;
}

R_API int r_debug_select(RDebug *dbg, int pid, int tid) {
	if (tid < 0) {
		tid = pid;
	}

	if (pid != -1 && tid != -1) {
		if (pid != dbg->pid || tid != dbg->tid) {
			eprintf ("= attach %d %d\n", pid, tid);
		}
	} else {
		if (dbg->pid != -1)
			eprintf ("Child %d is dead\n", dbg->pid);
	}

	if (dbg->h && dbg->h->select && !dbg->h->select (pid, tid))
		return false;

	r_io_system (dbg->iob.io, sdb_fmt (0, "pid %d", pid));

	dbg->pid = pid;
	dbg->tid = tid;

	return true;
}

R_API const char *r_debug_reason_to_string(int type) {
	switch (type) {
	case R_DEBUG_REASON_DEAD: return "dead";
	case R_DEBUG_REASON_ABORT: return "abort";
	case R_DEBUG_REASON_SEGFAULT: return "segfault";
	case R_DEBUG_REASON_NONE: return "none";
	case R_DEBUG_REASON_SIGNAL: return "signal";
	case R_DEBUG_REASON_BREAKPOINT: return "breakpoint";
	case R_DEBUG_REASON_TRACEPOINT: return "tracepoint";
	case R_DEBUG_REASON_READERR: return "read-error";
	case R_DEBUG_REASON_WRITERR: return "write-error";
	case R_DEBUG_REASON_DIVBYZERO: return "div-by-zero";
	case R_DEBUG_REASON_ILLEGAL: return "illegal";
	case R_DEBUG_REASON_UNKNOWN: return "unknown";
	case R_DEBUG_REASON_ERROR: return "error";
	case R_DEBUG_REASON_NEW_PID: return "new-pid";
	case R_DEBUG_REASON_NEW_TID: return "new-tid";
	case R_DEBUG_REASON_NEW_LIB: return "new-lib";
	case R_DEBUG_REASON_EXIT_PID: return "exit-pid";
	case R_DEBUG_REASON_EXIT_TID: return "exit-tid";
	case R_DEBUG_REASON_EXIT_LIB: return "exit-lib";
	case R_DEBUG_REASON_TRAP: return "trap";
	case R_DEBUG_REASON_SWI: return "software-interrupt";
	case R_DEBUG_REASON_INT: return "interrupt";
	case R_DEBUG_REASON_FPU: return "fpu";
	case R_DEBUG_REASON_STEP: return "step";
	}
	return "unhandled";
}

R_API RDebugReasonType r_debug_stop_reason(RDebug *dbg) {
	// TODO: return reason to stop debugging
	// - new process
	// - trap instruction
	// - illegal instruction
	// - fpu exception
	// return dbg->reason
	return dbg->reason.type;
}

/*
 * wait for an event to happen on the selected pid/tid
 *
 * Returns  R_DEBUG_REASON_*
 */
R_API RDebugReasonType r_debug_wait(RDebug *dbg, RBreakpointItem **bp) {
	RDebugReasonType reason = R_DEBUG_REASON_ERROR;
	if (!dbg) {
		return reason;
	}

	if (bp) {
		*bp = NULL;
	}
	/* default to unknown */
	dbg->reason.type = R_DEBUG_REASON_UNKNOWN;
	if (r_debug_is_dead (dbg)) {
		return R_DEBUG_REASON_DEAD;
	}

	/* if our debugger plugin has wait */
	if (dbg->h && dbg->h->wait) {
		reason = dbg->h->wait (dbg, dbg->pid);
		if (reason == R_DEBUG_REASON_DEAD) {
			eprintf ("\n==> Process finished\n\n");
			// XXX(jjd): TODO: handle fallback or something else
			//r_debug_select (dbg, -1, -1);
			return R_DEBUG_REASON_DEAD;
		}

		/* propagate errors from the plugin */
		if (reason == R_DEBUG_REASON_ERROR) {
			return R_DEBUG_REASON_ERROR;
		}

		/* read general purpose registers */
		if (!r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false)) {
			return R_DEBUG_REASON_ERROR;
		}

		/* if the underlying stop reason is a breakpoint, call the handlers */
		if (reason == R_DEBUG_REASON_BREAKPOINT || reason == R_DEBUG_REASON_STEP) {
			RRegItem *pc_ri;
			RBreakpointItem *b = NULL;
			ut64 pc;

			/* get the program coounter */
			pc_ri = r_reg_get (dbg->reg, dbg->reg->name[R_REG_NAME_PC], -1);
			if (!pc_ri) { /* couldn't find PC?! */
				return R_DEBUG_REASON_ERROR;
			}

			/* get the value */
			pc = r_reg_get_value (dbg->reg, pc_ri);

			if (!r_debug_bp_hit (dbg, pc_ri, pc, &b)) {
				return R_DEBUG_REASON_ERROR;
			}

			/* if we hit a tracing breakpoint, we need to continue in
			 * whatever mode the user desired. */
			if (dbg->corebind.core && b && b->cond) {
				if (bp) {
					*bp = b;
				}
				reason = R_DEBUG_REASON_COND;
			}
			if (b && b->trace) {
				reason = R_DEBUG_REASON_TRACEPOINT;
			}
		}

		dbg->reason.type = reason;
		if (reason == R_DEBUG_REASON_SIGNAL && dbg->reason.signum != -1) {
			/* handle signal on continuations here */
			eprintf ("got signal...\n");
			int what = r_debug_signal_what (dbg, dbg->reason.signum);
			const char *name = r_debug_signal_resolve_i (dbg, dbg->reason.signum);
			if (name && strcmp ("SIGTRAP", name)) {
				r_cons_printf ("[+] signal %d aka %s received %d\n",
						dbg->reason.signum, name, what);
			}
		}
	}
	return reason;
}

R_API int r_debug_step_soft(RDebug *dbg) {
	ut8 buf[32];
	ut64 pc, sp, r;
	ut64 next[2];
	RAnalOp op;
	int br, i, ret;
	union {
		ut64 r64;
		ut32 r32[2];
	} sp_top;
	union {
		ut64 r64;
		ut32 r32[2];
	} memval;

	if (dbg->recoil_mode == R_DBG_RECOIL_NONE) {
		dbg->recoil_mode = R_DBG_RECOIL_STEP;
	}

	if (r_debug_is_dead (dbg)) {
		return false;
	}

	pc = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_PC]);
	sp = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_SP]);

	if (!dbg->iob.read_at) {
		return false;
	}
	if (dbg->iob.read_at (dbg->iob.io, pc, buf, sizeof (buf)) < 0) {
		return false;
	}
	if (!r_anal_op (dbg->anal, &op, pc, buf, sizeof (buf))) {
		return false;
	}
	if (op.type == R_ANAL_OP_TYPE_ILL) {
		return false;
	}
	switch (op.type) {
	case R_ANAL_OP_TYPE_RET:
		dbg->iob.read_at (dbg->iob.io, sp, (ut8 *)&sp_top, 8);
		next[0] = (dbg->bits == R_SYS_BITS_32) ? sp_top.r32[0] : sp_top.r64;
		br = 1;
		break;
	case R_ANAL_OP_TYPE_CJMP:
	case R_ANAL_OP_TYPE_CCALL:
		next[0] = op.jump;
		next[1] = op.fail;
		br = 2;
		break;
	case R_ANAL_OP_TYPE_CALL:
	case R_ANAL_OP_TYPE_JMP:
		next[0] = op.jump;
		br = 1;
		break;
	case R_ANAL_OP_TYPE_RJMP:
	case R_ANAL_OP_TYPE_RCALL:
		r = r_debug_reg_get (dbg,op.reg);
		next[0] = r;
		br = 1;
		break;
	case R_ANAL_OP_TYPE_IRCALL:
	case R_ANAL_OP_TYPE_IRJMP:
		r = r_debug_reg_get (dbg,op.reg);
		if (dbg->iob.read_at (dbg->iob.io, r, (ut8*)&memval, 8) <0 ) {
			next[0] = op.addr + op.size;
		} else {
			next[0] = (dbg->bits == R_SYS_BITS_32) ? memval.r32[0] : memval.r64;
		}
		br = 1;
		break;
	case R_ANAL_OP_TYPE_UCALL:
	case R_ANAL_OP_TYPE_MJMP:
		if (op.ireg) {
			r = r_debug_reg_get (dbg,op.ireg);
		} else {
			r = 0;
		}
		if (dbg->iob.read_at (dbg->iob.io,
		      r*op.scale + op.disp, (ut8*)&memval, 8) <0 ) {
			next[0] = op.addr + op.size;
		} else {
			next[0] = (dbg->bits == R_SYS_BITS_32) ? memval.r32[0] : memval.r64;
		}
		br = 1;
		break;
	case R_ANAL_OP_TYPE_UJMP:
	default:
		next[0] = op.addr + op.size;
		br = 1;
		break;
	}

	for (i = 0; i < br; i++) {
		RBreakpointItem *bpi = r_bp_add_sw (dbg->bp, next[i], dbg->bpsize, R_BP_PROT_EXEC);
		if (bpi) {
			bpi->swstep = true;
		}
	}

	ret = r_debug_continue (dbg);

	for (i = 0; i < br; i++) {
		r_bp_del (dbg->bp, next[i]);
	}

	return ret;
}

R_API int r_debug_step_hard(RDebug *dbg) {
	RDebugReasonType reason;

	dbg->reason.type = R_DEBUG_REASON_STEP;
	if (r_debug_is_dead (dbg)) {
		return false;
	}

	/* only handle recoils when not already in recoil mode. */
	if (dbg->recoil_mode == R_DBG_RECOIL_NONE) {
		/* handle the stage-2 of breakpoints */
		if (!r_debug_recoil (dbg, R_DBG_RECOIL_STEP)) {
			return false;
		}

		/* recoil already stepped once, so we don't step again. */
		if (dbg->recoil_mode == R_DBG_RECOIL_STEP) {
			dbg->recoil_mode = R_DBG_RECOIL_NONE;
			return true;
		}
	}

	if (!dbg->h->step (dbg)) {
		return false;
	}
	reason = r_debug_wait (dbg, NULL);
	/* TODO: handle better */
	if (reason == R_DEBUG_REASON_ERROR) {
		return false;
	}
	if (reason == R_DEBUG_REASON_DEAD || r_debug_is_dead (dbg)) {
		return false;
	}
	return true;
}

R_API int r_debug_step(RDebug *dbg, int steps) {
	int ret, steps_taken = 0;

	/* who calls this without giving a positive number? */
	if (steps < 1) {
		steps = 1;
	}

	if (!dbg || !dbg->h) {
		return steps_taken;
	}

	if (r_debug_is_dead (dbg)) {
		return steps_taken;
	}

	dbg->reason.type = R_DEBUG_REASON_STEP;

	for (; steps_taken < steps; steps_taken++) {
		if (dbg->swstep) {
			ret = r_debug_step_soft (dbg);
		} else {
			ret = r_debug_step_hard (dbg);
		}
		if (!ret) {
			eprintf ("Stepping failed!\n");
			return steps_taken;
		}
		dbg->steps++;
		dbg->reason.type = R_DEBUG_REASON_STEP;
	}

	return steps_taken;
}

R_API void r_debug_io_bind(RDebug *dbg, RIO *io) {
	r_io_bind (io, &dbg->bp->iob);
	r_io_bind (io, &dbg->iob);
}

R_API int r_debug_step_over(RDebug *dbg, int steps) {
	RAnalOp op;
	ut64 buf_pc, pc, ins_size;
	ut8 buf[DBG_BUF_SIZE];
	int steps_taken = 0;

	if (r_debug_is_dead (dbg)) {
		return steps_taken;
	}

	if (steps < 1) {
		steps = 1;
	}

	if (dbg->h && dbg->h->step_over) {
		for (; steps_taken < steps; steps_taken++)
			if (!dbg->h->step_over (dbg))
				return steps_taken;
		return steps_taken;
	}

	if (!dbg->anal || !dbg->reg)
		return steps_taken;

	// Initial refill
	buf_pc = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_PC]);
	dbg->iob.read_at (dbg->iob.io, buf_pc, buf, sizeof (buf));

	for (; steps_taken < steps; steps_taken++) {
		pc = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_PC]);
		// Try to keep the buffer full
		if (pc - buf_pc > sizeof (buf)) {
			buf_pc = pc;
			dbg->iob.read_at (dbg->iob.io, buf_pc, buf, sizeof (buf));
		}
		// Analyze the opcode
		if (!r_anal_op (dbg->anal, &op, pc, buf + (pc - buf_pc), sizeof (buf) - (pc - buf_pc))) {
			eprintf ("Decode error at %"PFMT64x"\n", pc);
			return steps_taken;
		}
		if (op.fail == -1) {
			ins_size = pc + op.size;
		} else {
			// Use op.fail here instead of pc+op.size to enforce anal backends to fill in this field
			ins_size = op.fail;
		}
		// Skip over all the subroutine calls
		if ((op.type & R_ANAL_OP_TYPE_MASK) == R_ANAL_OP_TYPE_CALL ||
			(op.type & R_ANAL_OP_TYPE_MASK) == R_ANAL_OP_TYPE_UCALL) {
			if (!r_debug_continue_until (dbg, ins_size)) {
				eprintf ("Could not step over call @ 0x%"PFMT64x"\n", pc);
				return steps_taken;
			}
		} else if ((op.prefix & (R_ANAL_OP_PREFIX_REP | R_ANAL_OP_PREFIX_REPNE | R_ANAL_OP_PREFIX_LOCK))) {
			//eprintf ("REP: skip to next instruction...\n");
			if (!r_debug_continue_until (dbg, ins_size)) {
				eprintf ("step over failed over rep\n");
				return steps_taken;
			}
		} else {
			r_debug_step (dbg, 1);
		}
	}

	return steps_taken;
}

R_API int r_debug_continue_kill(RDebug *dbg, int sig) {
	RDebugReasonType reason, ret = false;
	RBreakpointItem *bp = NULL;

	if (!dbg) {
		return false;
	}

#if __WINDOWS__
	r_cons_break (w32_break_process, dbg);
#endif
repeat:
	if (r_debug_is_dead (dbg)) {
		return false;
	}
	if (dbg->h && dbg->h->cont) {
		/* handle the stage-2 of breakpoints */
		if (!r_debug_recoil (dbg, R_DBG_RECOIL_CONTINUE))
			return false;

		/* tell the inferior to go! */
		ret = dbg->h->cont (dbg, dbg->pid, dbg->tid, sig);

		//XXX(jjd): why? //dbg->reason.signum = 0;

		reason = r_debug_wait (dbg, &bp);
		if (dbg->corebind.core) {
			RCore *core = (RCore *)dbg->corebind.core;
			RNum *num = core->num;
			if (reason == R_DEBUG_REASON_COND) {
				if (bp->cond && dbg->corebind.cmd) {
					dbg->corebind.cmd (dbg->corebind.core, bp->cond);
				}
				if (num->value) {
					goto repeat;
				}
			}
		}


#if __WINDOWS__
		if (reason != R_DEBUG_REASON_DEAD) {
			// XXX(jjd): returning a thread id?!
			ret = dbg->tid;
		}
		if (reason == R_DEBUG_REASON_NEW_LIB ||
			reason == R_DEBUG_REASON_EXIT_LIB) {
			goto repeat;
		}
#endif

		/* if continuing killed the inferior, we won't be able to get
		 * the registers.. */
		if (reason == R_DEBUG_REASON_DEAD || r_debug_is_dead (dbg)) {
			return false;
		}

		/* if we hit a tracing breakpoint, we need to continue in
		 * whatever mode the user desired. */
		if (reason == R_DEBUG_REASON_TRACEPOINT) {
			r_debug_step (dbg, 1);
			goto repeat;
		}

		/* choose the thread that was returned from the continue function */
		// XXX(jjd): there must be a cleaner way to do this...
		r_debug_select (dbg, dbg->pid, ret);
		sig = 0; // clear continuation after signal if needed

		/* handle general signals here based on the return from the wait
		 * function */
		if (dbg->reason.signum != -1) {
			int what = r_debug_signal_what (dbg, dbg->reason.signum);
			if (what & R_DBG_SIGNAL_CONT) {
				sig = dbg->reason.signum;
				eprintf ("Continue into the signal %d handler\n", sig);
				goto repeat;
			} else if (what & R_DBG_SIGNAL_SKIP) {
				// skip signal. requires skipping one instruction
				ut8 buf[64];
				RAnalOp op = {0};
				ut64 pc = r_debug_reg_get (dbg, "PC");
				dbg->iob.read_at (dbg->iob.io, pc, buf, sizeof (buf));
				r_anal_op (dbg->anal, &op, pc, buf, sizeof (buf));
				if (op.size > 0) {
					const char *signame = r_debug_signal_resolve_i (dbg, dbg->reason.signum);
					r_debug_reg_set (dbg, "PC", pc+op.size);
					eprintf ("Skip signal %d handler %s\n",
						dbg->reason.signum, signame);
					goto repeat;
				} else {
					ut64 pc = r_debug_reg_get (dbg, "PC");
					eprintf ("Stalled with an exception at 0x%08"PFMT64x"\n", pc);
				}
			}
		}
	}
	return ret;
}

R_API int r_debug_continue(RDebug *dbg) {
	return r_debug_continue_kill (dbg, 0); //dbg->reason.signum);
}

R_API int r_debug_continue_until_nontraced(RDebug *dbg) {
	eprintf ("TODO\n");
	return false;
}

R_API int r_debug_continue_until_optype(RDebug *dbg, int type, int over) {
	int ret, n = 0;
	ut64 pc, buf_pc = 0;
	RAnalOp op;
	ut8 buf[DBG_BUF_SIZE];

	if (r_debug_is_dead (dbg)) {
		return false;
	}

	if (!dbg->anal || !dbg->reg) {
		eprintf ("Undefined pointer at dbg->anal\n");
		return false;
	}

	r_debug_step (dbg, 1);
	r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false);

	// Initial refill
	buf_pc = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_PC]);
	dbg->iob.read_at (dbg->iob.io, buf_pc, buf, sizeof (buf));

	// step first, we dont want to check current optype
	for (;;) {
		if (!r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false))
			break;

		pc = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_PC]);
		// Try to keep the buffer full
		if (pc - buf_pc > sizeof (buf)) {
			buf_pc = pc;
			dbg->iob.read_at (dbg->iob.io, buf_pc, buf, sizeof (buf));
		}
		// Analyze the opcode
		if (!r_anal_op (dbg->anal, &op, pc, buf + (pc - buf_pc), sizeof (buf) - (pc - buf_pc))) {
			eprintf ("Decode error at %"PFMT64x"\n", pc);
			return false;
		}
		if (op.type == type)
			break;
		// Step over and repeat
		ret = over
			? r_debug_step_over (dbg, 1)
			: r_debug_step (dbg, 1);

		if (!ret) {
			eprintf ("r_debug_step: failed\n");
			break;
		}
		n++;
	}

	return n;
}

R_API int r_debug_continue_until(RDebug *dbg, ut64 addr) {
	int has_bp;
	ut64 pc;

	if (r_debug_is_dead (dbg))
		return false;

	// Check if there was another breakpoint set at addr
	has_bp = r_bp_get_in (dbg->bp, addr, R_BP_PROT_EXEC) != NULL;
	if (!has_bp)
		r_bp_add_sw (dbg->bp, addr, dbg->bpsize, R_BP_PROT_EXEC);

	// Continue until the bp is reached
	for (;;) {
		if (r_debug_is_dead (dbg))
			break;
		pc = r_debug_reg_get (dbg, dbg->reg->name[R_REG_NAME_PC]);
		if (pc == addr)
			break;
		if (r_bp_get_at (dbg->bp, pc))
			break;
		r_debug_continue (dbg);
	}
	// Clean up if needed
	if (!has_bp) {
		r_bp_del (dbg->bp, addr);
	}
	return true;
}

static int show_syscall(RDebug *dbg, const char *sysreg) {
	const char *sysname;
	char regname[8];
	int reg, i, args;
	RSyscallItem *si;
	reg = (int)r_debug_reg_get (dbg, sysreg);
	si = r_syscall_get (dbg->anal->syscall, reg, -1);
	if (si) {
		sysname = si->name? si->name: "unknown";
		args = si->args;
	} else {
		sysname = "unknown";
		args = 3;
	}
	eprintf ("--> %s 0x%08"PFMT64x" syscall %d %s (", sysreg,
			r_debug_reg_get (dbg, "PC"), reg, sysname);
	for (i=0; i<args; i++) {
		ut64 val;
		snprintf (regname, sizeof (regname)-1, "A%d", i);
		val = r_debug_reg_get (dbg, regname);
		if (((st64)val<0) && ((st64)val>-0xffff)) {
			eprintf ("%"PFMT64d"%s", val, (i+1==args)?"":" ");
		} else {
			eprintf ("0x%"PFMT64x"%s", val, (i+1==args)?"":" ");
		}
	}
	eprintf (")\n");
	r_syscall_item_free (si);
	return reg;
}

R_API int r_debug_continue_syscalls(RDebug *dbg, int *sc, int n_sc) {
	int i, err, reg, ret = false;
	if (!dbg || !dbg->h || r_debug_is_dead (dbg)) {
		return false;
	}
	if (!dbg->h->contsc) {
		/* user-level syscall tracing */
		r_debug_continue_until_optype (dbg, R_ANAL_OP_TYPE_SWI, 0);
		return show_syscall (dbg, "A0");
	}

	if (!r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false)) {
		eprintf ("--> cannot read registers\n");
		return -1;
	}
	reg = (int)r_debug_reg_get_err (dbg, "SN", &err);
	if (err) {
		eprintf ("Cannot find 'sn' register for current arch-os.\n");
		return -1;
	}
	for (;;) {
		RDebugReasonType reason;

		if (r_cons_singleton()->breaked)
			break;
#if __linux__
		// step is needed to avoid dupped contsc results
		/* XXX(jjd): actually one stop is before the syscall, the other is
		 * after.  this allows you to inspect the arguments before and the
		 * return value after... */
		r_debug_step (dbg, 1);
#endif
		dbg->h->contsc (dbg, dbg->pid, 0); // TODO handle return value
		// wait until continuation
		reason = r_debug_wait (dbg, NULL);
		if (reason == R_DEBUG_REASON_DEAD || r_debug_is_dead (dbg)) {
			break;
		}
#if 0
		if (reason != R_DEBUG_REASON_STEP) {
			eprintf ("astep\n");
			break;
		}
#endif
		if (!r_debug_reg_sync (dbg, R_REG_TYPE_GPR, false)) {
			eprintf ("--> cannot sync regs, process is probably dead\n");
			return -1;
		}
		reg = show_syscall (dbg, "SN");
		if (n_sc == -1) {
			continue;
		}
		if (n_sc == 0) {
			break;
		}
		for (i = 0; i < n_sc; i++) {
			if (sc[i] == reg) {
				return reg;
			}
		}
		// TODO: must use r_core_cmd(as)..import code from rcore
	}
	return ret;
}

R_API int r_debug_continue_syscall(RDebug *dbg, int sc) {
	return r_debug_continue_syscalls (dbg, &sc, 1);
}

// TODO: remove from here? this is code injection!
R_API int r_debug_syscall(RDebug *dbg, int num) {
	bool ret = true;
	if (dbg->h->contsc) {
		ret = dbg->h->contsc (dbg, dbg->pid, num);
	}
	eprintf ("TODO: show syscall information\n");
	/* r2rc task? ala inject? */
	return (int)ret;
}

R_API int r_debug_kill(RDebug *dbg, int pid, int tid, int sig) {
	if (r_debug_is_dead (dbg))
		return false;
	if (dbg->h && dbg->h->kill) {
		return dbg->h->kill (dbg, pid, tid, sig);
	}
	eprintf ("Backend does not implement kill()\n");
	return false;
}

R_API RList *r_debug_frames(RDebug *dbg, ut64 at) {
	if (dbg && dbg->h && dbg->h->frames) {
		return dbg->h->frames (dbg, at);
	}
	return NULL;
}

/* TODO: Implement fork and clone */
R_API int r_debug_child_fork(RDebug *dbg) {
	//if (dbg && dbg->h && dbg->h->frames)
		//return dbg->h->frames (dbg);
	return 0;
}

R_API int r_debug_child_clone(RDebug *dbg) {
	//if (dbg && dbg->h && dbg->h->frames)
		//return dbg->h->frames (dbg);
	return 0;
}

R_API int r_debug_is_dead(RDebug *dbg) {
	int is_dead = (dbg->pid == -1);
	if (!is_dead && dbg->h && dbg->h->kill) {
		is_dead = !dbg->h->kill (dbg, dbg->pid, false, 0);
	}
	if (is_dead) {
		dbg->reason.type = R_DEBUG_REASON_DEAD;
	}
	return is_dead;
}

R_API int r_debug_map_protect(RDebug *dbg, ut64 addr, int size, int perms) {
	if (dbg && dbg->h && dbg->h->map_protect) {
		return dbg->h->map_protect (dbg, addr, size, perms);
	}
	return false;
}

R_API void r_debug_drx_list(RDebug *dbg) {
	if (dbg && dbg->h && dbg->h->drx) {
		dbg->h->drx (dbg, 0, 0, 0, 0, 0);
	}
}

R_API int r_debug_drx_set(RDebug *dbg, int idx, ut64 addr, int len, int rwx, int g) {
	if (dbg && dbg->h && dbg->h->drx) {
		return dbg->h->drx (dbg, idx, addr, len, rwx, g);
	}
	return false;
}

R_API int r_debug_drx_unset(RDebug *dbg, int idx) {
	if (dbg && dbg->h && dbg->h->drx) {
		return dbg->h->drx (dbg, idx, 0, -1, 0, 0);
	}
	return false;
}
