/* radare - LGPL - Copyright 2011 pancake<nopcode.org> */

// TODO: implement the rap API in r_socket ?
#include "r_io.h"
#include "r_lib.h"
#include "r_core.h"
#include "r_socket.h"
#include <sys/types.h>

// XXX: if in listener mode we need to use fd or fdlistener to listen or accept
// go fruit yourself
#define ENDIAN (0)
#define RIORAP_FD(x) ((x->data)?(((RIORap*)(x->data))->client):NULL)
#define RIORAP_IS_LISTEN(x) (((RIORap*)(x->data))->listener)
#define RIORAP_IS_VALID(x) ((x) && (x->data) && (x->plugin == &r_io_plugin_rap))

static int rap__write(struct r_io_t *io, RIODesc *fd, const ut8 *buf, int count) {
	RSocket *s = RIORAP_FD (fd);
	int ret;
	ut8 *tmp;
	if (count>RMT_MAX)
		count = RMT_MAX;
	if ((tmp = (ut8 *)malloc (count+5))) {
		eprintf ("rap__write: malloc failed\n");
		return -1;
	}
	tmp[0] = RMT_WRITE;
	r_mem_copyendian ((ut8 *)tmp+1, (ut8*)&count, 4, ENDIAN);
	memcpy (tmp+5, buf, count);

	ret = r_socket_write (s, tmp, count+5);
	if (r_socket_read (s, tmp, 5) != 5) { // TODO read_block?
		eprintf ("rap__write: error\n");
		ret = -1;
	}
	free (tmp);
	// TODO: get reply
        return ret;
}

static boolt rap__accept(RIO *io, RIODesc *desc, int fd) {
	RIORap *rap = desc->data;
	if (rap) {
		rap->client = r_socket_new_from_fd (fd);
		return R_TRUE;
	}
	return R_FALSE;
}

static int rap__read(struct r_io_t *io, RIODesc *fd, ut8 *buf, int count) {
	RSocket *s = RIORAP_FD (fd);
	int ret;
	int i = (int)count;
	ut8 tmp[5];

	if (count>RMT_MAX)
		count = RMT_MAX;
	// send
	tmp[0] = RMT_READ;
	r_mem_copyendian (tmp+1, (ut8*)&count, 4, ENDIAN);
	r_socket_write (s, tmp, 5);

	// recv
	ret = r_socket_read (s, tmp, 5);
	if (ret != 5 || tmp[0] != (RMT_READ|RMT_REPLY)) {
		eprintf ("rap__read: Unexpected rap read reply (%d=0x%02x) expected (%d=0x%02x)\n",
			ret, tmp[0], 2, (RMT_READ|RMT_REPLY));
		return -1;
	}
	r_mem_copyendian ((ut8*)&i, tmp+1, 4, ENDIAN);
	if (i>count) {
		eprintf ("rap__read: Unexpected data size %d\n", i);
		return -1;
	}
	r_socket_read_block (s, buf, i);
	if (count>0 && count<RMT_MAX) {
		//eprintf ("READ %d\n" ,i);
	} else count = 0;
        return count;
}

static int rap__close(RIODesc *fd) {
	int ret = -1;
	if (RIORAP_IS_VALID (fd)) {
		if (RIORAP_FD (fd) != NULL) {
			RIORap *r = fd->data;
			fd->state = R_IO_DESC_TYPE_CLOSED;
			ret = r_socket_close (r->fd);
			ret = r_socket_close (r->client);
			//ret = r_socket_close (r->client);
			free (fd->data);
			fd->data = NULL;
		}
	} else eprintf ("rap__close: fdesc is not a r_io_rap plugin\n");
	return ret;
}

static ut64 rap__lseek(struct r_io_t *io, RIODesc *fd, ut64 offset, int whence) {
	RSocket *s = RIORAP_FD (fd);
	int ret;
	ut8 tmp[10];
	// query
	tmp[0] = RMT_SEEK;
	tmp[1] = (ut8)whence;
	r_mem_copyendian (tmp+2, (ut8*)&offset, 8, ENDIAN);
	r_socket_write (s, &tmp, 10);
	// get reply
	ret = r_socket_read_block (s, (ut8*)&tmp, 9);
	if (ret!=9)
		return -1;
	if (tmp[0] != (RMT_SEEK | RMT_REPLY)) {
		eprintf ("Unexpected lseek reply\n");
		return -1;
	}
	r_mem_copyendian ((ut8 *)&offset, tmp+1, 8, !ENDIAN);
	return offset;
}

static int rap__plugin_open(struct r_io_t *io, const char *pathname) {
	return (!memcmp (pathname, "rap://", 6));
}

static RIODesc *rap__open(struct r_io_t *io, const char *pathname, int rw, int mode) {
	RSocket *rap_fd;
	RIORap *rior;
	const char *ptr;
	char *file, *port;
	char buf[1024];
	int i, p, listenmode;

	if (!rap__plugin_open (io, pathname))
		return NULL;
	ptr = pathname + 6;
	if (!(port = strchr (ptr, ':'))) {
		eprintf ("rap: wrong uri\n");
		return NULL;
	}
	listenmode = (*ptr==':');
	*port++ = 0;
	p = atoi (port);
	if ((file = strchr (port+1, '/'))) {
		*file = 0;
		file++;
	}
	if (listenmode) {
		if (p<=0) {
			eprintf ("rap: cannot listen here. Try rap://:9999\n");
			return NULL;
		}
		//TODO: Handle ^C signal (SIGINT, exit); // ???
		eprintf ("rap: listening at port %s\n", port);
		rior = R_NEW (RIORap);
		rior->listener = R_TRUE;
		rior->client = rior->fd = r_socket_new (R_FALSE);
		if (rior->fd == NULL)
			return NULL;
		if (!r_socket_listen (rior->fd, port, NULL))
			return NULL;
// TODO: listen mode is broken.. here must go the root loop!!
#warning TODO: implement rap:/:9999 listen mode
		return r_io_desc_new (&r_io_plugin_rap, rior->fd->fd, pathname, rw, mode, rior);
	}
	if ((rap_fd = r_socket_new (R_FALSE)) == NULL) {
		eprintf ("Cannot create new socket\n");
		return NULL;
	}
	if (r_socket_connect_tcp (rap_fd, ptr, port)) {
		eprintf ("Cannot connect to '%s' (%d)\n", ptr, p);
		return NULL;
	}
	eprintf ("Connected to: %s at port %s\n", ptr, port);
	rior = R_NEW (RIORap);
	rior->listener = R_FALSE;
	rior->client = rior->fd = rap_fd;
	if (file && *file) {
		// send
		buf[0] = RMT_OPEN;
		buf[1] = rw;
		buf[2] = (ut8)strlen (file);
		memcpy (buf+3, file, buf[2]);
		r_socket_write (rap_fd, buf, 3+buf[2]);
		// read
		eprintf ("waiting... ");
		r_socket_read (rap_fd, (ut8*)buf, 5);
		if (buf[0] != (char)(RMT_OPEN|RMT_REPLY)) {
			free (rior);
			return NULL;
		}
		r_mem_copyendian ((ut8 *)&i, (ut8*)buf+1, 4, ENDIAN);
		if (i>0) eprintf ("ok\n");
	} else return NULL;
	return r_io_desc_new (&r_io_plugin_rap, rior->fd->fd, pathname, rw, mode, rior);
}

static int rap__listener(RIODesc *fd) {
	if (RIORAP_IS_VALID (fd))
		return RIORAP_IS_LISTEN (fd);
	return R_FALSE;
}

static int rap__system(RIO *io, RIODesc *fd, const char *command) {
	RSocket *s = RIORAP_FD (fd);
	ut8 buf[1024];
	char *ptr;
	int ret, i, j = 0;

	// send
	buf[0] = RMT_SYSTEM;
	i = strlen (command);
	r_mem_copyendian (buf+1, (ut8*)&i, 4, ENDIAN);
	memcpy (buf+5, command, i);
	r_socket_write (s, buf, i+5);

	// read
	ret = r_socket_read_block (s, buf, 5);
	if (ret != 5)
		return -1;
	if (buf[0] != (RMT_SYSTEM | RMT_REPLY)) {
		eprintf ("Unexpected system reply\n");
		return -1;
	}
	r_mem_copyendian ((ut8*)&i, buf+1, 4, !ENDIAN);
	if (i == -1)
		return -1;
	if (i>RMT_MAX)
		i = RMT_MAX;
	ptr = (char *)malloc (i);
	if (ptr) {
		r_socket_read_block (s, (ut8*)ptr, i);
		j = write (1, ptr, i);
		free (ptr);
	}
	return i-j;
}

struct r_io_plugin_t r_io_plugin_rap = {
	.name = "rap",
        .desc = "radare network protocol (rap://:port rap://host:port/file)",
	.listener = rap__listener,
        .open = rap__open,
        .close = rap__close,
	.read = rap__read,
        .plugin_open = rap__plugin_open,
	.lseek = rap__lseek,
	.system = rap__system,
	.write = rap__write,
	.accept = rap__accept,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_IO,
	.data = &r_io_plugin_rap
};
#endif
