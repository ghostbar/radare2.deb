/* radare - LGPL - Copyright 2009-2010 nibble<.ds@gmail.com> */

#include <r_types.h>
#include <r_bin.h>

R_API int r_bin_meta_get_line(RBin *bin, ut64 addr, char *file, int len, int *line) {
	if (bin && bin->cur && bin->cur->meta) {
		// XXX quick hack to not show lines out of opened bin
		if (addr >= bin->baddr && addr < (bin->baddr+bin->size))
		if (bin->cur->meta->get_line)
			return bin->cur->meta->get_line (bin, addr, file, len, line);
	}
	return R_FALSE;
}

R_API char *r_bin_meta_get_source_line(RBin *bin, ut64 addr) {
	char file[4096];
	int line;
	char *out = NULL;
	if (r_bin_meta_get_line (bin, addr, file, sizeof (file), &line))
		out = r_file_slurp_line (file, line, 0);
	return out;
}
