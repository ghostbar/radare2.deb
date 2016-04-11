/* radare - LGPL - Copyright 2009-2016 nibble, pancake */

#include <r_types.h>
#include <r_util.h>
#include <r_lib.h>
#include <r_bin.h>
#include "mach0/dyldcache.h"

static RBinXtrData * extract(RBin *bin, int idx);
static RList * extractall(RBin *bin);
static RBinXtrData * oneshot(RBin *bin, const ut8 *buf, ut64 size, int idx);
static int check_bytes(const ut8* bytes, ut64 sz);
static RList * oneshotall(RBin *bin, const ut8 *buf, ut64 size);
static int free_xtr (void *xtr_obj);

static int check(RBin *bin) {
	int size = 0, ret = false;
	ut8 *filebuf = (ut8*)r_file_slurp_range (bin->file, 0, 4, &size);
	ret = check_bytes (filebuf, size);
	free (filebuf);
	return ret;
}

static int check_bytes(const ut8* buf, ut64 sz) {
	return (buf && sz > 3 && !memcmp (buf, "\x64\x79\x6c\x64", 4));
}

// TODO: destroy must be void?
static int destroy(RBin *bin) {
	return free_xtr (bin->cur->xtr_obj);
}

static int free_xtr (void *xtr_obj) {
	r_bin_dyldcache_free ((struct r_bin_dyldcache_obj_t*)xtr_obj);
	return true;
}

static bool load(RBin *bin) {
	if (!bin || !bin->cur) 
	    	return false;
	bin->cur->xtr_obj = r_bin_dyldcache_new (bin->cur->file);
	if (!bin->file)
	    	bin->file = bin->cur->file;
	return bin->cur->xtr_obj? true : false;
}

static RList * extractall(RBin *bin) {
	RList *result = NULL;
	int nlib, i = 0;
	RBinXtrData *data = extract (bin, i);
	if (!data) return result;

	// XXX - how do we validate a valid nlib?
	nlib = data->file_count;

	result = r_list_newf (r_bin_xtrdata_free);
	r_list_append (result, data);
	for (i = 1; data && i < nlib; i++) {
		data = extract (bin, i);
		r_list_append (result, data);
	}
	return result;
}

static RBinXtrData * extract(RBin *bin, int idx) {
	int nlib = 0;
	RBinXtrData *res = NULL;
	struct r_bin_dyldcache_lib_t *lib = r_bin_dyldcache_extract (
		(struct r_bin_dyldcache_obj_t*)bin->cur->xtr_obj, idx, &nlib);
	if (lib) {
		res = r_bin_xtrdata_new (NULL, NULL, lib->b,
			lib->offset, lib->size, nlib);
		r_buf_free (lib->b);
		free (lib);
	}
	return res;
}

static RBinXtrData *oneshot(RBin *bin, const ut8* buf, ut64 size, int idx) {
	RBinXtrData *res = NULL;
	struct r_bin_dyldcache_obj_t *xtr_obj;
	struct r_bin_dyldcache_lib_t *lib;
	int nlib = 0;
	if (!bin->file) {
		if (!load (bin))
		    	return NULL;
	}
	//XXX why allocate again and again? use bin->cur->xtr_obj though is producing uaf review
	xtr_obj = r_bin_dyldcache_from_bytes_new (buf, size);
	lib = r_bin_dyldcache_extract (xtr_obj, idx, &nlib);
	if (!lib) {
		free_xtr (xtr_obj);
		return NULL;
	}
	res = r_bin_xtrdata_new (xtr_obj, free_xtr, lib->b, lib->offset, lib->size, nlib);
	r_buf_free (lib->b);
	free (lib);
	return res;
}

static RList * oneshotall(RBin *bin, const ut8* buf, ut64 size) {
	RBinXtrData *data = NULL;
	RList *res = NULL;
	int nlib, i = 0;
	if (!bin->file) {
		if (!load (bin))
		    	return NULL;
	}
	data = oneshot (bin, buf, size, i);
	if (!data) return res;
	// XXX - how do we validate a valid nlib?
	nlib = data->file_count;
	res = r_list_newf (r_bin_xtrdata_free);
	r_list_append (res, data);
	for (i = 1; data && i < nlib; i++) {
		data = NULL;
		data = oneshot (bin, buf, size, i);
		r_list_append (res, data);
	}

	return res;
}

struct r_bin_xtr_plugin_t r_bin_xtr_plugin_xtr_dyldcache = {
	.name = "xtr_dyldcache",
	.desc = "dyld cache bin extractor plugin",
	.license = "LGPL3",
	.check = &check,
	.load = &load,
	.extract = &extract,
	.extractall = &extractall,
	.destroy = &destroy,
	.extract_from_bytes = &oneshot,
	.extractall_from_bytes = &oneshotall,
	.free_xtr = &free_xtr,
	.check_bytes = &check_bytes,
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_BIN_XTR,
	.data = &r_bin_xtr_plugin_xtr_dyldcache,
	.version = R2_VERSION
};
#endif
