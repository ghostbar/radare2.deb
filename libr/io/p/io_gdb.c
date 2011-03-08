/* radare - LGPL - Copyright 2010-2011 pancake<nopcode.org> */

#include <r_io.h>
#include <r_lib.h>
#include <r_socket.h>
#include <r_util.h>
#include "../../debug/p/libgdbwrap/gdbwrapper.c"
#include "../../debug/p/libgdbwrap/interface.c"

typedef struct {
	int fd;
	gdbwrap_t *desc;
} RIOGdb;
#define RIOGDB_FD(x) (((RIOGdb*)(x))->fd)
#define RIOGDB_DESC(x) (((RIOGdb*)(x))->desc)
#define RIOGDB_IS_VALID(x) (x && x->plugin==&r_io_plugin_gdb && x->data)

static int __plugin_open(RIO *io, const char *file) {
	return (!memcmp (file, "gdb://", 6));
}

static RIODesc *__open(RIO *io, const char *file, int rw, int mode) {
	char host[128], *port;
	int _fd;
	RIOGdb *riog;
	if (!__plugin_open (io, file))
		return NULL;
	strncpy (host, file+6, sizeof (host)-1);
	port = strchr (host , ':');
	if (!port) {
		eprintf ("Port not specified. Please use gdb://[host]:[port]\n");
		return NULL;
	}
	*port = '\0';
	_fd = r_socket_connect (host, atoi (port+1));
	if (_fd == -1) {
		eprintf ("Cannot connect to host.\n");
		return NULL;
	}
	riog = R_NEW (RIOGdb);
	riog->fd = _fd;
	riog->desc = gdbwrap_init (_fd);
	return r_io_desc_new (&r_io_plugin_shm, _fd, file, rw, mode, riog);
}

static int __write(RIO *io, RIODesc *fd, const ut8 *buf, int count) {
	gdbwrap_writemem (RIOGDB_DESC (fd), io->off, (void *)buf, count);
	return count;
}

static ut64 __lseek(RIO *io, RIODesc *fd, ut64 offset, int whence) {
        return offset;
}

static int __read(RIO *io, RIODesc *fd, ut8 *buf, int count) {
	memset (buf, 0xff, count);
	if (RIOGDB_IS_VALID (fd)) {
		char *ptr = gdbwrap_readmem (RIOGDB_DESC (fd), (la32)io->off, count);
		if (ptr == NULL)
			return -1;
		//eprintf ("READ %llx (%s)\n", (ut64)io->off, ptr);
		return r_hex_str2bin (ptr, buf);
	}
	return -1;
}

static int __close(RIODesc *fd) {
	return -1;
}

struct r_io_plugin_t r_io_plugin_gdb = {
        //void *plugin;
	.name = "gdb",
        .desc = "Attach to a running 'gdbserver', 'qemu -s' or other, gdb://localhost:1234", 
        .open = __open,
        .close = __close,
	.read = __read,
	.write = __write,
        .plugin_open = __plugin_open,
	.lseek = __lseek,
	.system = NULL,
	.debug = (void *)1,
};
