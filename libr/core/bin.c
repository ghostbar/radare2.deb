/* radare - LGPL - Copyright 2011-2014 - earada, pancake */

#include <r_core.h>

R_API void r_core_bin_set_by_fd (RCore *core, ut64 bin_fd) {
	RListIter *iter;
	RBinFile *bf = NULL;

	r_list_foreach (core->bin->binfiles, iter, bf) {
		if (bf && bf->fd == bin_fd) {
			core->bin->cur = bf;
			break;
		}
		bf = NULL;
	}
	if (bf) r_core_bin_bind (core);
}

R_API void r_core_bin_bind (RCore *core) {
	if (!core->bin) return;
	r_bin_bind (core->bin, &(core->anal->binb));
	r_bin_bind (core->bin, &(core->assembler->binb));
	r_bin_bind (core->bin, &(core->file->binb));
}

R_API int r_core_bin_refresh_strings(RCore *r) {
	return r_bin_reset_strings (r->bin) ? R_TRUE: R_FALSE;
}

static int bin_strings (RCore *r, int mode, ut64 baddr, int va) {
#define MINSTR 4
	char *p, *q, str[R_FLAG_NAME_SIZE];
	RBinSection *section;
	int hasstr, minstr, rawstr;
	RBinString *string;
	RListIter *iter;
	RList *list;
	int i = 0;
	minstr = r_config_get_i (r->config, "bin.minstr");
	rawstr = r_config_get_i (r->config, "bin.rawstr");
	r->bin->cur->rawstr = rawstr;

	if (!(hasstr = r_config_get_i (r->config, "bin.strings")))
		return 0;
	if (!r->bin->cur->curplugin) return 0;
	if (!r->bin->cur->curplugin->info) {
		if (!rawstr) {
			eprintf ("WARN: Use '-e bin.rawstr=true' or 'rabin2 -zz'"
				" to find strings on unknown file types\n");
			return R_FALSE;
		}
	}
	//if (r->bin->minstrlen == 0 && minstr>0) r->bin->minstrlen = minstr;
	//else if (r->bin->minstrlen > 0) r_config_set_i (r->config, "bin.minstr", r->bin->minstrlen);
	if (r->bin->minstrlen==0) {
		r->bin->minstrlen = r->bin->cur->curplugin->minstrlen;
		if (r->bin->minstrlen==0)
			r->bin->minstrlen = MINSTR;
	}
	if (minstr>0 || r->bin->minstrlen <=0) {
		r->bin->minstrlen = R_MIN (minstr, MINSTR);
		//return R_FALSE;
	}

	/* code */
	if (rawstr) {
		// TODO: search in whole file, ignoring sections
	}
	if ((list = r_bin_get_strings (r->bin)) == NULL)
		return R_FALSE;

	if ((mode & R_CORE_BIN_JSON)) {
		r_cons_printf ("[");
		r_list_foreach (list, iter, string) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, string->rva,
				string->offset): string->offset;
			q = strdup (string->string);
			//r_name_filter (str, 128);
			for (p=q; *p; p++) if (*p=='"') *p = '\'';
			r_cons_printf ("%s{\"offset\":%"PFMT64d
				",\"length\":%d,\"size\":%d,"
				"\"type\":\"%s\",\"string\":\"%s\"}", 
				iter->p? ",": "", addr,
				string->length, string->size,
				string->type=='W'?"wide":"ascii", q);
			free (q);
		}
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_SIMPLE)) {
		r_list_foreach (list, iter, string) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, string->rva,
				string->offset): string->offset;
			r_cons_printf ("%"PFMT64d" %d %d %s\n", 
				addr, string->size, string->length, string->string);
		}
	} else
	if ((mode & R_CORE_BIN_SET)) {
		if (r_config_get_i (r->config, "bin.strings"))
			r_flag_space_set (r->flags, "strings");
		r_cons_break (NULL, NULL);
		r_list_foreach (list, iter, string) {
			int size = (string->type == 'W')? string->size*2: string->size;
			if (r_cons_singleton()->breaked) break;
			/* Jump the withespaces before the string */
			for (i=0; *(string->string+i)==' '; i++);
			r_meta_add (r->anal, R_META_TYPE_STRING,
				va?baddr+string->rva:string->offset,
				(va?baddr+string->rva:string->offset)+string->size,
				string->string+i);
			r_name_filter (string->string, 128);
			snprintf (str, R_FLAG_NAME_SIZE, "str.%s", string->string);
			r_flag_set (r->flags, str,
				va? baddr+string->rva:string->offset,
				size, 0);
		}
		//r_meta_cleanup (r->anal->meta, 0LL, UT64_MAX);
		r_cons_break_end ();
	} else {
		if (mode) r_cons_printf ("fs strings\n"); //: "[strings]\n");
		r_list_foreach (list, iter, string) {
			section = r_bin_get_section_at (r->bin, string->offset, 0);
			int size = (string->type == 'W')? string->size*2: string->size;
// XXX string ->size is length! not size!!
			if (mode) {
				r_name_filter (string->string, sizeof (string->string));
				r_cons_printf ("f str.%s %"PFMT64d" @ 0x%08"PFMT64x"\n"
					"Cs %"PFMT64d" @ 0x%08"PFMT64x"\n",
					string->string, size, va?baddr+string->rva:string->offset,
					string->size, va?baddr+string->rva:string->offset);
			} else r_cons_printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x
				" ordinal=%03"PFMT64d" "
				"sz=%d len=%d section=%s type=%c string=%s\n",
				baddr+string->rva, string->offset,
				string->ordinal, string->size, string->length,
				section?section->name:"unknown",
				string->type, string->string);
			i++;
		}
		//if (!mode) r_cons_printf ("\n%i strings\n", i);
	}
	return R_TRUE;
}

static int bin_info (RCore *r, int mode) {
	int i, j;
	char str[R_FLAG_NAME_SIZE];
	RBinInfo *info = r_bin_get_info (r->bin);
	if (!info) return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("{\"type\":\"%s\","
			"\"class\":\"%s\","
			"\"endian\":\"%s\","
			"\"machine\":\"%s\","
			"\"arch\":\"%s\","
			"\"os\":\"%s\","
			"\"lang\":\"%s\","
			"\"pic\":%s,"
			"\"va\":%s,"
			"\"bits\":%d,"
			"\"stripped\":%s,"
			"\"static\":%s,"
			"\"linenums\":%s,"
			"\"syms\":%s,"
			"\"relocs\":%s}",
			info->rclass,
			info->bclass,
			info->big_endian? "big": "little",
			info->machine,
			info->arch,
			info->os,
			info->lang?info->lang:"",
			info->has_pi? "true": "false",
			info->has_va? "true": "false",
			info->bits,
			r_str_bool (R_BIN_DBG_STRIPPED (info->dbg_info)),
			r_str_bool (r_bin_is_static (r->bin)),//R_BIN_DBG_STATIC (info->dbg_info)),
			r_str_bool (R_BIN_DBG_LINENUMS (info->dbg_info)),
			r_str_bool (R_BIN_DBG_SYMS (info->dbg_info)),
			r_str_bool (R_BIN_DBG_RELOCS (info->dbg_info))
			);
	} else
	if ((mode & R_CORE_BIN_SIMPLE)) {
		r_cons_printf ("arch %s\n", info->arch);
		r_cons_printf ("bits %d\n", info->bits);
		r_cons_printf ("os %s\n", info->os);
		r_cons_printf ("endian %s\n", info->big_endian? "big": "little");
	} else
	if ((mode & R_CORE_BIN_SET)) {
		r_config_set (r->config, "file.type", info->rclass);
		r_config_set (r->config, "cfg.bigendian", info->big_endian?"true":"false");
		if (!strcmp (info->rclass, "fs")) {
			r_config_set (r->config, "asm.arch", info->arch);
			r_core_cmdf (r, "m /root %s 0", info->arch);
		} else {
			r_config_set (r->config, "asm.os", info->os);
			r_config_set (r->config, "asm.arch", info->arch);
			r_config_set (r->config, "anal.arch", info->arch);
			snprintf (str, R_FLAG_NAME_SIZE, "%i", info->bits);
			r_config_set (r->config, "asm.bits", str);
			r_config_set (r->config, "asm.dwarf",
				R_BIN_DBG_STRIPPED (info->dbg_info)?"false":"true");
		}
	} else {
		if (mode) {
			if (!strcmp (info->type, "fs")) {
				r_cons_printf ("e file.type=fs\n");
				r_cons_printf ("m /root %s 0\n", info->arch);
			} else {
				// XXX: hack to disable io.va when loading an elf object
				// XXX: this must be something generic for all filetypes
				// XXX: needs new api in r_bin_has_va () or something..
				//int has_va = (!strcmp (info->rclass, "elf-object"))? 0: 1;
				//if (!strcmp (info->type, "REL"))...relocatable object..
				r_cons_printf (
					"e file.type=%s\n"
					"e cfg.bigendian=%s\n"
					"e asm.os=%s\n"
					"e asm.arch=%s\n"
					"e anal.arch=%s\n"
					"e asm.bits=%i\n"
					"e asm.dwarf=%s\n",
					info->rclass, r_str_bool (info->big_endian), info->os,
					info->arch, info->arch, info->bits,
					r_str_bool (R_BIN_DBG_STRIPPED (info->dbg_info)));
			}
		} else {
			// if type is 'fs' show something different?
			//r_cons_printf ("# File info\n");
			r_cons_printf ("file\t%s\n"
					"type\t%s\n"
					"pic\t%s\n"
					"has_va\t%s\n"
					"root\t%s\n"
					"class\t%s\n"
					"lang\t%s\n"
					"arch\t%s\n"
					"bits\t%i\n"
					"machine\t%s\n"
					"os\t%s\n"
					"subsys\t%s\n"
					"endian\t%s\n"
					"strip\t%s\n"
					"static\t%s\n"
					"linenum\t%s\n"
					"lsyms\t%s\n"
					"relocs\t%s\n"
					"rpath\t%s\n",
					info->file, info->type,
					r_str_bool (info->has_pi),
					r_str_bool (info->has_va),
					info->rclass, info->bclass, info->lang?info->lang:"unknown",
					info->arch, info->bits, info->machine, info->os,
					info->subsystem, info->big_endian? "big": "little", 
					r_str_bool (R_BIN_DBG_STRIPPED (info->dbg_info)),
					r_str_bool (r_bin_is_static (r->bin)),
					r_str_bool (R_BIN_DBG_LINENUMS (info->dbg_info)),
					r_str_bool (R_BIN_DBG_SYMS (info->dbg_info)),
					r_str_bool (R_BIN_DBG_RELOCS (info->dbg_info)),
					info->rpath);
			for (i=0; info->sum[i].type; i++) {
				int len;
				//ut8 *sum = &info; // XXX
				RBinHash *h = &info->sum[i];
				ut64 hash = r_hash_name_to_bits (h->type);
				RHash *rh = r_hash_new (R_TRUE, hash);
				len = r_hash_calculate (rh, hash, (const ut8*)
					r->bin->cur->buf->buf+h->from, h->to);
				//ut8 *p = r->bin->cur->buf+h->addr;
				if (len<1) eprintf ("Invaild wtf\n");
				r_hash_free (rh);

				r_cons_printf ("%s\t%d-%dc\t", h->type, h->from, h->to+h->from);
				for (j=0; j<h->len; j++)
					r_cons_printf ("%02x", h->buf[j]);
				r_cons_newline ();
			}
		}
	}
	return R_TRUE;
}

static int bin_dwarf (RCore *core, int mode) {
        RBinDwarfRow *row;
        RListIter *iter;
        RList *list = NULL;

	if (core->bin && core->bin->cur->curplugin && core->bin->cur->curplugin->lines) {
		list = core->bin->cur->curplugin->lines (core->bin->cur);
	} else {
		// TODO: complete and speed-up support for dwarf
		if (r_config_get_i (core->config, "bin.dwarf")) {
			RBinDwarfDebugAbbrev *da;
			da = r_bin_dwarf_parse_abbrev (core->bin);
			r_bin_dwarf_parse_info (da, core->bin);
			r_bin_dwarf_parse_aranges (core->bin);
			list = r_bin_dwarf_parse_line (core->bin);

			r_bin_dwarf_free_debug_abbrev(da);
			free(da);
		}
	}
	if (!list) return R_FALSE;
	r_cons_break (NULL, NULL);
        r_list_foreach (list, iter, row) {
		if (r_cons_singleton()->breaked) break;
		if (mode) {
			// TODO: use 'Cl' instead of CC
			const char *path = row->file;
			char *line = r_file_slurp_line (
					path, row->line-1, 0);
			if (line) {
				r_str_filter (line, strlen (line));
				line = r_str_replace (line, "\"", "\\\"", 1);
				line = r_str_replace (line, "\\\\", "\\", 1);
			}
			// TODO: implement internal : if ((mode & R_CORE_BIN_SET)) {
			if ((mode & R_CORE_BIN_SET)) {
				r_core_cmdf (core, "\"CC %s:%d  %s\"@0x%"PFMT64x"\n",
						row->file, row->line, line?line:"", row->address);
			} else r_cons_printf ("\"CC %s:%d  %s\"@0x%"PFMT64x"\n",
				row->file, row->line, line?line:"", row->address);
			free (line);
		} else {
			r_cons_printf ("0x%08"PFMT64x"\t%s\t%d\n", row->address, row->file, row->line);
		}
        }
	r_cons_break_end ();
        r_list_destroy (list);
	return R_TRUE;
}

static int bin_main (RCore *r, int mode, ut64 baddr, int va) {
	RBinAddr *binmain = r_bin_get_sym (r->bin, R_BIN_SYM_MAIN);
	if (!binmain) return R_FALSE;

	if ((mode & R_CORE_BIN_SIMPLE) || mode & R_CORE_BIN_JSON) {
		r_cons_printf ("%"PFMT64d, va? baddr+binmain->rva:binmain->offset);
	} else
	if ((mode & R_CORE_BIN_SET)) {
		r_flag_space_set (r->flags, "symbols");
		r_flag_set (r->flags, "main", va? baddr+binmain->rva: binmain->offset,
				r->blocksize, 0);
	} else {
		if (mode) {
			r_cons_printf ("fs symbols\n");
			r_cons_printf ("f main @ 0x%08"PFMT64x"\n",
				va? baddr+binmain->rva: binmain->offset);
		} else {
			r_cons_printf ("[Main]\n");
			r_cons_printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x"\n",
					baddr+binmain->rva, binmain->offset);
		}
	}
	return R_TRUE;
}

static int bin_entry (RCore *r, int mode, ut64 baddr, int va) {
	char str[R_FLAG_NAME_SIZE];
	RList *entries;
	RListIter *iter;
	RBinAddr *entry = NULL;
	int i = 0;

	if ((entries = r_bin_get_entries (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (entries, iter, entry) {
			ut64 paddr = entry->offset;
			ut64 vaddr = r_bin_get_vaddr (r->bin, baddr, paddr, entry->rva);
			r_cons_printf ("%s%"PFMT64d,
					iter->p?",":"",
				va?vaddr: paddr);
		}
		r_cons_printf ("]");
	} else
	if (mode & R_CORE_BIN_SIMPLE) {
		r_list_foreach (entries, iter, entry) {
			ut64 paddr = entry->offset;
			ut64 vaddr = r_bin_get_vaddr (r->bin, baddr, paddr, entry->rva);
			r_cons_printf ("0x%08"PFMT64x"\n", va?vaddr: paddr); 
		}
	} else
	if ((mode & R_CORE_BIN_SET)) {
		r_list_foreach (entries, iter, entry) {
			ut64 paddr = entry->offset;
			ut64 vaddr = r_bin_get_vaddr (r->bin, baddr, paddr, entry->rva);
			snprintf (str, R_FLAG_NAME_SIZE, "entry%i", i++);
			r_flag_set (r->flags, str, va? vaddr: paddr, 
				r->blocksize, 0);
		}
		/* Seek to the last entry point */
		if (entry)
			r_core_seek (r, va? baddr+entry->rva: entry->offset, 0);
	} else {
		if (mode) r_cons_printf ("fs symbols\n");
		else r_cons_printf ("[Entrypoints]\n");

		r_list_foreach (entries, iter, entry) {
			ut64 paddr = entry->offset;
			ut64 vaddr = r_bin_get_vaddr (r->bin, baddr, paddr, entry->rva);
			if (mode) {
				r_cons_printf ("f entry%i @ 0x%08"PFMT64x"\n",
					i, va?vaddr: paddr);
				r_cons_printf ("s entry%i\n", i);
			} else r_cons_printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" baddr=0x%08"PFMT64x"\n",
					vaddr, paddr, baddr);
			i++;
		}
		if (!mode) r_cons_printf ("\n%i entrypoints\n", i);
	}
	return R_TRUE;
}

static const char *bin_reloc_type_name (RBinReloc *reloc) {
#define CASE(T) case R_BIN_RELOC_ ## T: return reloc->additive ? "ADD_" #T : "SET_" #T
	switch (reloc->type) {
		CASE(8);
		CASE(16);
		CASE(32);
		CASE(64);
	}
	return "UNKNOWN";
#undef CASE
}

static ut8 bin_reloc_size (RBinReloc *reloc) {
	#define CASE(T) case R_BIN_RELOC_ ## T: return T / 8
	switch (reloc->type) {
		CASE(8);
		CASE(16);
		CASE(32);
		CASE(64);
	}
	return 0;
	#undef CASE
}

static int bin_relocs (RCore *r, int mode, ut64 baddr, int va) {
	char str[R_FLAG_NAME_SIZE];
	RList *relocs;
	RListIter *iter;
	RBinReloc *reloc;
	int i = 0;

	if ((relocs = r_bin_get_relocs (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (relocs, iter, reloc) {
			if (reloc->import)
				r_cons_printf ("%s{\"name\":\"%s\",", iter->p?",":"", reloc->import->name);
			else
				r_cons_printf ("%s{\"name\":null,", iter->p?",":"");
			r_cons_printf ("\"type\":\"%s\","
				"\"offset\":%"PFMT64d","
				//"\"addend\":%"PFMT64d","
				"\"physical\":%"PFMT64d"}",
				bin_reloc_type_name (reloc),
				baddr+reloc->rva,
				//reloc->addend,
				reloc->offset);
		}
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_SET)) {
		r_flag_space_set (r->flags, "relocs");
		r_list_foreach (relocs, iter, reloc) {
			if (reloc->import) {
				snprintf (str, R_FLAG_NAME_SIZE,
					"reloc.%s", reloc->import->name);
				r_name_filter (str, 0);
				//r_str_replace_char (str, '$', '_');
				r_flag_set (r->flags, str, va?baddr+reloc->rva:reloc->offset,
					bin_reloc_size (reloc), 0);
			} else {
				// TODO(eddyb) implement constant relocs.
			}
		}
	} else
	if ((mode & R_CORE_BIN_SIMPLE)) {
		r_list_foreach (relocs, iter, reloc) {
			r_cons_printf ("0x%08"PFMT64x"  %s\n",
				va?baddr+reloc->rva:reloc->offset, reloc->import ? reloc->import->name : "");
		}
	} else {
		if (mode) {
			r_cons_printf ("fs relocs\n");
			r_list_foreach (relocs, iter, reloc) {
				if (reloc->import) {
					char *str = strdup (reloc->import->name);
					r_str_replace_char (str, '$', '_');
					r_cons_printf ("f reloc.%s @ 0x%08"PFMT64x"\n", str,
						va?baddr+reloc->rva:reloc->offset);
					free (str);
				} else {
					// TODO(eddyb) implement constant relocs.
				}
				i++;
			}
		} else {
			r_cons_printf ("[Relocations]\n");
			r_list_foreach (relocs, iter, reloc) {
				r_cons_printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" type=%s",
					baddr+reloc->rva, reloc->offset, bin_reloc_type_name (reloc));
				if (reloc->import && reloc->import->name[0])
					r_cons_printf (" %s", reloc->import->name);
				if (reloc->addend) {
					if (reloc->import && reloc->addend > 0)
						r_cons_printf (" +");
					if (reloc->addend < 0)
						r_cons_printf (" - 0x%08"PFMT64x, -reloc->addend);
					else
						r_cons_printf (" 0x%08"PFMT64x, reloc->addend);
				}
				r_cons_printf ("\n");
				i++;
			}
			r_cons_printf ("\n%i relocations\n", i);
		}
	}
	return R_TRUE;
}

/* XXX: This is a hack to get PLT references in rabin2 -i */
/* imp. is a prefix that can be rewritten by the symbol table */
static ut64 impaddr(RBin *bin, int va, ut64 baddr, const char *name) {
	RBinSymbol *symbol;
	RList *symbols;
	RListIter *iter;
	if (!name) return R_FALSE;
	if ((symbols = r_bin_get_symbols (bin)) == NULL)
		return R_FALSE;
	r_list_foreach (symbols, iter, symbol) {
		if (strncmp (symbol->name, "imp.", 4))
			continue;
		if (!strcmp (symbol->name+4, name))
			return va? r_bin_get_vaddr (bin, baddr, symbol->offset,
				symbol->rva): symbol->offset;
	}
	return 0;
}

static int bin_imports (RCore *r, int mode, ut64 baddr, int va, const char *name) {
	RBinImport *import;
	RListIter *iter;
	RList *imports;
	int i = 0;

	if ((imports = r_bin_get_imports (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (imports, iter, import)
			r_cons_printf ("%s{\"name\":\"%s\"}", iter->p?",":"", import->name);
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_SIMPLE)) {
		r_list_foreach (imports, iter, import)
			r_cons_printf ("%s\n", import->name);
	} else
	if ((mode & R_CORE_BIN_SET)) {
		// TODO(eddyb) use the logic below for symbols that are imports.
		/*r_flag_space_set (r->flags, "imports");
		r_list_foreach (imports, iter, import) {
			r_name_filter (import->name, 128);
			snprintf (str, R_FLAG_NAME_SIZE, "imp.%s", import->name);
			if (import->size)
				if (!r_anal_fcn_add (r->anal, va?baddr+import->rva:import->offset,
						import->size, str, R_ANAL_FCN_TYPE_IMP, NULL))
					eprintf ("Cannot add function: %s (duplicated)\n", import->name);
			r_flag_set (r->flags, str, va?baddr+import->rva:import->offset,
					import->size, 0);
			iname = import->name;
			p = strstr (iname+1, "__");
			if (p) iname = p+1;
			dname = r_bin_demangle (r->bin, iname);
			if (dname) {
				r_meta_add (r->anal->meta, R_META_TYPE_COMMENT,
						va? baddr+import->rva: import->offset,
						import->size, dname);
				free (dname);
			}
		}*/
	} else {
		ut64 addr;
		if (mode) r_cons_printf ("fs imports\n");
		else r_cons_printf ("[Imports]\n");

		r_list_foreach (imports, iter, import) {
			if (name && strcmp (import->name, name))
				continue;
			addr = impaddr (r->bin, va, baddr, import->name);
			if (mode) {
				// TODO(eddyb) use the logic below for symbols that are imports.
				/*r_name_filter (import->name, sizeof (import->name));
				iname = import->name;
				p = strstr (iname+1, "__");
				if (p) iname = p+1;
				mn = r_bin_demangle (r->bin, iname);
				if (mn) {
					//r_name_filter (mn, strlen (mn));
					r_cons_printf ("s 0x%08"PFMT64x"\n\"CC %s\"\n",
						import->offset, mn);
					free (mn);
				}
				if (import->size)
					r_cons_printf ("af+ 0x%08"PFMT64x" %"PFMT64d" imp.%s i\n",
							va?baddr+import->rva:import->offset,
							import->size, import->name);
				r_cons_printf ("f imp.%s @ 0x%08"PFMT64x"\n",
						import->name, va?baddr+import->rva:import->offset);*/
			} else r_cons_printf ("ordinal=%03"PFMT64d" plt=0x%08"PFMT64x" bind=%s type=%s name=%s\n",
					import->ordinal, addr,
					import->bind, import->type, import->name);
			i++;
		}
		if (!mode) r_cons_printf ("\n%i imports\n", i);
	}
	return R_TRUE;
}

static int bin_symbols (RCore *r, int mode, ut64 baddr, int va, ut64 at, const char *name) {
	char str[R_FLAG_NAME_SIZE];
	RList *symbols;
	RListIter *iter;
	RBinSymbol *symbol;
	int i = 0;

	if ((symbols = r_bin_get_symbols (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (symbols, iter, symbol) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, symbol->offset,
				symbol->rva): symbol->offset;
			r_cons_printf ("%s{\"name\":\"%s\","
				"\"size\":%"PFMT64d","
				"\"offset\":%"PFMT64d"}",
				iter->p?",":"", symbol->name, symbol->size, addr);
		}
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_SIMPLE)) {
		r_list_foreach (symbols, iter, symbol) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, symbol->offset,
				symbol->rva): symbol->offset;
			char *name = strdup (symbol->name);
			r_name_filter (name, 80);
			r_cons_printf ("0x%08"PFMT64x" %"PFMT64d" %s\n",
				addr, symbol->size, name);
			free (name);
		}
	} else
	if ((mode & R_CORE_BIN_SET)) {
		char *name, *dname, *cname;
		//ut8 cname_greater_than_15;
		r_flag_space_set (r->flags, "symbols");
		r_list_foreach (symbols, iter, symbol) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, symbol->offset,
				symbol->rva): symbol->offset;
			name = strdup (symbol->name);
			cname = symbol->classname ? strdup(symbol->classname) : NULL;
			// XXX - may want a configuration variable here for class and name lengths.
			// XXX - need something to handle overloaded symbols (e.g. methods)
			// void add (int i, int j);
			// void add (float i, int j);
			r_name_filter (name, 80);
			if (cname) {
				RFlagItem *flag_item = NULL;
				char * comment = NULL;
				r_name_filter (cname, 50);
				snprintf (str, R_FLAG_NAME_SIZE, "sym.%s", name);
				// check for a duplicate name sym.[name]
				flag_item = r_flag_get (r->flags, str);
				if (flag_item != NULL && (flag_item->offset - r->flags->base) == addr) {
					comment = flag_item->comment ? strdup(flag_item->comment) : NULL;
					r_flag_unset (r->flags, str, flag_item );
					flag_item = NULL;
				}
				// set the new sym.[cname].[name] with comment
				snprintf (str, R_FLAG_NAME_SIZE, "sym.%s.%s", cname, name);
				r_flag_set (r->flags, str, addr, symbol->size, 0);
				if (comment) {
					flag_item = r_flag_get (r->flags, str);
					if (flag_item) r_flag_item_set_comment (flag_item, comment);
					free(comment);
				}
			} else {
				snprintf (str, R_FLAG_NAME_SIZE, "sym.%s", name);
				r_flag_set (r->flags, str, addr, symbol->size, 0);
			}

			if (!strncmp (symbol->type, "OBJECT", 6))
					r_meta_add (r->anal, R_META_TYPE_DATA, addr,
                                            addr + symbol->size, name);

			dname = r_bin_demangle (r->bin, symbol->name);
			if (dname) {
				r_meta_add (r->anal, R_META_TYPE_COMMENT,
						addr, symbol->size, dname);
				free (dname);
			}
			free (name);
			free (cname);
			//r_meta_cleanup (r->anal->meta, 0LL, UT64_MAX);
		}
	} else {
		if (!at) {
			if (mode) r_cons_printf ("fs symbols\n");
			else r_cons_printf ("[Symbols]\n");
		}

		r_list_foreach (symbols, iter, symbol) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, symbol->offset,
				symbol->rva): symbol->offset;
			if (name && strcmp (symbol->name, name))
				continue;
			if (at) {
				if (symbol->size != 0 && ((at >= addr) && (at<(addr+symbol->size))))
					r_cons_printf ("%s\n", symbol->name);
			} else {
				if (mode) {
					char *mn = r_bin_demangle (r->bin, symbol->name);
					if (mn) {
						//r_name_filter (mn, strlen (mn));
						r_cons_printf ("s 0x%08"PFMT64x"\n\"CC %s\"\n",
							symbol->offset, mn);
						free (mn);
					}
					r_name_filter (symbol->name, sizeof (symbol->name));
					if (!strncmp (symbol->type,"OBJECT", 6)) {
						if (symbol->size == 0) {
							symbol->size = strlen (symbol->name);
							r_cons_printf ("Cs %"PFMT64d" @ 0x%08"PFMT64x"\n",
									symbol->size, addr);
						} else
						if (symbol->size>0) {
							r_cons_printf ("Cd %"PFMT64d" @ 0x%08"PFMT64x"\n",
									symbol->size, addr);
						} else eprintf ("Wrong symbol '%s' have size %"PFMT64d"\n",
								symbol->name, symbol->size);
					}
					r_cons_printf ("f sym.%s %"PFMT64d" 0x%08"PFMT64x"\n",
							symbol->name, symbol->size, addr);
				} else r_cons_printf ("addr=0x%08"PFMT64x" off=0x%08"PFMT64x" ord=%03"PFMT64d" "
						"fwd=%s sz=%"PFMT64d" bind=%s type=%s name=%s\n",
						addr, symbol->offset,
						symbol->ordinal, symbol->forwarder,
						symbol->size, symbol->bind, symbol->type,
						symbol->name);
			}
			i++;
		}
		if (!at && !mode) r_cons_printf ("\n%i symbols\n", i);
	}
	return R_TRUE;
}

static int bin_sections (RCore *r, int mode, ut64 baddr, int va, ut64 at, const char *name) {
	char str[R_FLAG_NAME_SIZE];
	RBinSection *section;
	ut64 secbase = 0LL;
	RList *sections;
	RListIter *iter;
	int i = 0;

	if ((sections = r_bin_get_sections (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (sections, iter, section) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, section->offset,
				section->rva): section->offset;
			r_cons_printf ("%s{\"name\":\"%s\","
				"\"size\":%"PFMT64d","
				"\"flags\":\"%s\","
				"\"offset\":%"PFMT64d"}",
				iter->p?",":"",
				section->name,
				section->size,
				r_str_rwx_i (section->srwx), addr);
		}
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_SIMPLE)) {
		r_list_foreach (sections, iter, section) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, section->offset,
				section->rva): section->offset;
			r_cons_printf ("0x%"PFMT64x" 0x%"PFMT64x" %s %s\n",
				addr, addr + section->size,
				r_str_rwx_i (section->srwx),
				section->name);
		}
	} else
	if ((mode & R_CORE_BIN_SET)) {
		RBinInfo *info = r_bin_get_info (r->bin);
		r_flag_space_set (r->flags, "sections");
		r_list_foreach (sections, iter, section) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, section->offset,
				section->rva): section->offset;
			if (!secbase || (section->rva && section->rva <secbase)) // ??
				secbase = section->rva;
			r_name_filter (section->name, 128);
			snprintf (str, R_FLAG_NAME_SIZE, "section.%s", section->name);
			r_flag_set (r->flags, str, addr, section->size, 0);
			snprintf (str, R_FLAG_NAME_SIZE, "section_end.%s", section->name);
			r_flag_set (r->flags, str, addr + section->size, 0, 0);
			r_io_section_add (r->io, section->offset, addr, section->size,
				section->vsize, section->srwx, section->name);
			if (section->arch || section->bits) {
				const char *arch = section->arch;
				int bits = section->bits;
				if (!arch) arch = info->arch;
				if (!bits) bits = info->bits;
				r_io_section_set_archbits (r->io, addr, arch, bits);
			}
			snprintf (str, R_FLAG_NAME_SIZE, "[%i] va=0x%08"PFMT64x" pa=0x%08"PFMT64x" sz=%"
					PFMT64d" vsz=%"PFMT64d" rwx=%c%c%c%c %s",
					i++, addr, section->offset, section->size, section->vsize,
					R_BIN_SCN_SHAREABLE (section->srwx)?'s':'-',
					R_BIN_SCN_READABLE (section->srwx)?'r':'-',
					R_BIN_SCN_WRITABLE (section->srwx)?'w':'-',
					R_BIN_SCN_EXECUTABLE (section->srwx)?'x':'-',
					section->name);
			r_meta_add (r->anal, R_META_TYPE_COMMENT, addr, addr, str);
		}
		// H -> Header fields
		if (0) {
			ut64 size = r_io_size (r->io);
			if (size == 0)
				size = r->file->size;
			secbase >>= 16;
			secbase <<= 16;
			secbase = baddr; // always override?
			r_io_section_add (r->io, 0, secbase, size, size, 7, "ehdr");
		}
	} else {
		RBinInfo *info = r_bin_get_info (r->bin);
		if (!at) r_cons_printf (mode? "fs sections\n": "[Sections]\n");

		r_list_foreach (sections, iter, section) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, section->offset,
				section->rva): section->offset;
			if (name && strcmp (section->name, name))
				continue;
			r_name_filter (section->name, sizeof (section->name));
			if (at) {
				if (section->size && ((at >= addr && (at<addr+section->size))))
					r_cons_printf ("%s\n", section->name);
			} else {
				if (mode) {
					r_cons_printf ("S 0x%08"PFMT64x" 0x%08"PFMT64x" 0x%08"PFMT64x" 0x%08"PFMT64x" %s %d\n",
						section->offset, addr, section->size, section->vsize,
						section->name, (int)section->srwx);
					if (section->arch || section->bits) {
						const char *arch = section->arch;
						int bits = section->bits;
						if (!arch) arch = info->arch;
						if (!bits) bits = info->bits;
						r_cons_printf ("Sa %s %d @ 0x%08"
							PFMT64x"\n", arch, bits, addr);
					}
					r_cons_printf ("f section.%s %"PFMT64d" 0x%08"PFMT64x"\n",
							section->name, section->size, addr);
					r_cons_printf ("f section_end.%s %"PFMT64d" 0x%08"PFMT64x"\n",
							section->name, (ut64)0, section->size+(va?baddr+section->rva:section->offset));
					r_cons_printf ("CC [%02i] va=0x%08"PFMT64x" pa=0x%08"PFMT64x" sz=%"PFMT64d" vsz=%"PFMT64d" "
							"rwx=%c%c%c%c %s @ 0x%08"PFMT64x"\n",
							i, addr, section->offset, section->size, section->vsize,
							R_BIN_SCN_SHAREABLE (section->srwx)?'s':'-',
							R_BIN_SCN_READABLE (section->srwx)?'r':'-',
							R_BIN_SCN_WRITABLE (section->srwx)?'w':'-',
							R_BIN_SCN_EXECUTABLE (section->srwx)?'x':'-',
							section->name, addr);
				} else {
					char str[128];
					if (section->arch || section->bits) {
						const char *arch = section->arch;
						int bits = section->bits;
						if (!arch) arch = info->arch;
						if (!bits) bits = info->bits;
						snprintf (str, sizeof (str), "arch=%s bits=%d ", arch, bits);
					} else str[0] = 0;
					r_cons_printf ("idx=%02i addr=0x%08"PFMT64x" off=0x%08"PFMT64x" sz=%"PFMT64d" vsz=%"PFMT64d" "
						"perm=%c%c%c%c %sname=%s\n",
						i, addr, section->offset, section->size, section->vsize,
						R_BIN_SCN_SHAREABLE (section->srwx)?'s':'-',
						R_BIN_SCN_READABLE (section->srwx)?'r':'-',
						R_BIN_SCN_WRITABLE (section->srwx)?'w':'-',
						R_BIN_SCN_EXECUTABLE (section->srwx)?'x':'-',
						str, section->name);
				}
			}
			i++;
		}
		if (!at && !mode) r_cons_printf ("\n%i sections\n", i);
	}

	return R_TRUE;
}

static int bin_fields (RCore *r, int mode, ut64 baddr, int va) {
	RList *fields;
	RListIter *iter;
	RBinField *field;
	int i = 0;
	ut64 size = r->bin->cur->size;

	if ((fields = r_bin_get_fields (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (fields, iter, field) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, field->offset,
				field->rva): field->offset;
			r_cons_printf ("%s{\"name\":\"%s\","
				"\"offset\":%"PFMT64d"}",
				iter->p?",":"",
				field->name, addr);
		}
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_SET)) {
		//XXX: Need more flags??
		r_io_section_add (r->io, 0, baddr, size, size, 7, "ehdr");
	} else {
		if (mode) r_cons_printf ("fs header\n");
		else r_cons_printf ("[Header fields]\n");

		r_list_foreach (fields, iter, field) {
			ut64 addr = va? r_bin_get_vaddr (r->bin, baddr, field->offset,
				field->rva): field->offset;
			if (mode) {
				r_name_filter (field->name, sizeof (field->name));
				r_cons_printf ("f header.%s @ 0x%08"PFMT64x"\n", field->name, addr);
				r_cons_printf ("[%02i] addr=0x%08"PFMT64x" off=0x%08"PFMT64x" name=%s\n",
						i, addr, field->offset, field->name);
			} else r_cons_printf ("idx=%02i addr=0x%08"PFMT64x" off=0x%08"PFMT64x" name=%s\n",
					i, addr, field->offset, field->name);
			i++;
		}

		if (mode) {
			/* add program header section */
			r_cons_printf ("S 0 0x%"PFMT64x" 0x%"PFMT64x" 0x%"PFMT64x" ehdr rwx\n",
					baddr, size, size);
		} else r_cons_printf ("\n%i fields\n", i);
	}
	return R_TRUE;
}

static int bin_classes (RCore *r, int mode) {
	RListIter *iter, *iter2;
	RBinClass *c;
	RList *cs = r_bin_get_classes (r->bin);
	const char *methname;
	if (!cs) return R_FALSE;

	// XXX: support for classes is broken and needs more love
	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (cs, iter, c) {
			if (c->super)
			r_cons_printf ("%s{\"name\":\"%s\",\"index\":%"PFMT64d",\"super\":\"%s\"}",
				iter->p?",":"", c->name, c->index, c->super);
			else
			r_cons_printf ("%s{\"name\":\"%s\",\"index\":%"PFMT64d"}",
				iter->p?",":"", c->name, c->index);
		}
		r_cons_printf ("]");
	} else
	if (mode & R_CORE_BIN_SIMPLE) {
		r_list_foreach (cs, iter, c) {
			r_cons_printf ("0x%08"PFMT64x"  %s  %s\n",
				c->index, c->name, c->super?c->super:"");
		}
	} else
	if (mode & R_CORE_BIN_SET) {
		// Nothing to set.
	} else {
		r_list_foreach (cs, iter, c) {
			if (mode) {
				r_cons_printf ("f class.%s @ %d\n", c->name, c->index);
				if (c->super)
					r_cons_printf ("f super.%s.%s @ %d\n", c->name, c->super, c->index);
				r_list_foreach (c->methods, iter2, methname) {
					r_cons_printf ("f method.%s.%s\n", c->name, methname);
				}
			} else {
				r_cons_printf ("class %d = %s\n", c->index, c->name);
				if (c->super)
					r_cons_printf ("  super = %s\n", c->super);
				r_list_foreach (c->methods, iter2, methname) {
					r_cons_printf ("  method %s\n", methname);
				}
			}
			// TODO: show belonging methods and fields
		}
	}
	return R_TRUE;
}

static int bin_size (RCore *r, int mode) {
	int size = r_bin_get_size (r->bin);
	if ((mode & R_CORE_BIN_SIMPLE) || mode&R_CORE_BIN_JSON)
		r_cons_printf ("%d\n", size);
	else if ((mode & R_CORE_BIN_RADARE))
		r_cons_printf ("f bin_size @ %d\n", size);
	else if ((mode & R_CORE_BIN_SET))
		r_core_cmdf (r, "f bin_size @ %d\n", size);
	else r_cons_printf ("%d\n", size);
	return R_TRUE;
}

static int bin_libs (RCore *r, int mode) {
	RList *libs;
	RListIter *iter;
	char* lib;
	int i = 0;

	if ((libs = r_bin_get_libs (r->bin)) == NULL)
		return R_FALSE;

	if (mode & R_CORE_BIN_JSON) {
		r_cons_printf ("[");
		r_list_foreach (libs, iter, lib) {
			r_cons_printf ("%s\"%s\"", iter->p?",":"",lib);
		}
		r_cons_printf ("]");
	} else
	if ((mode & R_CORE_BIN_RADARE)) {
		r_list_foreach (libs, iter, lib) {
			r_cons_printf ("CCa entry0 %s\n", lib);
		}
	} else
	if ((mode & R_CORE_BIN_SET)) {
		// Nothing to set.
		// TODO: load libraries with iomaps?
	} else {
		if (!mode) r_cons_printf ("[Linked libraries]\n");
		r_list_foreach (libs, iter, lib) {
			r_cons_printf ("%s\n", lib);
			i++;
		}
		if (!mode) r_cons_printf ("\n%i libraries\n", i);
	}
	return R_TRUE;
}

R_API int r_core_bin_info (RCore *core, int action, int mode, int va, RCoreBinFilter *filter, ut64 baseaddr) {
	int ret = R_TRUE;
	const char *name = NULL;
	ut64 at = 0;

	if (filter && filter->offset)
		at = filter->offset;
	if (filter && filter->name)
		name = filter->name;
	if ((action & R_CORE_BIN_ACC_STRINGS))
		ret &= bin_strings (core, mode, baseaddr, va);
	if ((action & R_CORE_BIN_ACC_INFO))
		ret &= bin_info (core, mode);
	if ((action & R_CORE_BIN_ACC_MAIN))
		ret &= bin_main (core, mode, baseaddr, va);
	if ((action & R_CORE_BIN_ACC_DWARF))
		ret &= bin_dwarf (core, mode);
	if ((action & R_CORE_BIN_ACC_ENTRIES))
		ret &= bin_entry (core, mode, baseaddr, va);
	if ((action & R_CORE_BIN_ACC_RELOCS))
		ret &= bin_relocs (core, mode, baseaddr, va);
	if ((action & R_CORE_BIN_ACC_IMPORTS))
		ret &= bin_imports (core, mode, baseaddr, va, name);
	if ((action & R_CORE_BIN_ACC_SYMBOLS))
		ret &= bin_symbols (core, mode, baseaddr, va, at, name);
	if ((action & R_CORE_BIN_ACC_SECTIONS))
		ret &= bin_sections (core, mode, baseaddr, va, at, name);
	if ((action & R_CORE_BIN_ACC_FIELDS))
		ret &= bin_fields (core, mode, baseaddr, va);
	if ((action & R_CORE_BIN_ACC_LIBS))
		ret &= bin_libs (core, mode);
	if ((action & R_CORE_BIN_ACC_CLASSES))
		ret &= bin_classes (core, mode);
	if ((action & R_CORE_BIN_ACC_SIZE))
		ret &= bin_size (core, mode);
	return ret;
}
