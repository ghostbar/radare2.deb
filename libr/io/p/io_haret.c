/* radare - LGPL - Copyright 2008-2011 pancake<nopcode.org> */

#if __UNIX__

#include "r_io.h"
#include "r_lib.h"
#include "r_socket.h"
#include <sys/types.h>

#define HARET_FD(x) ((int)((size_t)x->data))

//#if (sizeof(int)) > (sizeof(void*))
//#error WTF int>ptr? wrong compiler or architecture?
//#endif

//int haret_fd = -1;

static int haret__write(struct r_io_t *io, RIODesc *fd, const ut8 *buf, int count) {
	/* TODO */
	return 0;
}

static void haret_wait_until_prompt(int ufd) {
	unsigned char buf;
	int off = 0;
	for (;;) {
		if (r_socket_read (ufd, &buf, 1) != 1) {
			eprintf ("haret_wait_until_prompt: Unexpected eof in socket\n");
			return;
		}
		switch (off) {
		case 0: if (buf == ')') off = 1; break;
		case 1: if (buf == '#') return; off = 0; break;
		}
	}
}

static int haret__read(struct r_io_t *io, RIODesc *fd, ut8 *buf, int count) {
	char tmp[1024];
	int i = 0;
	ut64 off, j;
	int ufd = HARET_FD (fd);

	off = io->off & -4;
	sprintf (tmp, "pdump 0x%"PFMT64x" %i\r\n", off, count+4);
	r_socket_write (ufd, tmp, strlen (tmp));
	r_socket_read_block (ufd, (unsigned char *) tmp, strlen (tmp)+1);
	j = (io->off - off)*2;
	while (i<count && j >= 0) {
		r_socket_read_block (ufd, (ut8*) tmp, 11);
		r_socket_read_block (ufd, (ut8*) tmp, 35);
		if (i+16 < count || (io->off-off) == 0) {
			tmp[35] = 0;
			i += r_hex_str2bin (tmp+j, buf+i);
			r_socket_read_block (ufd, (unsigned char *) tmp, 21);
		} else {
			tmp[(io->off - off)*2] = 0;
			i += r_hex_str2bin (tmp+j, buf+i);
		}
		j=0;
	}
	haret_wait_until_prompt (ufd);
	return i;
}

static int haret__close(RIODesc *fd) {
	if (!fd || HARET_FD (fd)==-1)
		return -1;
	return r_socket_close (HARET_FD (fd));
}

static int haret__plugin_open(struct r_io_t *io, const char *pathname) {
	return (!memcmp (pathname, "haret://", 8));
}

static RIODesc *haret__open(struct r_io_t *io, const char *pathname, int rw, int mode) {
	char *port, *ptr, buf[1024];
	int p, ufd;

	strncpy (buf, pathname, sizeof (buf)-1);
	if (haret__plugin_open (io, pathname)) {
		ptr = buf + 8;
		if (!(port = strchr (ptr, ':'))) {
			eprintf ("haret: wrong url\n");
			return NULL;
		}
		*port = 0;
		p = atoi (port+1);
		if ((ufd = r_socket_connect (ptr, p)) == -1) {
			eprintf ("Cannot connect to '%s' (%d)\n", ptr, p);
			return NULL;
		} else eprintf ("Connected to: %s at port %d\n", ptr, p);
		haret_wait_until_prompt (ufd);
		return r_io_desc_new (&r_io_plugin_haret, ufd, pathname, rw, mode, (void*)(size_t)ufd);
	}
	return NULL;
}

static int haret__system(RIO *io, RIODesc *fd, const char *command) {
	char buf;
	int off = 0;

	r_socket_write (HARET_FD (fd), (char *)command, strlen(command));
	r_socket_write (HARET_FD (fd), "\r\n", 2);
	for (;;) {
		r_socket_read_block (HARET_FD (fd), (unsigned char *)&buf, 1);
		eprintf ("%c", buf);
		switch(off) {
		case 0: if (buf == ')') off =1; break;
		case 1: if (buf == '#') return 0; else off = 0; break;
		}
	}

	return 0;
}

static ut64 haret__lseek(struct r_io_t *io, RIODesc *fd, ut64 offset, int whence) {
	return offset;
}

struct r_io_plugin_t r_io_plugin_haret = {
	.name = "haret",
	.desc = "Attach to Haret WCE application (haret://host:port)",
	.system = haret__system,
	.open = haret__open,
	.read = haret__read,
	.lseek = haret__lseek,
	.write = haret__write,
	.close = haret__close,
	.plugin_open = haret__plugin_open,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_IO,
	.data = &r_io_plugin_haret
};
#endif

#endif
