/* radare - LGPL - Copyright 2009-2013 - nibble, pancake */

#include "r_core.h"

R_API RAnalHint *r_core_hint_begin (RCore *core, RAnalHint* hint, ut64 at) {
// XXX not here
	static char *hint_arch = NULL;
	static int hint_bits = 0;
	if (hint) {
		r_anal_hint_free (hint);
		hint = NULL;
	}
	hint = r_anal_hint_get (core->anal, at);
	if (hint_arch) {
		r_config_set (core->config, "asm.arch", hint_arch);
		hint_arch = NULL;
	}
	if (hint_bits) {
		r_config_set_i (core->config, "asm.bits", hint_bits);
		hint_bits = 0;
	}
	if (hint) {
		/* arch */
		if (hint->arch) {
			if (!hint_arch) hint_arch = strdup (
				r_config_get (core->config, "asm.arch"));
			r_config_set (core->config, "asm.arch", hint->arch);
		}
		/* bits */
		if (hint->bits) {
			if (!hint_bits) hint_bits =
				r_config_get_i (core->config, "asm.bits");
			r_config_set_i (core->config, "asm.bits", hint->bits);
		}
	}
	return hint;
}

static char *filter_refline(const char *str) {
	char *p, *s = strdup (str);
	p = strstr (s, "->");
	if (p) p[0]=p[1]=' ';
	p = strstr (s, "=<");
	if (p) p[0]=p[1]=' ';
	for (p=s; *p; p++) {
		if (*p=='`') *p = '|';
		if (*p=='-') *p = ' ';
		if (*p=='=') *p = '|';
	}
	return s;
}

static void printoffset(ut64 off, int show_color, int invert, int opt) {
	if (show_color) {
		if (invert)
			r_cons_invert (R_TRUE, R_TRUE);
		if (opt) {
			ut32 s, a;
			a = off & 0xffff;
			s = (off-a)>>4;
			r_cons_printf (Color_GREEN"%04x:%04x"Color_RESET, s, a);
		} else r_cons_printf (Color_GREEN"0x%08"PFMT64x""Color_RESET, off);
		if (invert)
			r_cons_printf (Color_RESET);
		r_cons_puts ("  ");
	} else r_cons_printf ("0x%08"PFMT64x"  ", off);
}

// int l is for lines
R_API int r_core_print_disasm(RPrint *p, RCore *core, ut64 addr, ut8 *buf, int len, int l, int invbreak, int cbytes) {
	/* hints */
	RAnalHint *hint = NULL;
	/* other */
	int ret, idx = 0, i, j, k, lines, ostackptr = 0, stackptr = 0;
	char *line = NULL, *comment = NULL, *opstr, *osl = NULL; // old source line
	int continueoninvbreak = (len == l) && invbreak;
	char str[128], strsub[128];
	RAnalFunction *f = NULL;
	char *refline = NULL;
	RAnalCC cc = {0};
	ut8 *nbuf = NULL;
	int counter = 0;
	int middle = 0;
	ut64 dest = UT64_MAX;
	RAsmOp asmop;
	RAnalOp analop = {0};
	RFlagItem *flag;
	RMetaItem *mi;
	int oplen = 0;
	int tries = 3;

	//r_cons_printf ("len =%d l=%d ib=%d limit=%d\n", len, l, invbreak, p->limit);
	// TODO: import values from debugger is possible
	// TODO: allow to get those register snapshots from traces
	// TODO: per-function register state trace

	// TODO: All those options must be print flags
	int show_color = r_config_get_i (core->config, "scr.color");
	int acase = r_config_get_i (core->config, "asm.ucase");
	int atabs = r_config_get_i (core->config, "asm.tabs");
	int decode = r_config_get_i (core->config, "asm.decode");
	int pseudo = r_config_get_i (core->config, "asm.pseudo");
	int filter = r_config_get_i (core->config, "asm.filter");
	int varsub = r_config_get_i (core->config, "asm.varsub");
	int show_lines = r_config_get_i (core->config, "asm.lines");
	int linesright = r_config_get_i (core->config, "asm.linesright");
#warning asm.dwarf is now marked as experimental and disabled
	int show_dwarf = 0; // r_config_get_i (core->config, "asm.dwarf");
	int show_linescall = r_config_get_i (core->config, "asm.linescall");
	int show_size = r_config_get_i (core->config, "asm.size");
	int show_trace = r_config_get_i (core->config, "asm.trace");
	int linesout = r_config_get_i (core->config, "asm.linesout");
	int adistrick = r_config_get_i (core->config, "asm.middle"); // TODO: find better name
	int show_offset = r_config_get_i (core->config, "asm.offset");
	int show_offseg = r_config_get_i (core->config, "asm.segoff");
	int show_flags = r_config_get_i (core->config, "asm.flags");
	int show_bytes = r_config_get_i (core->config, "asm.bytes");
	int show_comments = r_config_get_i (core->config, "asm.comments");
	int show_stackptr = r_config_get_i (core->config, "asm.stackptr");
	int show_xrefs = r_config_get_i (core->config, "asm.xrefs");
	int show_functions = r_config_get_i (core->config, "asm.functions");
	int cursor, nb, nbytes = r_config_get_i (core->config, "asm.nbytes");
	int show_comment_right_default = r_config_get_i (core->config, "asm.cmtright");
	int flagspace_ports = r_flag_space_get (core->flags, "ports");
	int lbytes = r_config_get_i (core->config, "asm.lbytes");
	int show_comment_right = 0;
	const char *pre = "  ";
	char *ocomment = NULL;
	int linesopts = 0;
	int lastfail = 0;
	int ocols = 0;
	int lcols = 0;

	if (show_lines) ocols += 10; // XXX
	if (show_offset) ocols += 14;
	lcols = ocols+2;
	if (show_bytes) ocols += 20;
	if (show_trace) ocols += 8;
	if (show_stackptr) ocols += 4;
	/* disasm */ ocols += 20;

	nb = (nbytes*2);
	core->inc = 0;

	if (core->print->cur_enabled) {
		if (core->print->cur<0)
			core->print->cur = 0;
		cursor = core->print->cur;
	} else cursor = -1;

	if (r_config_get_i (core->config, "asm.linesstyle"))
		linesopts |= R_ANAL_REFLINE_TYPE_STYLE;
	if (r_config_get_i (core->config, "asm.lineswide"))
		linesopts |= R_ANAL_REFLINE_TYPE_WIDE;
	lines = 0;
toro:
	// uhm... is this necesary? imho can be removed
	r_asm_set_pc (core->assembler, addr+idx);
#if 0
	/* find last function else stackptr=0 */
	{
		RAnalFunction *fcni;
		RListIter *iter;

		r_list_foreach (core->anal.fcns, iter, fcni) {
			if (addr >= fcni->addr && addr<(fcni->addr+fcni->size)) {
				stack_ptr = fcni->stack;
				r_cons_printf ("/* function: %s (%d) */\n", fcni->name, fcni->size, stack_ptr);
				break;
			}
		}
	}
#endif
	if (core->print->cur_enabled) {
		// TODO: support in-the-middle-of-instruction too
		if (r_anal_op (core->anal, &analop, core->offset+core->print->cur,
			buf+core->print->cur, (int)(len-core->print->cur))) {
			// TODO: check for analop.type and ret
			dest = analop.jump;
#if 0
			switch (analop.type) {
			case R_ANAL_OP_TYPE_JMP:
			case R_ANAL_OP_TYPE_CALL:
				dest = analop.jump;
				break;
			}
#endif
		}
	} else {
		/* highlight eip */
		RFlagItem *item;
		const char *pc = core->anal->reg->name[R_REG_NAME_PC];
		item = r_flag_get (core->flags, pc);
		if (item)
			dest = item->offset;
	}
	if (show_lines) {
		// TODO: make anal->reflines implicit
		free (core->reflines); // TODO: leak
		free (core->reflines2); // TODO: leak
		core->reflines = r_anal_reflines_get (core->anal,
			addr, buf, len, -1, linesout, show_linescall);
		core->reflines2 = r_anal_reflines_get (core->anal,
			addr, buf, len, -1, linesout, 1);
	} else core->reflines = core->reflines2 = NULL;

	oplen = 1;
	for (i=idx=ret=0; idx < len && lines < l; idx+=oplen,i++, lines++) {
		ut64 at = addr + idx;

		hint = r_core_hint_begin (core, hint, at);
		if (cbytes && idx>=l)
			break;
		r_asm_set_pc (core->assembler, at);
		if (show_lines) {
			line = r_anal_reflines_str (core->anal,
				core->reflines, at, linesopts);
			refline = filter_refline (line);
		} else {
			line = NULL;
			refline = strdup ("");
		}
		f = show_functions? r_anal_fcn_find (core->anal, at,
			R_ANAL_FCN_TYPE_NULL): NULL;
		// Show xrefs
		if (show_xrefs) {
			RList *xrefs;
			RAnalRef *refi;
			RListIter *iter;
			if ((xrefs = r_anal_xref_get (core->anal, at))) {
				r_list_foreach (xrefs, iter, refi) {
					RAnalFunction *fun = r_anal_fcn_find (
						core->anal, refi->addr,
						R_ANAL_FCN_TYPE_NULL);
					r_cons_printf ("%c %s", ((f&&f->type==R_ANAL_FCN_TYPE_FCN)
						&&f->addr==at)?' ':'|',refline);
					if (show_color)
					r_cons_printf (Color_TURQOISE"; %s XREF 0x%08"PFMT64x" (%s)"Color_RESET"\n",
						refi->type==R_ANAL_REF_TYPE_CODE?"CODE (JMP)":
						refi->type==R_ANAL_REF_TYPE_CALL?"CODE (CALL)":"DATA", refi->addr,
						fun?fun->name:"unk");
					else r_cons_printf ("; %s XREF 0x%08"PFMT64x" (%s)\n",
						refi->type==R_ANAL_REF_TYPE_CODE?"CODE (JMP)":
						refi->type==R_ANAL_REF_TYPE_CALL?"CODE (CALL)":"DATA", refi->addr,
						fun?fun->name:"unk");
				}
				r_list_free (xrefs);
			}
		}

		/* show comment at right? */
		show_comment_right = 0;
		if (show_comments) {
			RFlagItem *item = r_flag_get_i (core->flags, at);
			comment = r_meta_get_string (core->anal->meta, R_META_TYPE_COMMENT, at);
			if (!comment && item && item->comment) {
				ocomment = item->comment;
				comment = strdup (item->comment);
			}
			if (comment) {
				int linelen, maxclen = strlen (comment)+5;
				linelen = maxclen;
				if (show_comment_right_default)
				if (ocols+maxclen < core->cons->columns) {
					if (comment && *comment && strlen (comment)<maxclen) {
						char *p = strchr (comment, '\n');
						if (p) {
							linelen = p-comment;
							if (!strchr (p+1, '\n')) // more than one line?
								show_comment_right = 1;
						}
					}
				}
				if (!show_comment_right) {
					int mycols = lcols;
					if (mycols + linelen + 10 > core->cons->columns)
						mycols = 0;
					mycols /= 2;
					if (show_color) r_cons_strcat (Color_TURQOISE);
// XXX: always prefix with ; the comments
				//	if (*comment != ';') r_cons_strcat ("  ;  ");
					r_cons_strcat_justify (comment, mycols, ';');
					if (show_color) r_cons_strcat (Color_RESET);
					if (!strchr (comment, '\n')) r_cons_newline ();
					free (comment);
					comment = NULL;

					/* flag one */
					if (item && item->comment && ocomment != item->comment) {
						if (show_color) r_cons_strcat (Color_TURQOISE);
						r_cons_newline ();
						r_cons_strcat ("  ;  ");
						r_cons_strcat_justify (item->comment, mycols, ';');
						r_cons_newline ();
						if (show_color) r_cons_strcat (Color_RESET);
					}
				}
			}
		}
		// TODO : line analysis must respect data types! shouldnt be interpreted as code
		ret = r_asm_disassemble (core->assembler, &asmop, buf+idx, len-idx);
		if (ret<1) { // XXX: move to r_asm_disassemble ()
			oplen = ret = 1;
			//eprintf ("** invalid opcode at 0x%08"PFMT64x" %d %d**\n",
			//	core->assembler->pc + ret, l, len);
#if 1
//eprintf ("~~~~~~LEN~~~~ %d %d %d\n", l, len, lines);
			if (!cbytes && tries>0) { //1||l < len) {
//eprintf ("~~~~~~~~~~~~~ %d %d\n", idx, core->blocksize);
				addr = core->assembler->pc;
				tries--;
				//eprintf ("-- %d %d\n", len, r_core_read_at (core, addr, buf, len));
				//eprintf ("REtry 0x%llx -- %x %x\n", addr, buf[0], buf[1]);
				idx = 0;
				goto retry;
			}
#endif
			lastfail = 1;
			strcpy (asmop.buf_asm, "invalid");
			sprintf (asmop.buf_hex, "%02x", buf[idx]);
		} else {
			lastfail = 0;
			if (hint && hint->length)
				oplen = hint->length;
			else oplen = r_asm_op_get_size (&asmop);
		}
		if (acase)
			r_str_case (asmop.buf_asm, 1);
		if (atabs) {
			int i = 0;
			char *b = asmop.buf_asm;
			for (;*b;b++,i++) {
				if (*b==' ') {
					//*b = '\t';
					int n = (10-i);
					char *t = strdup (b+1); //XXX slow!
					if (n<1) n = 1;
					memset (b, ' ', n);
					b += n;
					strcpy (b, t);
					free (t);
				}
			}
		}
		// TODO: store previous oplen in core->dec
		if (core->inc == 0)
			core->inc = oplen;

		r_anal_op_fini (&analop);
		if (!lastfail)
			r_anal_op (core->anal, &analop, at, buf+idx, (int)(len-idx));
		if (hint && hint->length)
			analop.length = hint->length;
		{
			RAnalValue *src;
			switch (analop.type) {
			case R_ANAL_OP_TYPE_MOV:
				src = analop.src[0];
				if (src && src->memref>0 && src->reg) {
					if (core->anal->reg && core->anal->reg->name) {
						const char *pc = core->anal->reg->name[R_REG_NAME_PC];
						RAnalValue *dst = analop.dst;
						if (dst && dst->reg && dst->reg->name)
						if (!strcmp (src->reg->name, pc)) {
							RFlagItem *item;
							ut8 b[8];
							ut64 ptr = idx+addr+src->delta+analop.length;
							ut64 off = 0LL;
							r_core_read_at (core, ptr, b, src->memref);
							off = r_mem_get_num (b, src->memref, 1);
							item = r_flag_get_i (core->flags, off);
							r_cons_printf ("; MOV %s = [0x%"PFMT64x"] = 0x%"PFMT64x" %s\n",
									dst->reg->name, ptr, off, item?item->name: "");
						}
					}
				}
				break;
#if 1
// TODO: get from meta anal?
			case R_ANAL_OP_TYPE_LEA:
				src = analop.src[0];
				if (src && src->reg && core->anal->reg && core->anal->reg->name) {
					const char *pc = core->anal->reg->name[R_REG_NAME_PC];
					RAnalValue *dst = analop.dst;
					if (dst && dst->reg && !strcmp (src->reg->name, pc)) {
						int memref = core->assembler->bits/8;
						RFlagItem *item;
						ut8 b[64];
						ut64 ptr = idx+addr+src->delta+analop.length;
						ut64 off = 0LL;
						r_core_read_at (core, ptr, b, sizeof (b)); //memref);
						off = r_mem_get_num (b, memref, 1);
						item = r_flag_get_i (core->flags, off);
						{ char s[64];
						r_str_ncpy (s, (const char *)b, sizeof (s));
						r_cons_printf ("; LEA %s = [0x%"PFMT64x"] = 0x%"PFMT64x" \"%s\"\n",
								dst->reg->name, ptr, off, item?item->name: s);
						}
					}
				}
#endif
			}
		}
		if (adistrick)
			middle = r_anal_reflines_middle (core->anal,
					core->reflines, at, analop.length);
		/* XXX: This is really cpu consuming.. need to be fixed */
		if (show_functions) {
			if (f) {
//eprintf ("fun    0x%llx 0x%llx\n", at, f->addr+f->size-analop.length);
				pre = "  ";

				if (f->addr == at) {
					char *sign = r_anal_fcn_to_string (core->anal, f);
					if (f->type == R_ANAL_FCN_TYPE_LOC) {
						r_cons_printf ("|- %s (%d)\n| ", f->name, f->size);
					} else {
						const char *fmt = show_color? 
							"/ "Color_MAGENTA"%s: "Color_YELLOW"%s"Color_RESET" (%d)\n| ":
							"/ %s: %s (%d)\n| ";
						r_cons_printf (fmt, 
							(f->type==R_ANAL_FCN_TYPE_FCN||f->type==R_ANAL_FCN_TYPE_SYM)?"function":
							(f->type==R_ANAL_FCN_TYPE_IMP)?"import":"loc",
							f->name, f->size);
					}
					if (sign) r_cons_printf ("// %s\n", sign);
					free (sign);
					pre = "| ";
					stackptr = 0;
				} else if (f->addr+f->size-analop.length == at) {
					r_cons_printf ("\\ ");
				} else if (at > f->addr && at < f->addr+f->size-1) {
					r_cons_printf ("| ");
					pre = "| ";
				} else f = NULL;
				if (f && at == f->addr+f->size-analop.length) // HACK
					pre = "\\ ";
			} else pre = "  "; //r_cons_printf ("  ");
		}
		if (show_flags) {
			flag = r_flag_get_i (core->flags, at);
			if (flag) {
				if (show_lines && refline)
					r_cons_strcat (refline);
				if (show_offset)
					r_cons_printf ("; -------- ");
				if (show_functions)
					r_cons_printf ("%s:\n%s", flag->name, f?pre:"");
				else r_cons_printf ("%s:\n", flag->name);
			}
		}
		if (!linesright && show_lines && line) r_cons_strcat (line);
		if (show_offset)
			printoffset (at, show_color, (at==dest), show_offseg);
		if (show_size)
			r_cons_printf ("%d ", analop.length);
		if (show_trace) {
			RDebugTracepoint *tp = r_debug_trace_get (core->dbg, at);
			r_cons_printf ("%02x:%04x ", tp?tp->times:0, tp?tp->count:0);
		}
		if (show_stackptr) {
			r_cons_printf ("%3d%s", stackptr,
				analop.type==R_ANAL_OP_TYPE_CALL?">":
				stackptr>ostackptr?"+":stackptr<ostackptr?"-":" ");
			ostackptr = stackptr;
			stackptr += analop.stackptr;
			/* XXX if we reset the stackptr 'ret 0x4' has not effect.
			 * Use RAnalFunction->RAnalOp->stackptr? */
			if (analop.type == R_ANAL_OP_TYPE_RET)
				stackptr = 0;
		}
		// TODO: implement ranged meta find (if not at the begging of function..
		mi = r_meta_find (core->anal->meta, at, R_META_TYPE_ANY,
			R_META_WHERE_HERE);
		if (mi)
		switch (mi->type) {
		case R_META_TYPE_STRING:
			{
			char *out = r_str_unscape (mi->str);
			if (show_color)
				r_cons_printf ("    .string "Color_YELLOW"\"%s\""
					Color_RESET" ; len=%"PFMT64d"\n", out, mi->size);
			else r_cons_printf ("    .string \"%s\" ; len=%"PFMT64d
					"\n", out, mi->size);
			free (out);
			}
			oplen = ret = (int)mi->size;
			i += mi->size-1; // wtf?
			free (line);
			free (refline);
			line = refline = NULL;
			continue;
		case R_META_TYPE_HIDE:
			r_cons_printf ("(%d bytes hidden)\n", mi->size);
			oplen = mi->size;
			continue;
		case R_META_TYPE_DATA:
			{
				int hexlen = len - idx;
				int delta = at-mi->from;
				if (mi->size<hexlen)
					hexlen = mi->size;
				core->print->flags &= ~R_PRINT_FLAGS_HEADER;
				r_cons_printf ("hex length=%lld delta=%d\n", mi->size , delta);
				r_print_hexdump (core->print, at, buf+idx, hexlen, 16, 1);
			core->inc = 16;
				core->print->flags |= R_PRINT_FLAGS_HEADER;
				oplen = ret = (int)mi->size; //-delta;
				free (line);
				free (refline);
				line = refline = NULL;
			}
			continue;
		case R_META_TYPE_FORMAT:
			r_cons_printf ("format %s {\n", mi->str);
			r_print_format (core->print, at, buf+idx, len-idx, mi->str, -1, NULL);
			r_cons_printf ("} %d\n", mi->size);
			oplen = ret = (int)mi->size;
			free (line);
			free (refline);
			line = refline = NULL;
			continue;
		}
		/* show cursor */
		{
			int q = core->print->cur_enabled && cursor >= idx && cursor < (idx+oplen);
			void *p = r_bp_get (core->dbg->bp, at);
			r_cons_printf (p&&q?"b*":p? "b ":q?"* ":"  ");
		}
		if (show_bytes) {
			char *str = NULL, pad[64];
			char extra[64];
			strcpy (extra, " ");
			flag = NULL; // HACK
			if (!flag) {
				str = strdup (asmop.buf_hex);
				if (r_str_ansi_len (str) > nb) {
					char *p = (char *)r_str_ansi_chrn (str, nb);
					if (p)  {
						p[0] = '.';
						p[1] = '\0';
					}
					*extra = 0;
				}
				k = nb-r_str_ansi_len (str);
				if (k<0) k = 0;
				for (j=0; j<k; j++)
					pad[j] = ' ';
				pad[j] = 0;
				if (lbytes) {
					// hack to align bytes left
					strcpy (extra, pad);
					*pad = 0;
				}
			//	if (show_color) {
					char *nstr;
					p->cur_enabled = cursor!=-1;
					//p->cur = cursor;
					nstr = r_print_hexpair (p, str, idx);
					free (str);
					str = nstr;
			//	}
			} else {
				str = strdup (flag->name);
				k = nb-strlen (str)-2;
				if (k<0) k = 0;
				for (j=0; j<k; j++)
					pad[j] = ' ';
				pad[j] = '\0';
			}
			if (show_color)
				r_cons_printf (" %s %s %s"Color_RESET, pad, str, extra);
			else r_cons_printf (" %s %s %s", pad, str, extra);
			free (str);
		}

		if (linesright && show_lines && line) r_cons_strcat (line);
		if (show_color) {
			switch (analop.type) {
			case R_ANAL_OP_TYPE_NOP:
				r_cons_printf (Color_BLUE);
				break;
			case R_ANAL_OP_TYPE_JMP:
			case R_ANAL_OP_TYPE_CJMP:
			case R_ANAL_OP_TYPE_UJMP:
				r_cons_printf (Color_GREEN);
				break;
			case R_ANAL_OP_TYPE_CMP:
				r_cons_printf (Color_BMAGENTA);
				break;
			case R_ANAL_OP_TYPE_UCALL:
			case R_ANAL_OP_TYPE_CALL:
				r_cons_printf (Color_BGREEN);
				break;
			case R_ANAL_OP_TYPE_SWI:
				r_cons_printf (Color_MAGENTA);
				break;
			case R_ANAL_OP_TYPE_TRAP:
				r_cons_printf (Color_BRED);
				break;
			case R_ANAL_OP_TYPE_RET:
				r_cons_printf (Color_RED);
				break;
			case R_ANAL_OP_TYPE_PUSH:
			case R_ANAL_OP_TYPE_UPUSH:
			case R_ANAL_OP_TYPE_LOAD:
				r_cons_printf (Color_YELLOW);
				break;
			case R_ANAL_OP_TYPE_POP:
			case R_ANAL_OP_TYPE_STORE:
				r_cons_printf (Color_BYELLOW);
				break;
			}
		}
		opstr = NULL;
		if (decode) {
			char *tmpopstr = r_anal_op_to_string (core->anal, &analop);
			// TODO: Use data from code analysis..not raw analop here
			// if we want to get more information
			opstr = tmpopstr? tmpopstr: strdup (asmop.buf_asm);
		}
		if (hint && hint->opcode) {
			free (opstr);
			opstr = strdup (hint->opcode);
		}
		if (filter) {
			int ofs = core->parser->flagspace;
			int fs = flagspace_ports;
			if (analop.type == R_ANAL_OP_TYPE_IO) {
				core->parser->notin_flagspace = -1;
				core->parser->flagspace = fs;
			} else {
				if (fs != -1) {
					core->parser->notin_flagspace = fs;
					core->parser->flagspace = fs;
				} else {
					core->parser->notin_flagspace = -1;
					core->parser->flagspace = -1;
				}
			}
			r_parse_filter (core->parser, core->flags,
				opstr? opstr: asmop.buf_asm, str, sizeof (str));
			core->parser->flagspace = ofs;
			free (opstr);
			opstr = strdup (str);
			core->parser->flagspace = ofs; // ???
		} else 
		if (!opstr) opstr = strdup (asmop.buf_asm);
		if (pseudo) {
			r_parse_parse (core->parser, opstr?
				opstr:asmop.buf_asm, str);
			free (opstr);
			opstr = strdup (str);
		}
		if (varsub) {
			RAnalFunction *f = r_anal_fcn_find (core->anal,
				at, R_ANAL_FCN_TYPE_NULL);
			if (f) {
				r_parse_varsub (core->parser, f,
					opstr, strsub, sizeof (strsub));
				free (opstr);
				opstr = strdup (strsub);
			}
		}
		r_cons_strcat (opstr);
		free (opstr);
		opstr = NULL;
		if (show_color)
			r_cons_strcat (Color_RESET);
		if (show_dwarf) {
			char *sl = r_bin_meta_get_source_line (core->bin, at);
			int len = strlen (opstr);
			if (len<30) len = 30-len;
			if (sl) {
				if ((!osl || (osl && strcmp (sl, osl)))) {
					while (len--)
						r_cons_strcat (" ");
					if (show_color)
						r_cons_printf (Color_TURQOISE
						"  ; %s"Color_RESET"%s", sl, pre);
					else r_cons_printf ("  ; %s\n%s", sl, pre);
					free (osl);
					osl = sl;
				}
			} else {
				eprintf ("Warning: Forced asm.dwarf=false because of error\n");
				show_dwarf = R_FALSE;
				r_config_set (core->config, "asm.dwarf", "false");
			}
		}
		if (middle != 0) {
			ret -= middle;
			r_cons_printf (" ;  *middle* %d", ret);
		}
		if (asmop.payload != 0)
			r_cons_printf ("\n; .. payload of %d bytes", asmop.payload);
		if (core->assembler->syntax != R_ASM_SYNTAX_INTEL) {
			RAsmOp ao; /* disassemble for the vm .. */
			int os = core->assembler->syntax;
			r_asm_set_syntax (core->assembler, R_ASM_SYNTAX_INTEL);
			r_asm_disassemble (core->assembler, &ao, buf+idx, len-idx+5);
			r_asm_set_syntax (core->assembler, os);
		}

		if (!r_anal_cc_update (core->anal, &cc, &analop)) {
			if (show_functions) {
				char *ccstr = r_anal_cc_to_string (core->anal, &cc);
				if (ccstr) {
					RFlagItem *flag = r_flag_get_at (core->flags, cc.jump);
					if (show_color)
						r_cons_printf ("\n%s%s   "Color_TURQOISE"; %s (%s+%d)"Color_RESET,
							f?pre:"", refline, ccstr, flag? flag->name: "", (f&&flag)? cc.jump-flag->offset: 0);
					else r_cons_printf ("\n%s%s    ; %s (%s+%d)",
						pre, refline, ccstr,
						flag?flag->name:"", flag? cc.jump-flag->offset: 0);
					free (ccstr);
				}
			}
			r_anal_cc_reset (&cc);
		}

		if (core->vmode)
		switch (analop.type) {
		case R_ANAL_OP_TYPE_JMP:
		case R_ANAL_OP_TYPE_CJMP:
		case R_ANAL_OP_TYPE_CALL:
			counter++;
			if (counter<10){
				core->asmqjmps[counter] = analop.jump;
				r_cons_printf (" [%d]", counter);
			} else r_cons_strcat (" [?]");
			break;
		}
		switch (analop.type) {
		case R_ANAL_OP_TYPE_PUSH:
			if (analop.value) {
				RFlagItem *flag = r_flag_get_i (core->flags, analop.value);
				if (flag) r_cons_printf (" ; %s", flag->name);
			}
			break;
		}

		if (analop.refptr) {
			ut64 word8 = 0;
			ut32 word4 = 0;
			int ret;
			if (core->assembler->bits==64) {
				ret = r_io_read_at (core->io, analop.ref,
					(void *)&word8, sizeof (word8))
					== sizeof (word8);
			} else {
				ret = r_io_read_at (core->io, analop.ref,
					(void *)&word4, sizeof (word4))
					== sizeof (word4);
				word8 = word4;
			}
			
			if (ret) {
				RMetaItem *mi2 = r_meta_find (core->anal->meta, word8,
					R_META_TYPE_ANY, R_META_WHERE_HERE);
				if (!mi2) {
					mi2 = r_meta_find (core->anal->meta, (ut64)analop.ref,
						R_META_TYPE_ANY, R_META_WHERE_HERE);
					if (mi2) {
						char *str = r_str_unscape (mi2->str);
						r_cons_printf (" \"%s\" @ 0x%08"PFMT64x":%"PFMT64d,
								str, analop.ref, mi2->size);
						free (str);
					} else r_cons_printf (" ; 0x%08x [0x%"PFMT64x"]",
							word8, analop.ref);
				} else {
					if (mi2->type == R_META_TYPE_STRING) {
						char *str = r_str_unscape (mi2->str);
						r_cons_printf (" (at=0x%08"PFMT64x") (len=%"PFMT64d
							") \"%s\" ", word8, mi2->size, str);
						free (str);
					} else r_cons_printf ("unknown type '%c'\n", mi2->type);
				}
			} else {
				st64 sref = analop.ref;
				if (sref>0)
					r_cons_printf (" ; 0x%08"PFMT64x"\n", analop.ref); //addr+idx+analop.ref);
			}
		} else {
			if (analop.ref != UT64_MAX && analop.ref) {
				r_cons_printf (" ; 0x%08"PFMT64x" ", analop.ref);
			}
		}
		if (show_comments && show_comment_right && comment) {
			int c = r_cons_get_column ();
			if (c<ocols)
				r_cons_memset (' ',ocols-c);
			if (show_color) r_cons_strcat (Color_TURQOISE);
			r_cons_strcat ("  ; ");
	//		r_cons_strcat_justify (comment, strlen (refline) + 5, ';');
			r_cons_strcat (comment);
			if (show_color) r_cons_strcat (Color_RESET);
			free (comment);
			comment = NULL;
		} else r_cons_newline ();
		if (line) {
			if (show_lines && analop.type == R_ANAL_OP_TYPE_RET) {
				if (strchr (line, '>'))
					memset (line, ' ', strlen (line));
				r_cons_printf ("%s%s; ------------\n", (f&&show_functions)?"  ":"", line);
			}
			free (line);
			free (refline);
			line = refline = NULL;
		}
	}
	if (nbuf == buf) {
		free (buf);
		buf = NULL;
	}
#if 1
	if (!cbytes && idx>=len) {// && (invbreak && !lastfail)) {
	retry:
		if (len<4) len = 4;
		buf = nbuf = malloc (len);
		if (tries>0) {
			addr += idx;
			if (r_core_read_at (core, addr, buf, len) ) {
				idx = 0;
				goto toro;
			}
		}
		//if (invbreak && lines<l) {
		if (lines<l) {
//eprintf ("RETR %d\n", );
			addr += idx;
			if (r_core_read_at (core, addr, buf, len) != len) {
				//tries = -1;
			}
			goto toro;
		}
		if (continueoninvbreak)
			goto toro;
	}
#endif
	r_anal_op_fini (&analop);
	if (hint) r_anal_hint_free (hint);
	free (osl);
	return idx-lastfail;
}

R_API int r_core_print_disasm_json(RCore *core, ut64 addr, ut8 *buf, int len) {
	RAsmOp asmop;
	RAnalOp analop;
	int i, oplen, ret;
	r_cons_printf ("[");
	// TODO: add support for anal hints
	for (i=0; i<len;) {
		ut64 at = addr +i;
		r_asm_set_pc (core->assembler, at);
		ret = r_asm_disassemble (core->assembler, &asmop, buf+i, len-i+5);
		if (ret<1) {
			r_cons_printf ("%s{", i>0? ",": "");
			r_cons_printf ("\"offset\":%"PFMT64d, at);
			r_cons_printf (",\"size\":1,\"type\":\"invalid\"}");
			i++;
			continue;
		}
		r_anal_op (core->anal, &analop, at, buf+i, len-i+5);

		oplen = r_asm_op_get_size (&asmop);
		r_cons_printf ("%s{", i>0? ",": "");
		r_cons_printf ("\"offset\":%"PFMT64d, at);
		r_cons_printf (",\"size\":%d", oplen);
		r_cons_printf (",\"opcode\":\"%s\"", asmop.buf_asm);
		r_cons_printf (",\"bytes\":\"%s\"", asmop.buf_hex);
		//r_cons_printf (",\"family\":\"%s\"", asmop.family);
		r_cons_printf (",\"type\":\"%s\"", r_anal_optype_to_string (analop.type));
		if (analop.jump != UT64_MAX) {
			r_cons_printf (",\"next\":%"PFMT64d, analop.jump);
			if (analop.fail != UT64_MAX)
				r_cons_printf (",\"fail\":%"PFMT64d, analop.fail);
		}
		r_cons_printf ("}");
		i += oplen;
	}
	r_cons_printf ("]");
	return R_TRUE;
}

R_API int r_core_print_disasm_instructions (RCore *core, int len, int l) {
	RAsmOp asmop;
	char *opstr, *tmpopstr;
	int i, j, ret, err = 0;
	const ut8 *buf = core->block;
	int bs = core->blocksize;
	RAnalOp analop = {0};
	RAnalHint *hint = NULL;
	int decode = r_config_get_i (core->config, "asm.decode");
	ut64 at;

	if (len>core->blocksize)
		r_core_block_size (core, len);
	if (l==0) l = len;
	for (i=j=0; i<bs && i<len && j<l; i+=ret, j++) {
		at = core->offset +i;
		if (hint) {
			r_anal_hint_free (hint);
			hint = NULL;
		}
		hint = r_core_hint_begin (core, hint, at);
		r_asm_set_pc (core->assembler, at);
		ret = r_asm_disassemble (core->assembler,
			&asmop, buf+i, core->blocksize-i);
		//r_cons_printf ("0x%08"PFMT64x"  ", core->offset+i);
		if (hint && hint->length)
			ret = hint->length;
		if (hint && hint->opcode) {
			opstr = strdup (hint->opcode);
		} else {
			if (decode) {
				r_anal_op (core->anal, &analop, at, buf+i, core->blocksize-i);
				tmpopstr = r_anal_op_to_string (core->anal, &analop);
				opstr = (tmpopstr)? tmpopstr: strdup (asmop.buf_asm);
			} else opstr = strdup (asmop.buf_asm);
		}
		if (ret<1) {
			ret = err = 1;
			r_cons_printf ("???\n");
		} else {
			r_cons_printf ("%s\n", opstr);
			free (opstr);
		}
	}
	if (hint) r_anal_hint_free (hint);
	return 0;
}
