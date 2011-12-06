/* radare - LGPL - Copyright 2008-2011 nibble<.ds@gmail.com>, pancake<nopcode.org> */
// TODO: review the rest of strtab index out of range
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <r_types.h>
#include <r_util.h>
#include "elf.h"

static ut64 Elf_(r_bin_elf_get_section_offset)(struct Elf_(r_bin_elf_obj_t) *bin, const char *section_name);

static inline int __strnlen(const char *str, int len) {
	int l = 0;
	while (*str && --len) {
		str++;
		l++;
	}
	return l+1;
}

static int Elf_(r_bin_elf_init_ehdr)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut8 e_ident[16];
	int len;
	if (r_buf_read_at (bin->b, 0, e_ident, 16) == -1) {
		eprintf ("Error: read (magic)\n");
		return R_FALSE;
	}
	bin->endian = (e_ident[EI_DATA] == ELFDATA2MSB)?
		LIL_ENDIAN: !LIL_ENDIAN;
	len = r_buf_fread_at (bin->b, 0, (ut8*)&bin->ehdr,
#if R_BIN_ELF64
		bin->endian?"16c2SI3LI6S":"16c2si3li6s",
#else
		bin->endian?"16c2S5I6S":"16c2s5i6s",
#endif
			1);
	if (len == -1) {
		eprintf ("Error: read (ehdr)\n");
		return R_FALSE;
	}
	if (strncmp ((char *)bin->ehdr.e_ident, ELFMAG, SELFMAG))
		return R_FALSE;
	return R_TRUE;
}

static int Elf_(r_bin_elf_init_phdr)(struct Elf_(r_bin_elf_obj_t) *bin) {
	int phdr_size, len;
	if (bin->ehdr.e_phnum == 0) {
		return R_FALSE;
	}
	if (bin->phdr) return R_TRUE;
	phdr_size = bin->ehdr.e_phnum * sizeof (Elf_(Phdr));
	if ((bin->phdr = (Elf_(Phdr) *)malloc (phdr_size)) == NULL) {
		perror ("malloc (phdr)");
		return R_FALSE;
	}
	len = r_buf_fread_at (bin->b, bin->ehdr.e_phoff, (ut8*)bin->phdr,
		#if R_BIN_ELF64
		bin->endian?"2I6L":"2i6l",
		#else
		bin->endian?"8I":"8i",
		#endif
		bin->ehdr.e_phnum);
	if (len == -1) {
		eprintf ("Error: read (phdr)\n");
		R_FREE (bin->phdr);
		return R_FALSE;
	}
	return R_TRUE;
}

static int Elf_(r_bin_elf_init_shdr)(struct Elf_(r_bin_elf_obj_t) *bin) {
	int len, shdr_size;
	if (bin->shdr) return R_TRUE;
	shdr_size = bin->ehdr.e_shnum * sizeof (Elf_(Shdr));
	if ((bin->shdr = (Elf_(Shdr) *)malloc (shdr_size)) == NULL) {
		perror ("malloc (shdr)");
		return R_FALSE;
	}
	len = r_buf_fread_at (bin->b, bin->ehdr.e_shoff, (ut8*)bin->shdr,
#if R_BIN_ELF64
			bin->endian?"2I4L2I2L":"2i4l2i2l",
#else
			bin->endian?"10I":"10i",
#endif
			bin->ehdr.e_shnum);
	if (len == -1) {
		eprintf ("Error: read (shdr)\n");
		R_FREE (bin->shdr);
		return R_FALSE;
	}
	return R_TRUE;
}

static int Elf_(r_bin_elf_init_strtab)(struct Elf_(r_bin_elf_obj_t) *bin) {
	int sz;
	if (bin->strtab) return R_FALSE;
	bin->shstrtab_section =
	bin->strtab_section = &bin->shdr[bin->ehdr.e_shstrndx];
	bin->shstrtab_size =
	bin->strtab_size = bin->strtab_section->sh_size;
	sz = sizeof (struct r_bin_elf_section_t) + bin->strtab_section->sh_size;
	if ((bin->strtab = (char *)malloc (sz)) == NULL) {
		perror ("malloc");
		bin->shstrtab = NULL;
		return R_FALSE;
	}
	memset (bin->strtab, 0, sz);
	bin->shstrtab = bin->strtab;
	if (r_buf_read_at (bin->b, bin->strtab_section->sh_offset, (ut8*)bin->strtab,
				bin->strtab_section->sh_size) == -1) {
		eprintf ("Error: read (strtab)\n");
		R_FREE (bin->strtab);
		bin->shstrtab = NULL;
		return R_FALSE;
	}
	return R_TRUE;
}

#if 0
static ut64 Elf_(r_bin_elf_get_section_size)(struct Elf_(r_bin_elf_obj_t) *bin, const char *section_name) {
	int i;
	if (!bin->shdr || !bin->strtab)
		return -1;
	for (i = 0; i < bin->ehdr.e_shnum; i++) {
		if (bin->shdr[i].sh_name > bin->shstrtab_section->sh_size)
			continue;
		if (!strcmp (&bin->shstrtab[bin->shdr[i].sh_name], section_name))
			return (ut64)bin->shdr[i].sh_size;
	}
	return -1;
}

static int Elf_(r_bin_elf_init_strtab)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut64 size;
	if (!bin->shstrtab)
		return  R_FALSE;
	bin->shstrtab_section = &bin->shdr[bin->ehdr.e_shstrndx];
	bin->shstrtab_size = size = 4096; //bin->shstrtab_section->sh_size;

	if ((bin->shstrtab = (char *)malloc (size)) == NULL) {
		perror ("malloc");
		return R_FALSE;
	}
//eprintf ("BUFBUFBUF %p\n",
	eprintf ("%p off=%x buf=%p sz=%llx\n", 
bin->b, bin->shstrtab_section->sh_offset, (ut8*)bin->shstrtab, size);
	if (r_buf_read_at (bin->b, (ut64)bin->shstrtab_section->sh_offset, (ut8*)bin->shstrtab, size) == -1) {
		eprintf ("Error: read (strtab)\n");
		R_FREE (bin->strtab);
		return R_FALSE;
	}
write (1, "\n\n", 2);
write (1, bin->shstrtab, size);
write (1, "\n\n", 2);

	size = Elf_(r_bin_elf_get_section_size)(bin, ".strtab");
	if (size != UT64_MAX) {
		ut64 off = Elf_(r_bin_elf_get_section_offset)(bin, ".strtab");
		bin->strtab_size = size;
		free (bin->strtab);
		if ((bin->strtab = (char *)malloc (size+1)) == NULL) {
			perror ("malloc");
			return R_FALSE;
		}
		if (r_buf_read_at (bin->b, off, (ut8*)bin->strtab, size) == -1) {
			eprintf ("Error: read (strtab)\n");
			R_FREE (bin->strtab);
			return R_FALSE;
		}
	}
	return R_TRUE;
}
#endif

static int Elf_(r_bin_elf_init)(struct Elf_(r_bin_elf_obj_t) *bin) {
	bin->phdr = NULL;
	bin->shdr = NULL;
	bin->strtab = NULL;
	bin->strtab_size = 0;
	bin->strtab_section = NULL;
	if (!Elf_(r_bin_elf_init_ehdr) (bin)) {
		eprintf ("Warning: File is not ELF\n");
		return R_FALSE;
	}
	Elf_(r_bin_elf_init_phdr) (bin);
	//if (!Elf_(r_bin_elf_init_phdr) (bin))
		//eprintf ("Warning: Cannot initialize program headers\n");
	if (!Elf_(r_bin_elf_init_shdr) (bin))
		eprintf ("Warning: Cannot initialize section headers\n");
	if (!Elf_(r_bin_elf_init_strtab) (bin))
		eprintf ("Warning: Cannot initialize strings table\n");
	bin->baddr = Elf_(r_bin_elf_get_baddr) (bin);

	return R_TRUE;
}

static ut64 Elf_(r_bin_elf_get_section_offset)(struct Elf_(r_bin_elf_obj_t) *bin, const char *section_name) {
	int i;
	if (!bin->shdr || !bin->strtab)
		return -1;
	for (i = 0; i < bin->ehdr.e_shnum; i++) {
		if (bin->shdr[i].sh_name > bin->shstrtab_section->sh_size)
			continue;
		if (!strcmp (&bin->shstrtab[bin->shdr[i].sh_name], section_name))
			return (ut64)bin->shdr[i].sh_offset;
	}
	return -1;
}

static ut64 Elf_(r_bin_elf_get_section_addr)(struct Elf_(r_bin_elf_obj_t) *bin, const char *section_name) {
	int i;
	if (!bin->shdr || !bin->strtab)
		return -1;
	for (i = 0; i < bin->ehdr.e_shnum; i++) {
		if (bin->shdr[i].sh_name > bin->shstrtab_section->sh_size)
			continue;
		if (!strcmp (&bin->strtab[bin->shdr[i].sh_name], section_name))
			return (ut64)bin->shdr[i].sh_addr;
	}
	return -1;
}

static ut64 Elf_(get_import_addr)(struct Elf_(r_bin_elf_obj_t) *bin, int sym) {
	Elf_(Rel) *rel = NULL;
	Elf_(Addr) plt_sym_addr;
	ut64 got_addr, got_offset;
	int i, j, k, tsize, len;
	
	if (!bin->shdr || !bin->strtab)
		return -1;
	if ((got_offset = Elf_ (r_bin_elf_get_section_offset) (bin, ".got")) == -1 &&
		(got_offset = Elf_ (r_bin_elf_get_section_offset) (bin, ".got.plt")) == -1)
		return -1;
	if ((got_addr = Elf_ (r_bin_elf_get_section_addr) (bin, ".got")) == -1 &&
		(got_addr = Elf_ (r_bin_elf_get_section_addr) (bin, ".got.plt")) == -1)
		return -1;
	for (i = 0; i < bin->ehdr.e_shnum; i++) {
		if (!strcmp (&bin->strtab[bin->shdr[i].sh_name], ".rel.plt"))
			tsize = sizeof (Elf_(Rel));
		else if (!strcmp (&bin->strtab[bin->shdr[i].sh_name], ".rela.plt"))
			tsize = sizeof (Elf_(Rela));
		else continue;

		free (rel);
		if ((rel = (Elf_(Rel) *)malloc ((int)(bin->shdr[i].sh_size / tsize) * sizeof (Elf_(Rel)))) == NULL) {
			perror ("malloc (rel)");
			return -1;
		}
		for (j = k = 0; j < bin->shdr[i].sh_size; j += tsize, k++) {
			len = r_buf_fread_at (bin->b, bin->shdr[i].sh_offset + j, (ut8*)&rel[k],
#if R_BIN_ELF64
					bin->endian?"2L":"2l",
#else
					bin->endian?"2I":"2i",
#endif
					1);
			if (len == -1) {
				eprintf ("Error: read (rel)\n");
				free (rel);
				return -1;
			}
		}
		for (j = k = 0; j < bin->shdr[i].sh_size; j += tsize, k++) {
			if (ELF_R_SYM (rel[k].r_info) == sym) {
				if (r_buf_read_at (bin->b, rel[k].r_offset-got_addr+got_offset,
							(ut8*)&plt_sym_addr, sizeof (Elf_(Addr))) == -1) {
					eprintf ("Error: read (got)\n");
					return UT64_MAX;
				}
				free (rel);
				return (ut64)(plt_sym_addr - 6);
			}
		}
		break;
	}
	free (rel);
	return UT64_MAX;
}

ut64 Elf_(r_bin_elf_get_baddr)(struct Elf_(r_bin_elf_obj_t) *bin) {
	int i;
	if (!bin->phdr)
		return 0;
	for (i = 0; i < bin->ehdr.e_phnum; i++)
		if (bin->phdr[i].p_type == PT_LOAD && bin->phdr[i].p_offset == 0)
			return (ut64)bin->phdr[i].p_vaddr;
	return 0;
}

ut64 Elf_(r_bin_elf_get_init_offset)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut64 entry = Elf_(r_bin_elf_get_entry_offset) (bin);
	ut8 buf[512];

	if (r_buf_read_at (bin->b, entry+16, buf, sizeof (buf)) == -1) {
		eprintf ("Error: read (entry)\n");
		return 0;
	}
	if (buf[0] == 0x68) { // push // x86 only
		memmove (buf, buf+1, 4);
		return (ut64)((int)(buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24)))-bin->baddr;
	}
	return 0;
}

ut64 Elf_(r_bin_elf_get_fini_offset)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut64 entry = Elf_(r_bin_elf_get_entry_offset) (bin);
	ut8 buf[512];

	if (r_buf_read_at (bin->b, entry+11, buf, sizeof (buf)) == -1) {
		eprintf ("Error: read (entry)\n");
		return 0;
	}
	if (*buf == 0x68) { // push // x86/32 only
		memmove (buf, buf+1, 4);
		return (ut64)((int)(buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24)))-bin->baddr;
	}
	return 0;
}

ut64 Elf_(r_bin_elf_get_entry_offset)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut64 entry = (ut64) bin->ehdr.e_entry;
	if (entry == 0LL) {
		entry = Elf_(r_bin_elf_get_section_offset)(bin, ".init.text");
		if (entry != UT64_MAX) return entry;
		entry = Elf_(r_bin_elf_get_section_offset)(bin, ".text");
		if (entry != UT64_MAX) return entry;
		entry = Elf_(r_bin_elf_get_section_offset)(bin, ".init");
		if (entry != UT64_MAX) return entry;
	}
	if (bin->ehdr.e_entry < bin->baddr)
		return bin->ehdr.e_entry;
	return bin->ehdr.e_entry - bin->baddr; 
}

ut64 Elf_(r_bin_elf_get_main_offset)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut64 entry = Elf_(r_bin_elf_get_entry_offset) (bin);
	ut8 buf[512];

	if (r_buf_read_at (bin->b, entry, buf, sizeof (buf)) == -1) {
		eprintf ("Error: read (entry)\n");
		return 0;
	}
	// TODO: Use arch to identify arch before memcmp's

	// MIPS
	/* get .got, calculate offset of main symbol */
	if (!memcmp (buf, "\x21\x00\xe0\x03\x01\x00\x11\x04\x00\x00\x00\x00", 12)) {
		ut64 got_addr = 0LL; // TODO: get .got offset
		short delta = (buf[28]+(buf[29]<<8));
		// NOTE: This is the way to resolve 'gp' register
		r_buf_read_at (bin->b, got_addr+(32734+delta), buf, 4);
		return (ut64)((int)(buf[0]+(buf[1]<<8)+(buf[2]<<16)+(buf[3]<<24)))-bin->baddr;
	}
	// ARM
	if (!memcmp (buf, "\x24\xc0\x9f\xe5\x00\xb0\xa0\xe3", 8)) {
		return (ut64)((int)(buf[48+0]+(buf[48+1]<<8)+
		(buf[48+2]<<16)+(buf[48+3]<<24)))-bin->baddr;
	}
	// X86
#if R_BIN_ELF64
	if (!memcmp (buf, "\x49\x89\xd9", 3) && buf[156] == 0xe8) {// openbsd
		return (ut64)((int)(buf[157+0]+(buf[157+1]<<8)+
		(buf[157+2]<<16)+(buf[157+3]<<24)))+ entry + 156 + 5;
	}
	if (!memcmp (buf+29, "\x48\xc7\xc7", 3)) // linux
		return (ut64)((int)(buf[29+3]+(buf[29+4]<<8)+
		(buf[29+5]<<16)+(buf[29+6]<<24)))-bin->baddr;
#else
	if (buf[23] == '\x68')
		return (ut64)((int)(buf[23+1]+(buf[23+2]<<8)+
		(buf[23+3]<<16)+(buf[23+4]<<24)))-bin->baddr;
#endif
	return 0;
}

int Elf_(r_bin_elf_get_stripped)(struct Elf_(r_bin_elf_obj_t) *bin) {
	int i;
	if (!bin->shdr)
		return R_FALSE;
	for (i = 0; i < bin->ehdr.e_shnum; i++)
		if (bin->shdr[i].sh_type == SHT_SYMTAB)
			return R_FALSE;
	return R_TRUE;
}

int Elf_(r_bin_elf_get_static)(struct Elf_(r_bin_elf_obj_t) *bin) {
	int i;
	if (!bin->phdr)
		return R_FALSE;
	for (i = 0; i < bin->ehdr.e_phnum; i++)
		if (bin->phdr[i].p_type == PT_INTERP)
			return R_FALSE;
	return R_TRUE;
}

char* Elf_(r_bin_elf_get_data_encoding)(struct Elf_(r_bin_elf_obj_t) *bin) {
	switch (bin->ehdr.e_ident[EI_DATA]) {
	case ELFDATANONE: return strdup ("none");
	case ELFDATA2LSB: return strdup ("2's complement, little endian");
	case ELFDATA2MSB: return strdup ("2's complement, big endian");
	default: return r_str_dup_printf ("<unknown: %x>", bin->ehdr.e_ident[EI_DATA]);
	}
}

int Elf_(r_bin_elf_has_va)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut32 e_type = (ut32)bin->ehdr.e_type; // cast to avoid warn in iphone-gcc, must be ut16
	//if (bin->ehdr.e_phnum == 0)
	return (e_type == ET_REL)? 0: 1;
}

// TODO: do not strdup here
char* Elf_(r_bin_elf_get_arch)(struct Elf_(r_bin_elf_obj_t) *bin) {
	switch (bin->ehdr.e_machine) {
	case EM_AVR:
		return strdup ("avr");
	case EM_68K:
		return strdup ("m68k");
	case EM_MIPS:
	case EM_MIPS_RS3_LE:
	case EM_MIPS_X:
		return strdup ("mips");
	case EM_ARM:
		return strdup ("arm");
	case EM_SPARC:
	case EM_SPARC32PLUS:
	case EM_SPARCV9:
		return strdup ("sparc");
	case EM_PPC:
	case EM_PPC64:
		return strdup ("ppc");
	case EM_SH:
		return strdup ("sh");
	default:
		return strdup ("x86");
	}
}

char* Elf_(r_bin_elf_get_machine_name)(struct Elf_(r_bin_elf_obj_t) *bin) {
	switch (bin->ehdr.e_machine) {
	case EM_NONE:        return strdup ("No machine");
	case EM_M32:         return strdup ("AT&T WE 32100");
	case EM_SPARC:       return strdup ("SUN SPARC");
	case EM_386:         return strdup ("Intel 80386");
	case EM_68K:         return strdup ("Motorola m68k family");
	case EM_88K:         return strdup ("Motorola m88k family");
	case EM_860:         return strdup ("Intel 80860");
	case EM_MIPS:        return strdup ("MIPS R3000 big-endian");
	case EM_S370:        return strdup ("IBM System/370");
	case EM_MIPS_RS3_LE: return strdup ("MIPS R3000 little-endian");
	case EM_PARISC:      return strdup ("HPPA");
	case EM_VPP500:      return strdup ("Fujitsu VPP500");
	case EM_SPARC32PLUS: return strdup ("Sun's \"v8plus\"");
	case EM_960:         return strdup ("Intel 80960");
	case EM_PPC:         return strdup ("PowerPC");
	case EM_PPC64:       return strdup ("PowerPC 64-bit");
	case EM_S390:        return strdup ("IBM S390");
	case EM_V800:        return strdup ("NEC V800 series");
	case EM_FR20:        return strdup ("Fujitsu FR20");
	case EM_RH32:        return strdup ("TRW RH-32");
	case EM_RCE:         return strdup ("Motorola RCE");
	case EM_ARM:         return strdup ("ARM");
	case EM_FAKE_ALPHA:  return strdup ("Digital Alpha");
	case EM_SH:          return strdup ("Hitachi SH");
	case EM_SPARCV9:     return strdup ("SPARC v9 64-bit");
	case EM_TRICORE:     return strdup ("Siemens Tricore");
	case EM_ARC:         return strdup ("Argonaut RISC Core");
	case EM_H8_300:      return strdup ("Hitachi H8/300");
	case EM_H8_300H:     return strdup ("Hitachi H8/300H");
	case EM_H8S:         return strdup ("Hitachi H8S");
	case EM_H8_500:      return strdup ("Hitachi H8/500");
	case EM_IA_64:       return strdup ("Intel Merced");
	case EM_MIPS_X:      return strdup ("Stanford MIPS-X");
	case EM_COLDFIRE:    return strdup ("Motorola Coldfire");
	case EM_68HC12:      return strdup ("Motorola M68HC12");
	case EM_MMA:         return strdup ("Fujitsu MMA Multimedia Accelerator");
	case EM_PCP:         return strdup ("Siemens PCP");
	case EM_NCPU:        return strdup ("Sony nCPU embeeded RISC");
	case EM_NDR1:        return strdup ("Denso NDR1 microprocessor");
	case EM_STARCORE:    return strdup ("Motorola Start*Core processor");
	case EM_ME16:        return strdup ("Toyota ME16 processor");
	case EM_ST100:       return strdup ("STMicroelectronic ST100 processor");
	case EM_TINYJ:       return strdup ("Advanced Logic Corp. Tinyj emb.fam");
	case EM_X86_64:      return strdup ("AMD x86-64 architecture");
	case EM_PDSP:        return strdup ("Sony DSP Processor");
	case EM_FX66:        return strdup ("Siemens FX66 microcontroller");
	case EM_ST9PLUS:     return strdup ("STMicroelectronics ST9+ 8/16 mc");
	case EM_ST7:         return strdup ("STmicroelectronics ST7 8 bit mc");
	case EM_68HC16:      return strdup ("Motorola MC68HC16 microcontroller");
	case EM_68HC11:      return strdup ("Motorola MC68HC11 microcontroller");
	case EM_68HC08:      return strdup ("Motorola MC68HC08 microcontroller");
	case EM_68HC05:      return strdup ("Motorola MC68HC05 microcontroller");
	case EM_SVX:         return strdup ("Silicon Graphics SVx");
	case EM_ST19:        return strdup ("STMicroelectronics ST19 8 bit mc");
	case EM_VAX:         return strdup ("Digital VAX");
	case EM_CRIS:        return strdup ("Axis Communications 32-bit embedded processor");
	case EM_JAVELIN:     return strdup ("Infineon Technologies 32-bit embedded processor");
	case EM_FIREPATH:    return strdup ("Element 14 64-bit DSP Processor");
	case EM_ZSP:         return strdup ("LSI Logic 16-bit DSP Processor");
	case EM_MMIX:        return strdup ("Donald Knuth's educational 64-bit processor");
	case EM_HUANY:       return strdup ("Harvard University machine-independent object files");
	case EM_PRISM:       return strdup ("SiTera Prism");
	case EM_AVR:         return strdup ("Atmel AVR 8-bit microcontroller");
	case EM_FR30:        return strdup ("Fujitsu FR30");
	case EM_D10V:        return strdup ("Mitsubishi D10V");
	case EM_D30V:        return strdup ("Mitsubishi D30V");
	case EM_V850:        return strdup ("NEC v850");
	case EM_M32R:        return strdup ("Mitsubishi M32R");
	case EM_MN10300:     return strdup ("Matsushita MN10300");
	case EM_MN10200:     return strdup ("Matsushita MN10200");
	case EM_PJ:          return strdup ("picoJava");
	case EM_OPENRISC:    return strdup ("OpenRISC 32-bit embedded processor");
	case EM_ARC_A5:      return strdup ("ARC Cores Tangent-A5");
	case EM_XTENSA:      return strdup ("Tensilica Xtensa Architecture");
	default:             return r_str_dup_printf ("<unknown>: 0x%x", bin->ehdr.e_machine);
	}
}

char* Elf_(r_bin_elf_get_file_type)(struct Elf_(r_bin_elf_obj_t) *bin) {
	ut32 e_type = (ut32)bin->ehdr.e_type; // cast to avoid warn in iphone-gcc, must be ut16
	switch (e_type) {
	case ET_NONE: return strdup ("NONE (None)");
	case ET_REL:  return strdup ("REL (Relocatable file)");
	case ET_EXEC: return strdup ("EXEC (Executable file)");
	case ET_DYN:  return strdup ("DYN (Shared object file)");
	case ET_CORE: return strdup ("CORE (Core file)");
	}

	if ((e_type >= ET_LOPROC) && (e_type <= ET_HIPROC))
		return r_str_dup_printf ("Processor Specific: %x", e_type);
	else if ((e_type >= ET_LOOS) && (e_type <= ET_HIOS))
		return r_str_dup_printf ("OS Specific: %x", e_type);
	else return r_str_dup_printf ("<unknown>: %x", e_type);
}

char* Elf_(r_bin_elf_get_elf_class)(struct Elf_(r_bin_elf_obj_t) *bin) {
	switch (bin->ehdr.e_ident[EI_CLASS]) {
	case ELFCLASSNONE: return strdup ("none");
	case ELFCLASS32:   return strdup ("ELF32");
	case ELFCLASS64:   return strdup ("ELF64");
	default:           return r_str_dup_printf ("<unknown: %x>", bin->ehdr.e_ident[EI_CLASS]);
	}
}

int Elf_(r_bin_elf_get_bits)(struct Elf_(r_bin_elf_obj_t) *bin) {
	switch (bin->ehdr.e_ident[EI_CLASS]) {
	case ELFCLASSNONE: return 0;
	case ELFCLASS32:   return 32;
	case ELFCLASS64:   return 64;
	default:           return -1;
	}
}

static inline int needle(struct Elf_(r_bin_elf_obj_t) *bin, const char *s) {
	if (bin->shstrtab)
		return r_mem_mem ((const ut8*)bin->shstrtab, bin->shstrtab_size,
				(const ut8*)s, strlen (s)) != NULL;
	return 0;
}

// TODO: must return const char * all those strings must be const char os[LINUX] or so
char* Elf_(r_bin_elf_get_osabi_name)(struct Elf_(r_bin_elf_obj_t) *bin) {
	/* Hack to identify OS */
	if (needle (bin, "openbsd")) return strdup ("openbsd");
	if (needle (bin, "netbsd")) return strdup ("netbsd");
	if (needle (bin, "freebsd")) return strdup ("freebsd");
	if (needle (bin, "GNU")) return strdup ("linux");
	return strdup ("linux");
#if 0
	// XXX: this is wrong. openbsd bins are identified as linux ones.
	switch (bin->ehdr.e_ident[EI_OSABI]) {
	case ELFOSABI_ARM_AEABI:
	case ELFOSABI_ARM:        return strdup ("arm");
	case ELFOSABI_NONE:       return strdup ("linux"); // sysv
	case ELFOSABI_HPUX:       return strdup ("hpux");
	case ELFOSABI_NETBSD:     return strdup ("netbsd");
	case ELFOSABI_LINUX:      return strdup ("linux");
	case ELFOSABI_SOLARIS:    return strdup ("solaris");
	case ELFOSABI_AIX:        return strdup ("aix");
	case ELFOSABI_IRIX:       return strdup ("irix");
	case ELFOSABI_FREEBSD:    return strdup ("freebsd");
	case ELFOSABI_TRU64:      return strdup ("tru64");
	case ELFOSABI_MODESTO:    return strdup ("modesto");
	case ELFOSABI_OPENBSD:    return strdup ("openbsd");
	case ELFOSABI_STANDALONE: return strdup ("standalone");
	default:                  return r_str_dup_printf ("<unknown: %x>", bin->ehdr.e_ident[EI_OSABI]);
	}
#endif
}

int Elf_(r_bin_elf_is_big_endian)(struct Elf_(r_bin_elf_obj_t) *bin) {
	return (bin->ehdr.e_ident[EI_DATA] == ELFDATA2MSB);
}

/* XXX Init dt_strtab? */
char *Elf_(r_bin_elf_get_rpath)(struct Elf_(r_bin_elf_obj_t) *bin) {
	Elf_(Dyn) *dyn = NULL;
	ut64 stroff = 0;
	char *ret = NULL;
	int ndyn, i, j, len;

	if (!bin->phdr)
		return NULL;
	for (i = 0; i < bin->ehdr.e_phnum; i++)
		if (bin->phdr[i].p_type == PT_DYNAMIC) {
			free (dyn); // TODO: reuse dyn allocation
			if (!(dyn = malloc (1+bin->phdr[i].p_filesz))) {
				perror ("malloc (dyn)");
				free (ret);
				free (dyn);
				return NULL;
			}
			ndyn = (int)(bin->phdr[i].p_filesz / sizeof (Elf_(Dyn)));
			len = r_buf_fread_at (bin->b, bin->phdr[i].p_offset, (ut8*)dyn,
#if R_BIN_ELF64
				bin->endian?"2L":"2l",
#else
				bin->endian?"2I":"2i",
#endif
					ndyn);
			if (len  == -1) {
				eprintf ("Error: read (dyn)\n");
				free (ret);
				free (dyn);
				return NULL;
			}
			for (j = 0; j < ndyn; j++)
				if (dyn[j].d_tag == DT_STRTAB) {
					stroff = (ut64)(dyn[j].d_un.d_ptr - bin->baddr);
					break;
				}
			for (j = 0; j < ndyn; j++)
				if (dyn[j].d_tag == DT_RPATH || dyn[j].d_tag == DT_RUNPATH) {
					free (ret);
					if ((ret = malloc (ELF_STRING_LENGTH)) == NULL) {
						perror ("malloc (rpath)");
						free (dyn);
						return NULL;
					}
					if (r_buf_read_at (bin->b, stroff + dyn[j].d_un.d_val,
								(ut8*)ret, ELF_STRING_LENGTH) == -1) {
						eprintf ("Error: read (rpath)\n");
						free (ret);
						free (dyn);
						return NULL;
					}
					break;
				}
			break;
		}
	free (dyn);
	return ret;
}

struct r_bin_elf_reloc_t* Elf_(r_bin_elf_get_relocs)(struct Elf_(r_bin_elf_obj_t) *bin) {
	struct r_bin_elf_reloc_t *ret = NULL;
	Elf_(Sym) *sym = NULL;
	Elf_(Rel) *rel;
	ut64 got_addr, got_offset;
	char *strtab = NULL;
	int i, j, nrel, tsize, len, nsym, idx;
	
	if (!bin->shdr || !bin->strtab)
		return NULL;
	if ((got_offset = Elf_ (r_bin_elf_get_section_offset) (bin, ".got")) == -1 &&
		(got_offset = Elf_ (r_bin_elf_get_section_offset) (bin, ".got.plt")) == -1)
		return NULL;
	if ((got_addr = Elf_ (r_bin_elf_get_section_addr) (bin, ".got")) == -1 &&
		(got_addr = Elf_ (r_bin_elf_get_section_addr) (bin, ".got.plt")) == -1)
		return NULL;
	for (i = 0, nsym = 0; i < bin->ehdr.e_shnum; i++)
		if (bin->shdr[i].sh_type == (bin->ehdr.e_type == ET_REL ? SHT_SYMTAB : SHT_DYNSYM)) {
			bin->strtab_section = &bin->shdr[bin->shdr[i].sh_link];
			if ((strtab = (char *)malloc (8+bin->strtab_section->sh_size)) == NULL) {
				perror ("malloc (syms strtab)");
				return NULL;
			}
			if (r_buf_read_at (bin->b, bin->strtab_section->sh_offset, (ut8*)strtab, bin->strtab_section->sh_size) == -1) {
				eprintf ("Error: read (syms strtab)\n");
				return NULL;
			}
			if ((sym = (Elf_(Sym) *)malloc (1+bin->shdr[i].sh_size)) == NULL) { // LEAKS
				perror ("malloc (syms)");
				return NULL;
			}
			nsym = (int)(bin->shdr[i].sh_size/sizeof (Elf_(Sym)));
			if (r_buf_fread_at (bin->b, bin->shdr[i].sh_offset, (ut8*)sym,
#if R_BIN_ELF64
					bin->endian?"I2cS2L":"i2cs2l",
#else
					bin->endian?"3I2cS":"3i2cs",
#endif
					nsym) == -1) {
				eprintf ("Error: read (sym)\n");
				return NULL;
			}
		}

	for (i = 0; i < bin->ehdr.e_shnum; i++) {
		if (bin->shdr[i].sh_name > bin->strtab_size) {
			eprintf ("Invalid shdr index in strtab %d/%"PFMT64d"\n",
				bin->shdr[i].sh_name, (ut64) bin->strtab_size);
			continue;
		}
		if (!strcmp (&bin->strtab[bin->shdr[i].sh_name], ".rel.plt"))
			tsize = sizeof (Elf_(Rel));
		else if (!strcmp (&bin->strtab[bin->shdr[i].sh_name], ".rela.plt"))
			tsize = sizeof (Elf_(Rela));
		else continue;
		if (tsize <1)
			return ret; // -1 ?
		if ((rel = (Elf_(Rel) *)malloc ((int)(bin->shdr[i].sh_size / tsize) * sizeof (Elf_(Rel)))) == NULL) {
			perror ("malloc (rel)");
			return NULL;
		}
		for (j = nrel = 0; j < bin->shdr[i].sh_size; j += tsize, nrel++) {
			len = r_buf_fread_at (bin->b, bin->shdr[i].sh_offset + j, (ut8*)&rel[nrel],
#if R_BIN_ELF64
					bin->endian?"2L":"2l",
#else
					bin->endian?"2I":"2i",
#endif
					1);
			if (len == -1) {
				eprintf ("Error: read (rel)\n");
				return NULL;
			}
		}
		if ((ret = (struct r_bin_elf_reloc_t *)malloc ((nrel+1) * sizeof (struct r_bin_elf_reloc_t))) == NULL) {
			perror ("malloc (reloc)");
			return NULL;
		}
		if (sym)
		for (j =  0; j < nrel; j++) {
			idx = ELF_R_SYM (rel[j].r_info);
			if (idx < nsym) {
				if (sym[idx].st_name > bin->strtab_section->sh_size) {
					eprintf ("Invalid symbol index in strtab %d/%"PFMT64d"\n",
							bin->shdr[i].sh_name, (ut64) bin->strtab_section->sh_size);
					continue;
				}
				len = __strnlen (&strtab[sym[idx].st_name], ELF_STRING_LENGTH-1);
				memcpy (ret[j].name, &strtab[sym[idx].st_name], len);
			} else strncpy (ret[j].name, "unknown", ELF_STRING_LENGTH);
			ret[j].sym = ELF_R_SYM (rel[j].r_info);
			ret[j].type = ELF_R_TYPE (rel[j].r_info);
			ret[j].offset = rel[j].r_offset-got_addr+got_offset,
			ret[j].rva = rel[j].r_offset - bin->baddr;
			ret[j].last = 0;
		}
		ret[j].last = 1;
		break;
	}
	return ret;
}

struct r_bin_elf_lib_t* Elf_(r_bin_elf_get_libs)(struct Elf_(r_bin_elf_obj_t) *bin) {
	struct r_bin_elf_lib_t *ret = NULL;
	Elf_(Dyn) *dyn = NULL;
	ut64 stroff = 0;
	int ndyn, i, j, k, len;

	if (!bin->phdr)
		return NULL;
	for (i = 0; i < bin->ehdr.e_phnum; i++)
		if (bin->phdr[i].p_type == PT_DYNAMIC) {
			if (!(dyn = malloc (bin->phdr[i].p_filesz))) {
				perror ("malloc (dyn)");
				return NULL;
			}
			ndyn = (int)(bin->phdr[i].p_filesz / sizeof (Elf_(Dyn)));
			len = r_buf_fread_at (bin->b, bin->phdr[i].p_offset, (ut8*)dyn,
#if R_BIN_ELF64
					bin->endian?"2L":"2l",
#else
					bin->endian?"2I":"2i",
#endif
					ndyn);
			if (len  == -1) {
				eprintf ("Error: read (dyn)\n");
				free (dyn);
				return NULL;
			}
			for (j = 0; j < ndyn; j++)
				if (dyn[j].d_tag == DT_STRTAB) {
					stroff = (ut64)(dyn[j].d_un.d_ptr - bin->baddr);
					break;
				}
			for (j = 0, k = 0; j < ndyn; j++)
				if (dyn[j].d_tag == DT_NEEDED) {
					ret = realloc (ret, (k+1) * sizeof (struct r_bin_elf_lib_t));
					if (ret == NULL) {
						perror ("realloc (libs)");
						free (dyn);
						return NULL;
					}
					if (r_buf_read_at (bin->b, stroff + dyn[j].d_un.d_val,
							(ut8*)ret[k].name, ELF_STRING_LENGTH) == -1) {
						eprintf ("Error: read (libs)\n");
						free (ret);
						free (dyn);
						return NULL;
					}
					ret[k].last = 0;
					k++;
				}
			ret = realloc (ret, (k+1) * sizeof (struct r_bin_elf_lib_t));
			if (ret == NULL) {
				perror ("realloc (libs)");
				free (dyn);
				return NULL;
			}
			ret[k].last = 1;
			free (dyn);
			break;
		}
	return ret;
}

struct r_bin_elf_section_t* Elf_(r_bin_elf_get_sections)(struct Elf_(r_bin_elf_obj_t) *bin) {
	struct r_bin_elf_section_t *ret = NULL;
	int i, nidx;
	
	if ((ret = malloc ((bin->ehdr.e_shnum + 1) * sizeof (struct r_bin_elf_section_t))) == NULL)
		return NULL;
	for (i = 0; i < bin->ehdr.e_shnum; i++) {
		ret[i].offset = bin->shdr[i].sh_offset;
		ret[i].rva = bin->shdr[i].sh_addr > bin->baddr?
			bin->shdr[i].sh_addr-bin->baddr:bin->shdr[i].sh_addr;
		ret[i].size = bin->shdr[i].sh_size;
		ret[i].align = bin->shdr[i].sh_addralign;
		ret[i].flags = bin->shdr[i].sh_flags;
		//memset (ret[i].name, 0, sizeof (ret[i].name));
		nidx = bin->shdr[i].sh_name;
		if (nidx<0 || nidx>bin->shstrtab_section->sh_size)
			strncpy (ret[i].name, "invalid", sizeof (ret[i].name)-4);
		else strncpy (ret[i].name, bin->shstrtab?
			&bin->shstrtab[bin->shdr[i].sh_name]: "unknown", sizeof (ret[i].name)-4);
		ret[i].name[sizeof (ret[i].name)-2] = 0;
		ret[i].last = 0;
	}
	ret[i].last = 1;
	return ret;
}

struct r_bin_elf_symbol_t* Elf_(r_bin_elf_get_symbols)(struct Elf_(r_bin_elf_obj_t) *bin, int type) {
	Elf_(Shdr) *strtab_section;
	Elf_(Sym) *sym;
	struct r_bin_elf_symbol_t *ret = NULL;
	char *strtab;
	ut64 sym_offset = 0, data_offset = 0, toffset;
	int tsize, nsym, ret_ctr, i, j, k, len;

	if (!bin->shdr || bin->ehdr.e_shnum == 0)
		return NULL;
	if (bin->ehdr.e_type == ET_REL) {
		// XXX: we must obey shndx here
		if ((sym_offset = Elf_(r_bin_elf_get_section_offset)(bin, ".text")) == -1)
			sym_offset = 0;
		if ((data_offset = Elf_(r_bin_elf_get_section_offset)(bin, ".rodata")) == -1)
			data_offset = 0;
	}
	for (i = 0; i < bin->ehdr.e_shnum; i++)
		if ((type == R_BIN_ELF_IMPORTS &&
				bin->shdr[i].sh_type == (bin->ehdr.e_type == ET_REL ? SHT_SYMTAB : SHT_DYNSYM)) ||
			(type == R_BIN_ELF_SYMBOLS  &&
			 	bin->shdr[i].sh_type == (Elf_(r_bin_elf_get_stripped) (bin) ? SHT_DYNSYM : SHT_SYMTAB))) {
			strtab_section = &bin->shdr[bin->shdr[i].sh_link];
			if ((strtab = (char *)malloc (8+strtab_section->sh_size)) == NULL) {
				perror ("malloc (syms strtab)");
				return NULL;
			}
			if (r_buf_read_at (bin->b, strtab_section->sh_offset, (ut8*)strtab, strtab_section->sh_size) == -1) {
				eprintf ("Error: read (syms strtab)\n");
				return NULL;
			}

			if ((sym = (Elf_(Sym) *)malloc (1+bin->shdr[i].sh_size)) == NULL) {
				perror ("malloc (syms)");
				return NULL;
			}
			nsym = (int)(bin->shdr[i].sh_size/sizeof (Elf_(Sym)));
			if (r_buf_fread_at (bin->b, bin->shdr[i].sh_offset, (ut8*)sym,
#if R_BIN_ELF64
					bin->endian? "I2cS2L": "i2cs2l",
#else
					bin->endian? "3I2cS": "3i2cs",
#endif
					nsym) == -1) {
				eprintf ("Error: read (sym)\n");
				return NULL;
			}
			for (j = k = ret_ctr = 0; j < bin->shdr[i].sh_size; j += sizeof (Elf_(Sym)), k++) {
				if (k == 0)
					continue;
				if (type == R_BIN_ELF_IMPORTS && sym[k].st_shndx == STN_UNDEF) {
					if (sym[k].st_value)
						toffset = sym[k].st_value;
					else if ((toffset = Elf_(get_import_addr) (bin, k)) == -1)
						toffset = 0;
					tsize = 16;
				} else if (type == R_BIN_ELF_SYMBOLS && sym[k].st_shndx != STN_UNDEF &&
						ELF_ST_TYPE(sym[k].st_info) != STT_SECTION && ELF_ST_TYPE(sym[k].st_info) != STT_FILE) {
					int idx = sym[k].st_shndx;
					tsize = sym[k].st_size;
					toffset = (ut64)sym[k].st_value; //-sym_offset; // + (ELF_ST_TYPE(sym[k].st_info) == STT_FUNC?sym_offset:data_offset);
				} else continue;
				if ((ret = realloc (ret, (ret_ctr + 1) * sizeof (struct r_bin_elf_symbol_t))) == NULL) {
					perror ("realloc (symbols|imports)");
					return NULL;
				}
				{
					int idx = sym[k].st_shndx;
					if (idx>=0 && idx < bin->ehdr.e_shnum) {
						if (bin->baddr && toffset>bin->baddr)
							toffset -= bin->baddr;
						else
							toffset += bin->shdr[idx].sh_offset;
					} else {
						//eprintf ("orphan symbol %d %d %s\n", idx, STN_UNDEF, &strtab[sym[k].st_name] );
						continue;
					}
				}
				ret[ret_ctr].offset = toffset; //(toffset >= bin->baddr ? toffset -= bin->baddr : toffset);
				ret[ret_ctr].size = tsize;
				if (sym[k].st_name > strtab_section->sh_size) {
					perror ("index out of strtab range\n");
					return NULL;
				}
				len = __strnlen (&strtab[sym[k].st_name], ELF_STRING_LENGTH-1);
				memcpy (ret[ret_ctr].name, &strtab[sym[k].st_name], len);
				ret[ret_ctr].ordinal = k;
				ret[ret_ctr].name[ELF_STRING_LENGTH-2] = '\0';
				#define s_bind(x) snprintf (ret[ret_ctr].bind, ELF_STRING_LENGTH, x);
				switch (ELF_ST_BIND(sym[k].st_info)) {
				case STB_LOCAL:  s_bind ("LOCAL"); break;
				case STB_GLOBAL: s_bind ("GLOBAL"); break;
				case STB_NUM:    s_bind ("NUM"); break;
				case STB_LOOS:   s_bind ("LOOS"); break;
				case STB_HIOS:   s_bind ("HIOS"); break;
				case STB_LOPROC: s_bind ("LOPROC"); break;
				case STB_HIPROC: s_bind ("HIPROC"); break;
				default:         s_bind ("UNKNOWN");
				}
				#define s_type(x) snprintf (ret[ret_ctr].type, ELF_STRING_LENGTH, x);
				switch (ELF_ST_TYPE (sym[k].st_info)) {
				case STT_NOTYPE:  s_type ("NOTYPE"); break;
				case STT_OBJECT:  s_type ("OBJECT"); break;
				case STT_FUNC:    s_type ("FUNC"); break;
				case STT_SECTION: s_type ("SECTION"); break;
				case STT_FILE:    s_type ("FILE"); break;
				case STT_COMMON:  s_type ("COMMON"); break;
				case STT_TLS:     s_type ("TLS"); break;
				case STT_NUM:     s_type ("NUM"); break;
				case STT_LOOS:    s_type ("LOOS"); break;
				case STT_HIOS:    s_type ("HIOS"); break;
				case STT_LOPROC:  s_type ("LOPROC"); break;
				case STT_HIPROC:  s_type ("HIPROC"); break;
				default:          s_type ("UNKNOWN");
				}
				ret[ret_ctr].last = 0;
				ret_ctr++;
			}
			if ((ret = realloc (ret, (ret_ctr + 1) * sizeof (struct r_bin_elf_symbol_t))) == NULL)
				return NULL;
			ret[ret_ctr].last = 1; // ugly dirty hack :D
			break;
		}
	return ret;
}

struct r_bin_elf_field_t* Elf_(r_bin_elf_get_fields)(struct Elf_(r_bin_elf_obj_t) *bin) {
	struct r_bin_elf_field_t *ret = NULL;
	int i = 0, j;

	if ((ret = malloc ((bin->ehdr.e_phnum+3 + 1) *
			sizeof (struct r_bin_elf_field_t))) == NULL)
		return NULL;
	strncpy (ret[i].name, "ehdr", ELF_STRING_LENGTH); 
	ret[i].offset = 0;
	ret[i++].last = 0;
	strncpy (ret[i].name, "shoff", ELF_STRING_LENGTH); 
	ret[i].offset = bin->ehdr.e_shoff;
	ret[i++].last = 0;
	strncpy (ret[i].name, "phoff", ELF_STRING_LENGTH); 
	ret[i].offset = bin->ehdr.e_phoff;
	ret[i++].last = 0;
	for (j = 0; bin->phdr && j < bin->ehdr.e_phnum; i++, j++) {
		snprintf (ret[i].name, ELF_STRING_LENGTH, "phdr_%i", j);
		ret[i].offset = bin->phdr[j].p_offset;
		ret[i].last = 0;
	}
	ret[i].last = 1;
	return ret;
}

void* Elf_(r_bin_elf_free)(struct Elf_(r_bin_elf_obj_t)* bin) {
	if (!bin) return NULL;
	free (bin->phdr);
	free (bin->shdr);
	free (bin->strtab);
	//free (bin->strtab_section);
	r_buf_free (bin->b);
	free (bin);
	return NULL;
}

struct Elf_(r_bin_elf_obj_t)* Elf_(r_bin_elf_new)(const char* file) {
	struct Elf_(r_bin_elf_obj_t) *bin;
	ut8 *buf;
// TODO: use R_NEW0 here
	if (!(bin = malloc (sizeof (struct Elf_(r_bin_elf_obj_t)))))
		return NULL;
	memset (bin, 0, sizeof (struct Elf_(r_bin_elf_obj_t)));
	bin->file = file;
	if (!(buf = (ut8*)r_file_slurp (file, &bin->size))) 
		return Elf_(r_bin_elf_free) (bin);
	bin->b = r_buf_new ();
	if (!r_buf_set_bytes (bin->b, buf, bin->size))
		return Elf_(r_bin_elf_free) (bin);
	free (buf);
	if (!Elf_(r_bin_elf_init) (bin))
		return Elf_(r_bin_elf_free) (bin);
	return bin;
}

struct Elf_(r_bin_elf_obj_t)* Elf_(r_bin_elf_new_buf)(struct r_buf_t *buf) {
	struct Elf_(r_bin_elf_obj_t) *bin;
// TODO: use R_NEW here
	if (!(bin = malloc (sizeof (struct Elf_(r_bin_elf_obj_t)))))
		return NULL;
	memset (bin, 0, sizeof (struct Elf_(r_bin_elf_obj_t)));
	bin->b = buf;
	bin->size = buf->length;
	if (!Elf_(r_bin_elf_init) (bin))
		return Elf_(r_bin_elf_free) (bin);
	return bin;
}
