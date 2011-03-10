/* radare - LGPL - Copyright 2008-2011 pancake<nopcode.org> */

#include <r_userconf.h>

#include <r_io.h>
#include <r_lib.h>
#include <r_cons.h>

#if __WINDOWS__

#include <windows.h>
#include <tlhelp32.h>

typedef struct {
	int pid;
	int tid;
	PROCESS_INFORMATION pi;
} RIOW32Dbg;
#define RIOW32DBG_PID(x) (((RIOW32Dbg*)x->data)->pid)

#undef R_IO_NFDS
#define R_IO_NFDS 2
extern int errno;

static int debug_os_read_at(RIOW32Dbg *dbg, void *buf, int len, ut64 addr) {
	DWORD ret;
        ReadProcessMemory (dbg->pi.hProcess, (PCVOID)(ULONG)addr, buf, len, &ret);
//	if (len != ret)
//		eprintf ("Cannot read 0x%08llx\n", addr);
	return len; // XXX: Handle read correctly and not break r2 shell
	//return (int)ret; //(int)len; //ret;
}

static int __read(struct r_io_t *io, RIODesc *fd, ut8 *buf, int len) {
	memset (buf, '\xff', len); // TODO: only memset the non-readed bytes
	return debug_os_read_at (fd->data, buf, len, io->off);
}

static int w32dbg_write_at(RIODesc *fd, const ut8 *buf, int len, ut64 addr) {
	DWORD ret;
	RIOW32Dbg *dbg = fd->data;
        WriteProcessMemory (dbg->pi.hProcess, (LPVOID)(ULONG)addr, buf, len, &ret);
	return (int)ret;
}

static int __write(struct r_io_t *io, RIODesc *fd, const ut8 *buf, int len) {
	return w32dbg_write_at (fd->data, buf, len, io->off);
}

static int __plugin_open(struct r_io_t *io, const char *file) {
	if (!memcmp (file, "attach://", 9))
		return R_TRUE;
	return (!memcmp (file, "w32dbg://", 9))? R_TRUE: R_FALSE;
}

static int __attach (RIOW32Dbg *dbg) {
	eprintf ("---> attach to %d\n", dbg->pid);
	dbg->pi.hProcess = OpenProcess (PROCESS_ALL_ACCESS, FALSE, dbg->pid);
	if (dbg->pi.hProcess == NULL)
		return -1;
	return dbg->pid;
}

static RIODesc *__open(struct r_io_t *io, const char *file, int rw, int mode) {
	if (__plugin_open (io, file)) {
		RIOW32Dbg *dbg = R_NEW (RIOW32Dbg);
		if (dbg == NULL)
			return NULL;
		dbg->pid = atoi (file+9);
		if (__attach (dbg) == -1) {
			free (dbg);
			return NULL;
		}
		return r_io_desc_new (&r_io_plugin_w32dbg, -1, file, R_TRUE, 0, dbg);
	}
	return NULL;
}

static ut64 __lseek(RIO *io, RIODesc *fd, ut64 offset, int whence) {
	return (!whence)?offset:whence==1?io->off+offset:UT64_MAX;
}

static int __close(RIODesc *fd) {
	// TODO: detach
	return R_TRUE;
}

static int __system(RIO *io, RIODesc *fd, const char *cmd) {
	RIOW32Dbg *iop = fd->data;
	//printf("w32dbg io command (%s)\n", cmd);
	/* XXX ugly hack for testing purposes */
	if (!strcmp (cmd, "pid")) {
		int pid = atoi (cmd+4);
		if (pid != 0)
			iop->pid = iop->tid = pid;
		io->printf ("\n");
		//printf("PID=%d\n", io->fd);
		return pid;
	} else eprintf ("Try: '|pid'\n");
	return R_TRUE;
}

static int __init(struct r_io_t *io) {
//	eprintf ("w32dbg init\n");
	return R_TRUE;
}

// TODO: rename w32dbg to io_w32dbg .. err io.w32dbg ??
struct r_io_plugin_t r_io_plugin_w32dbg = {
        //void *plugin;
	.name = "io_w32dbg",
        .desc = "w32dbg io",
        .open = __open,
        .close = __close,
	.read = __read,
        .plugin_open = __plugin_open,
	.lseek = __lseek,
	.system = __system,
	.init = __init,
	.write = __write,
        //void *widget;
/*
        struct debug_t *debug;
        ut32 (*write)(int fd, const ut8 *buf, ut32 count);
	int fds[R_IO_NFDS];
*/
};
#else
struct r_io_plugin_t r_io_plugin_w32dbg = {
	.name = "w32dbg",
        .desc = "w32dbg io (NOT SUPPORTED FOR THIS PLATFORM)",
};
#endif

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_IO,
	.data = &r_io_plugin_w32dbg
};
#endif
