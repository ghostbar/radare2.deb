/* radare 2008-2011 LGPL -- pancake <youterm.com> */

#include <r_types.h>
#include <r_util.h>
#include <r_syscall.h>
#include <stdio.h>
#include <string.h>
#include "fastcall.h"

extern RSyscallItem syscalls_netbsd_x86[];
extern RSyscallItem syscalls_linux_x86[];
extern RSyscallItem syscalls_linux_mips[];
extern RSyscallItem syscalls_linux_arm[];
extern RSyscallItem syscalls_freebsd_x86[];
extern RSyscallItem syscalls_darwin_x86[];
extern RSyscallItem syscalls_darwin_arm[];
extern RSyscallItem syscalls_win7_x86[];
extern RSyscallPort sysport_x86[];

R_API RSyscall* r_syscall_new() {
	RSyscall *rs = R_NEW (RSyscall);
	if (rs) {
		rs->fd = NULL;
		rs->sysptr = syscalls_linux_x86;
		rs->sysport = sysport_x86;
		rs->printf = (PrintfCallback)printf;
		rs->regs = fastcall_x86;
	}
	return rs;
}

R_API void r_syscall_free(RSyscall *ctx) {
	free (ctx);
}

/* return fastcall register argument 'idx' for a syscall with 'num' args */
R_API const char *r_syscall_reg(RSyscall *s, int idx, int num) {
	num--; idx--; // arg 1 = index 0
	if (num<0 || num>=R_SYSCALL_ARGS || idx<0 || idx>=R_SYSCALL_ARGS)
		return NULL;
	return s->regs[num].arg[idx];
}

R_API int r_syscall_setup(RSyscall *ctx, const char *arch, const char *os) {
	if (os == NULL)
		os = R_SYS_OS;
	if (arch == NULL)
		arch = R_SYS_ARCH;
	/// XXX: spaghetti here
	if (!strcmp (os, "any")) {
		// ignored
		return R_TRUE;
	}
	if (!strcmp (arch, "mips")) {
		ctx->regs = fastcall_mips;
		if (!strcmp (os, "linux"))
			ctx->sysptr = syscalls_linux_mips;
		else {
			eprintf ("r_syscall_setup: Unknown arch '%s'\n", arch);
			return R_FALSE;
		}
	} else
	if (!strcmp (arch, "arm")) {
		ctx->regs = fastcall_arm;
		if (!strcmp (os, "linux"))
			ctx->sysptr = syscalls_linux_arm;
		else
		if (!strcmp (os, "macos") || !strcmp (os, "darwin"))
			ctx->sysptr = syscalls_darwin_arm;
		else {
			eprintf ("r_syscall_setup: Unknown OS '%s'\n", os);
			return R_FALSE;
		}
	} else
	if (!strcmp (arch, "x86")) {
		ctx->regs = fastcall_x86;
		if (!strcmp (os, "linux"))
			ctx->sysptr = syscalls_linux_x86;
		else if (!strcmp (os, "netbsd"))
			ctx->sysptr = syscalls_netbsd_x86;
		else if (!strcmp (os, "freebsd"))
			ctx->sysptr = syscalls_freebsd_x86;
		//else if (!strcmp (os, "openbsd"))
		//	ctx->sysptr = syscalls_openbsd_x86;
		else if ((!strcmp (os, "darwin")) || (!strcmp (os, "macos")))
			ctx->sysptr = syscalls_darwin_x86;
		else if (!strcmp (os, "windows")) //win7
			ctx->sysptr = syscalls_win7_x86;
		else {
			eprintf ("r_syscall_setup: Unknown os '%s'\n", os);
			return R_FALSE;
		}
	} else {
		eprintf ("r_syscall_setup: Unknown os/arch '%s'/'%s'\n", os, arch);
		return R_FALSE;
	}
	if (ctx->fd)
		fclose (ctx->fd);
	ctx->fd = NULL;
	return R_TRUE;
}

R_API int r_syscall_setup_file(RSyscall *ctx, const char *path) {
	if (ctx->fd)
		fclose (ctx->fd);
	ctx->fd = fopen (path, "r");
	if (ctx->fd == NULL)
		return 1;
	/* TODO: load info from file */
	return 0;
}

R_API RSyscallItem *r_syscall_get(RSyscall *ctx, int num, int swi) {
	int i;
	for (i=0; ctx->sysptr[i].name; i++) {
		if (num == ctx->sysptr[i].num && \
				(swi == -1 || swi == ctx->sysptr[i].swi))
			return &ctx->sysptr[i];
	}
	return NULL;
}

R_API int r_syscall_get_num(RSyscall *ctx, const char *str) {
	int i;
	for (i=0; ctx->sysptr[i].num;i++)
		if (!strcmp (str, ctx->sysptr[i].name))
			return ctx->sysptr[i].num;
	return 0;
}

// we can probably wrap all this with r_list getters
/* XXX: ugly iterator implementation */
R_API RSyscallItem *r_syscall_get_n(RSyscall *ctx, int n) {
	int i;
	for (i=0; ctx->sysptr[i].num && i!=n; i++)
		return &ctx->sysptr[i];
	return NULL;
}

R_API const char *r_syscall_get_i(RSyscall *ctx, int num, int swi) {
	int i;
	for (i=0; ctx->sysptr[i].num; i++) {
		if (num == ctx->sysptr[i].num && \
				(swi == -1 || swi == ctx->sysptr[i].swi))
			return ctx->sysptr[i].name;
	}
	return NULL;
}

R_API const char *r_syscall_get_io(RSyscall *ctx, int ioport) {
	int i;
	for (i=0; ctx->sysport[i].name; i++) {
		if (ioport == ctx->sysport[i].port)
			return ctx->sysport[i].name;
	}
	return NULL;
}

R_API void r_syscall_list(RSyscall *ctx) {
	int i;
	for (i=0; ctx->sysptr[i].num; i++) {
		ctx->printf ("%02x: %d = %s\n",
			ctx->sysptr[i].swi, ctx->sysptr[i].num, ctx->sysptr[i].name);
	}
}
