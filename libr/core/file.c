/* radare - LGPL - Copyright 2009-2011 pancake<nopcode.org> */

#include <r_core.h>

R_API ut64 r_core_file_resize(struct r_core_t *core, ut64 newsize) {
	if (newsize==0 && core->file)
		return core->file->size;
	return 0LL;
}

// TODO: add support for args
R_API int r_core_file_reopen(RCore *core, const char *args) {
	char *path;
	RCoreFile *file;
	int ret = R_FALSE;
	int newpid, perm;
	if (!core->file) {
		eprintf ("No file opened to reopen\n");
		return R_FALSE;
	}
	newpid = core->file->fd->fd;
	perm = core->file->rwx;
	ut64 addr = 0; // XXX ? check file->map ?
	path = strdup (core->file->uri);
	if (r_config_get_i (core->config, "cfg.debug"))
		r_debug_kill (core->dbg, R_FALSE, 9); // KILL
	r_core_file_close (core, core->file);
	file = r_core_file_open (core, path, perm, addr);
	if (file) {
		eprintf ("File %s reopened\n", path);
		ret = R_TRUE;
	}
	// close old file
	r_core_file_close_fd (core, newpid);
	// TODO: in debugger must select new PID
	if (r_config_get_i (core->config, "cfg.debug")) {
		newpid = core->file->fd->fd;
		r_debug_select (core->dbg, newpid, newpid);
	}
	free (path);
	return ret;
}

// NOTE: probably not all environment vars takes sesnse
// because they can be replaced by commands in the given
// command.. we should only expose the most essential and
// unidirectional ones.
R_API void r_core_sysenv_help() {
	r_cons_printf (
	"Usage: !<cmd>\n"
	"  !ls                   ; execute 'ls' in shell\n"
	"  .!rabin2 -ri ${FILE}  ; run each output line as a r2 cmd\n"
	"  !echo $SIZE           ; display file size\n"
	"Environment:\n"
	"  FILE       file name\n"
	"  SIZE       file size\n"
	"  OFFSET     10base offset 64bit value\n"
	"  XOFFSET    same as above, but in 16 base\n"
	"  BSIZE      block size\n"
	"  ENDIAN     'big' or 'little'\n"
	"  ARCH       value of asm.arch\n"
	"  DEBUG      debug mode enabled? (1,0)\n"
	"  IOVA       is io.va true? virtual addressing (1,0)\n"
	"  BLOCK      TODO: dump current block to tmp file\n"
	"  BYTES      TODO: variable with bytes in curblock\n"
	);
}

R_API void r_core_sysenv_end(RCore *core, const char *cmd) {
	// TODO: remove tmpfilez
	if (strstr (cmd, "BLOCK")) {
		// remove temporary BLOCK file
	}
}

R_API char *r_core_sysenv_begin(RCore *core, const char *cmd) {
	char buf[64], *ret;
#if DISCUSS
	// EDITOR      cfg.editor (vim or so)
	CURSOR      cursor position (offset from curseek)
	COLOR       scr.color?1:0
	VERBOSE     cfg.verbose
	// only if cmd matches BYTES or BLOCK ?
	BYTES       hexpairs of current block
	BLOCK       temporally file with contents of current block
#endif
	if (!core->file)
		return NULL;
	ret = strdup (cmd);
	if (strstr (cmd, "BLOCK")) {
		// replace BLOCK in RET string
	}
	if (core->file->filename)
		r_sys_setenv ("FILE", core->file->filename);
	snprintf (buf, sizeof (buf), "%"PFMT64d, core->offset);
	r_sys_setenv ("OFFSET", buf);
	snprintf (buf, sizeof (buf), "0x%08"PFMT64x, core->offset);
	r_sys_setenv ("XOFFSET", buf);
	snprintf (buf, sizeof (buf), "%"PFMT64d, core->file->size);
	r_sys_setenv ("SIZE", buf);
	r_sys_setenv ("ENDIAN", core->assembler->big_endian?"big":"little");
	snprintf (buf, sizeof (buf), "%d", core->blocksize);
	r_sys_setenv ("BSIZE", buf);
	r_sys_setenv ("ARCH", r_config_get (core->config, "asm.arch"));
	r_sys_setenv ("DEBUG", r_config_get_i (core->config, "cfg.debug")?"1":"0");
	r_sys_setenv ("IOVA", r_config_get_i (core->config, "io.va")?"1":"0");
	return ret;
}

R_API int r_core_bin_load(RCore *r, const char *file) {
	int va = r->io->va || r->io->debug;

	if (file == NULL) {
		if (r->file == NULL)
			return R_FALSE;
		file = r->file->filename;
	}
	if (r_bin_load (r->bin, file, R_FALSE)) {
		if (r->bin->narch>1) {
			int i;
			eprintf ("NOTE: Fat binary found. Selected sub-bin is: -a %s -b %d\n",
					r->assembler->cur->arch, r->assembler->bits);
			eprintf ("NOTE: Use -a and -b to select sub binary in fat binary\n");
			for (i=0; i<r->bin->narch; i++) {
				r_bin_select_idx (r->bin, i);
				if (r->bin->curarch.info == NULL) {
					eprintf ("No extract info found.\n");
				} else {
					eprintf ("  $ r2 -a %s -b %d %s  # 0x%08"PFMT64x"\n", 
							r->bin->curarch.info->arch,
							r->bin->curarch.info->bits,
							r->bin->curarch.file,
							r->bin->curarch.offset);
				}
			}
		}
		r_bin_select (r->bin, r->assembler->cur->arch, r->assembler->bits, NULL);//"x86_32");
	} else if (!r_bin_load (r->bin, file, R_TRUE))
		return R_FALSE;
	r->file->obj = r_bin_get_object (r->bin, 0);
	if (r->file->obj->info != NULL) {
		r_config_set_i (r->config, "io.va", r->file->obj->info->has_va);
	} else r_config_set_i (r->config, "io.va", 0);
	{
		ut64 offset = r_bin_get_offset (r->bin);
		r_core_bin_info (r, R_CORE_BIN_ACC_ALL, R_CORE_BIN_SET, va, NULL, offset);
	}
	return R_TRUE;
}

R_API RCoreFile *r_core_file_open(RCore *r, const char *file, int mode, ut64 loadaddr) {
	RCoreFile *fh;
	const char *cp;
	char *p;
	RIODesc *fd = r_io_open (r->io, file, mode, 0644);
	if (fd == NULL)
		return NULL;
	if (r_io_is_listener (r->io)) {
		r_core_serve (r, fd);
		return NULL;
	}

	fh = R_NEW (RCoreFile);
	fh->fd = fd;
	fh->map = NULL;
	fh->uri = strdup (file);
	fh->filename = strdup (fh->uri);
	p = strstr (fh->filename, "://");
	if (p != NULL) {
		char *s = strdup (p+3);
		free (fh->filename);
		fh->filename = s;
	}
	fh->rwx = mode;
	r->file = fh;
	r->io->plugin = fd->plugin;
	fh->size = r_io_size (r->io);
	r_list_append (r->files, fh);

//	r_core_bin_load (r, fh->filename);
	r_core_block_read (r, 0);
	cp = r_config_get (r->config, "cmd.open");
	if (cp && *cp)
		r_core_cmd (r, cp, 0);
	r_config_set (r->config, "file.path", file);
	r_config_set_i (r->config, "zoom.to", loadaddr+fh->size);
	fh->map = r_io_map_add (r->io, fh->fd->fd, mode, 0, loadaddr, fh->size);
	return fh;
}

R_API void r_core_file_free(RCoreFile *cf) {
	free (cf->uri);
	cf->uri = NULL;
	free (cf->filename);
	cf->filename = NULL;
	cf->fd = NULL;
}

R_API int r_core_file_close(struct r_core_t *r, struct r_core_file_t *fh) {
	int ret = r_io_close (r->io, fh->fd);
	// TODO: free fh->obj
	//r_list_delete (fh);
	//list_del (&(fh->list));
	// TODO: set previous opened file as current one
	return ret;
}

R_API RCoreFile *r_core_file_get_fd(RCore *core, int fd) {
	RCoreFile *file;
	RListIter *iter;
	r_list_foreach (core->files, iter, file) {
		if (file->fd->fd == fd)
			return file;
	}
	return NULL;
}

R_API int r_core_file_list(RCore *core) {
	int count = 0;
	RCoreFile *f;
	RListIter *iter;
	r_list_foreach (core->files, iter, f) {
		if (f->map)
			eprintf ("%c %d %s 0x%"PFMT64x"\n",
				core->io->raised == f->fd->fd?'*':' ',
				f->fd->fd, f->uri, f->map->from);
		else eprintf ("  %d %s\n", f->fd->fd, f->uri);
		count++;
	}
	return count;
}

R_API int r_core_file_close_fd(RCore *core, int fd) {
	RCoreFile *file;
	RListIter *iter;
	r_list_foreach (core->files, iter, file) {
		if (file->fd->fd == fd) {
			r_io_close (core->io, file->fd);
			r_list_delete (core->files, iter);
			if (r_list_empty (core->files))
				core->file = NULL;
			return R_TRUE;
		}
	}
	return R_FALSE;
}

R_API int r_core_hash_load(RCore *r, const char *file) {
	ut8 *buf = NULL;
	int i, buf_len = 0;
	const ut8 *md5, *sha1;
	char hash[128], *p;
	RHash *ctx;
	ut64 limit;

	limit = r_config_get_i (r->config, "cfg.hashlimit");
	if (r->file->size > limit)
		return R_FALSE;
	buf = (ut8*)r_file_slurp (file, &buf_len);
	if (buf==NULL)
		return R_FALSE;
	ctx = r_hash_new (R_TRUE, R_HASH_MD5);
	md5 = r_hash_do_md5 (ctx, buf, buf_len);
	p = hash;
	for (i=0; i<R_HASH_SIZE_MD5; i++) {
		sprintf (p, "%02x", md5[i]);
		p += 2;
	}
	*p = 0;
	r_config_set (r->config, "file.md5", hash);
	r_hash_free (ctx);
	ctx = r_hash_new (R_TRUE, R_HASH_SHA1);
	sha1 = r_hash_do_sha1 (ctx, buf, buf_len);
	p = hash;
	for (i=0; i<R_HASH_SIZE_SHA1;i++) {
		sprintf (p, "%02x", sha1[i]);
		p+=2;
	}
	*p=0;
	r_config_set (r->config, "file.sha1", hash);
	r_hash_free (ctx);
	free (buf);
	return R_TRUE;
}
