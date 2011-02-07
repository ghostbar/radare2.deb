/* radare - LGPL - Copyright 2008-2010 pancake<nopcode.org> nibble <.ds@gmail.com> */

#include "r_io.h"

R_API void r_io_section_init(RIO *io) {
	io->enforce_rwx = 0; // do not enforce RWX section permissions by default
	io->enforce_seek = 0; // do not limit seeks out of the file by default
	INIT_LIST_HEAD (&(io->sections));
}

R_API void r_io_section_add(RIO *io, ut64 offset, ut64 vaddr, ut64 size, ut64 vsize, int rwx, const char *name) {
	RIOSection *s = R_NEW (RIOSection);
	s->offset = offset;
	s->vaddr = vaddr;
	s->size = size;
	s->vsize = vsize;
	s->rwx = rwx;
	if (name) strncpy (s->name, name, sizeof (s->name));
	else *s->name = '\0';
	list_add (&(s->list), &io->sections);
}

R_API RIOSection *r_io_section_get_i(RIO *io, int idx) {
	int i = 0;
	struct list_head *pos;
	list_for_each_prev (pos, &io->sections) {
		RIOSection *s = (RIOSection *)list_entry(pos, RIOSection, list);
		if (i == idx)
			return s;
		i++;
	}
	return NULL;
}

R_API int r_io_section_rm(RIO *io, int idx) {
	RIOSection *s = r_io_section_get_i (io, idx);
	if (s != NULL) {
		list_del ((&s->list));
		free (s);
		return 1;
	}
	return 0;
}

// TODO: implement as callback
R_API void r_io_section_list(RIO *io, ut64 offset, int rad) {
	int i = 0;
	struct list_head *pos;

	if (io->va || io->debug)
		offset = r_io_section_vaddr_to_offset (io, offset);
	list_for_each_prev(pos, &io->sections) {
		RIOSection *s = (RIOSection *)list_entry(pos, RIOSection, list);
		if (rad) io->printf ("S 0x%08"PFMT64x" 0x%08"PFMT64x" 0x%08"PFMT64x" 0x%08"PFMT64x" %s %d\n",
			s->offset, s->vaddr, s->size, s->vsize, s->name, s->rwx);
		else io->printf ("[%02d] %c 0x%08"PFMT64x" %s va=0x%08"PFMT64x" sz=0x%08"PFMT64x" vsz=%08"PFMT64x" %s\n",
			i, (offset>=s->offset && offset<s->offset+s->size)?'*':'.', 
			s->offset, r_str_rwx_i (s->rwx), s->vaddr, s->size, s->vsize, s->name);
		i++;
	}
}

/* TODO: move to print ??? support pretty print of ranges following an array of offsetof */
R_API void r_io_section_list_visual(RIO *io, ut64 seek, ut64 len) {
	struct list_head *pos;
	ut64 min = -1;
	ut64 max = -1;
	ut64 mul;
	int j, i, width = 50; //config.width-30;

	seek = (io->va || io->debug) ? r_io_section_vaddr_to_offset (io, seek) : seek;
	list_for_each (pos, &io->sections) {
		RIOSection *s = (RIOSection *)list_entry(pos, RIOSection, list);
		if (min == -1 || s->offset < min)
			min = s->offset;
		if (max == -1 || s->offset+s->size > max)
			max = s->offset+s->size;
	}

	mul = (max-min) / width;
	if (min != -1 && mul != 0) {
		i = 0;
		list_for_each_prev (pos, &io->sections) {
			RIOSection *s = (RIOSection *)list_entry (pos, RIOSection, list);
			io->printf ("%02d%c 0x%08"PFMT64x" |",
					i, (seek>=s->offset && seek<s->offset+s->size)?'*':' ', s->offset);
			for (j=0; j<width; j++) {
				if ((j*mul)+min >= s->offset && (j*mul)+min <=s->offset+s->size)
					io->printf("#");
				else
					io->printf("-");
			}
			io->printf ("| 0x%08"PFMT64x" %s\n", s->offset+s->size, s->name);
			i++;
		}
		/* current seek */
		if (i>0 && len != 0) {
			io->printf ("=>  0x%08"PFMT64x" |", seek);
			for(j=0;j<width;j++) {
				io->printf (
					((j*mul)+min >= seek &&
					 (j*mul)+min <= seek+len)
					?"#":"-");
			}
			io->printf ("| 0x%08"PFMT64x"\n", seek+len);
		}
	}
}

R_API RIOSection *r_io_section_get(RIO *io, ut64 offset) {
	struct list_head *pos;
	list_for_each (pos, &io->sections) {
		RIOSection *s = (RIOSection *)list_entry(pos, RIOSection, list);
		if (offset >= s->offset && offset <= s->offset + s->size)
			return s;
	}
	return NULL;
}

R_API ut64 r_io_section_get_offset(RIO *io, ut64 offset) {
	RIOSection *s = r_io_section_get(io, offset);
	return s?s->offset:-1;
}

R_API ut64 r_io_section_get_vaddr(RIO *io, ut64 offset) {
	RIOSection *s = r_io_section_get (io, offset);
	return s?s->vaddr:-1;
}

// TODO: deprecate
R_API int r_io_section_get_rwx(RIO *io, ut64 offset) {
	RIOSection *s = r_io_section_get (io, offset);
eprintf ("r_io_section_get_rwx: must be deprecated\n");
	return s?s->rwx:R_IO_READ|R_IO_WRITE|R_IO_EXEC;
}

R_API int r_io_section_overlaps(RIO *io, RIOSection *s) {
	int i = 0;
	struct list_head *pos;
	list_for_each_prev(pos, &io->sections) {
		RIOSection *s2 = (RIOSection *)list_entry(pos, RIOSection, list);
		if (s != s2) {
			if (s->offset >= s2->offset) {
				if (s2->offset+s2->size < s->offset)
					return i;
			} else {
				if (s->offset+s->size < s2->offset)
					return i;
			}
		}
		i++;
	}
	return -1;
}

R_API ut64 r_io_section_vaddr_to_offset(RIO *io, ut64 vaddr) {
	struct list_head *pos;
	list_for_each_prev (pos, &io->sections) {
		RIOSection *s = (RIOSection *)list_entry (pos, RIOSection, list);
		if (vaddr >= s->vaddr && vaddr < s->vaddr + s->vsize)
			return (vaddr - s->vaddr + s->offset); 
	}
	return -1;
}

R_API ut64 r_io_section_offset_to_vaddr(RIO *io, ut64 offset) {
	struct list_head *pos;
	list_for_each_prev(pos, &io->sections) {
		RIOSection *s = (RIOSection *)list_entry(pos, RIOSection, list);
		if (offset >= s->offset && offset < s->offset + s->size)
			return (s->vaddr + offset - s->offset); 
	}
	return -1;
}
