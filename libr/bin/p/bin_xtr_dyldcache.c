/* radare - LGPL - Copyright 2009-2016 nibble, pancake */

#include <r_types.h>
#include <r_util.h>
#include <r_lib.h>
#include <r_bin.h>
#include "mach0/dyldcache.h"
#include "mach0/mach0.h"

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

static int free_xtr(void *xtr_obj) {
	r_bin_dyldcache_free ((struct r_bin_dyldcache_obj_t*)xtr_obj);
	return true;
}

static bool load(RBin *bin) {
	if (!bin || !bin->cur)
	    	return false;
	if (!bin->cur->xtr_obj) {
		bin->cur->xtr_obj = r_bin_dyldcache_new (bin->cur->file);
	}
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


static inline void fill_metadata_info_from_hdr(RBinXtrMetadata *meta, struct MACH0_(mach_header) *hdr) {
	meta->arch = MACH0_(get_cputype_from_hdr) (hdr);
	meta->bits = MACH0_(get_bits_from_hdr) (hdr);
	meta->machine = MACH0_(get_cpusubtype_from_hdr) (hdr);
	meta->type = MACH0_(get_filetype_from_hdr) (hdr);
}

static RBinXtrData *extract(RBin *bin, int idx) {
	int nlib = 0;
	RBinXtrData *res = NULL;
	char *libname;
	struct MACH0_(mach_header) *hdr;
	struct r_bin_dyldcache_lib_t *lib = r_bin_dyldcache_extract (
		(struct r_bin_dyldcache_obj_t*)bin->cur->xtr_obj, idx, &nlib);

	if (lib) {
		RBinXtrMetadata *metadata = R_NEW0(RBinXtrMetadata);
		if (!metadata) {
			free (lib);
			return NULL;
		}
		hdr = MACH0_(get_hdr_from_bytes) (lib->b);
		if (!hdr) {
			free (lib);
			free (hdr);
			return NULL;
		}
		fill_metadata_info_from_hdr (metadata, hdr);
		r_bin_dydlcache_get_libname (lib, &libname);
		metadata->libname = strdup (libname);

		res = r_bin_xtrdata_new (lib->b, lib->offset, lib->size, nlib, metadata, bin->sdb);
		r_buf_free (lib->b);
		free (lib);
		free (hdr);
	}
	return res;
}

static RBinXtrData *oneshot(RBin *bin, const ut8* buf, ut64 size, int idx) {
	RBinXtrData *res = NULL;
	struct r_bin_dyldcache_obj_t *xtr_obj;
	struct r_bin_dyldcache_lib_t *lib;
	int nlib = 0;
	char *libname;
	struct MACH0_(mach_header) *hdr;

	if (!load (bin)) {
		return NULL;
	}

	xtr_obj = bin->cur->xtr_obj;
	lib = r_bin_dyldcache_extract (xtr_obj, idx, &nlib);
	if (!lib) {
		free_xtr (xtr_obj);
		bin->cur->xtr_obj = NULL;
		return NULL;
	}
	RBinXtrMetadata *metadata = R_NEW0 (RBinXtrMetadata);
	if (!metadata) {
		free (lib);
		return NULL;
	}
	hdr = MACH0_(get_hdr_from_bytes) (lib->b);
	if (!hdr) {
		free (lib);
		free (hdr);
		return NULL;
	}
	fill_metadata_info_from_hdr (metadata, hdr);
	r_bin_dydlcache_get_libname (lib, &libname);
	metadata->libname = strdup (libname);

	res = r_bin_xtrdata_new (lib->b, lib->offset, lib->b->length, nlib, metadata, bin->sdb);
	r_buf_free (lib->b);
	free (hdr);
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
