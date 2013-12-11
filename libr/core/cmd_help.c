/* radare - LGPL - Copyright 2009-2013 - pancake */

static int cmd_help(void *data, const char *input) {
	RCore *core = (RCore *)data;
	const char *k;
	char *p, out[128];
	ut64 n, n2;
	int i;

	switch (input[0]) {
	case 'r':
		{ // TODO : Add support for 64bit random numbers
		ut64 b = 0;
		ut32 r = UT32_MAX;
		if (input[1]) {
			strncpy (out, input+(input[1]==' '? 2: 1), sizeof (out)-1);
			p = strchr (out+1, ' ');
			if (p) {
				*p = 0;
				b = (ut32)r_num_math (core->num, out);
				r = (ut32)r_num_math (core->num, p+1)-b;
			} else r = (ut32)r_num_math (core->num, out);
		} else r = 0LL;
		if (r == 0)
			r = UT32_MAX>>1;
		core->num->value = (ut64) (b + r_num_rand (r));
		r_cons_printf ("0x%"PFMT64x"\n", core->num->value);
		}
		break;
	case 'b':
		n = r_num_get (core->num, input+1);
		r_num_to_bits (out, n);
		r_cons_printf ("%sb\n", out);
		break;
	case 'B':
		k = r_str_chop_ro (input+1);
		r_core_get_boundaries (core, k, &n, &n2);
		r_cons_printf ("0x%"PFMT64x" 0x%"PFMT64x"\n", n, n2);
		break;
	case 'd':
		if (input[1]==' '){
			char *d = r_asm_describe (core->assembler, input+2);
			if (d && *d) {
				r_cons_printf ("%s\n", d);
				free (d);
			} else eprintf ("Unknown opcode\n");
		} else eprintf ("Use: ?d [opcode]    to get the description of the opcode\n");
		break;
	case 'h':
		if (input[1]==' ') {
			r_cons_printf ("0x%08x\n", (ut32)r_str_hash (input+2));
		} else eprintf ("Usage: ?h [string-to-hash]\n");
		break;
	case 'y':
		for (input++; input[0]==' '; input++);
		if (*input) {
			free (core->yank_buf);
			core->yank_buf = (ut8*)strdup (input);
			core->yank_len = strlen ((const char*)core->yank_buf);
		} else {
			r_cons_memcat ((const char *)core->yank_buf, core->yank_len);
			r_cons_newline ();
		}
		break;
	case 'F':
		r_cons_flush ();
		break;
	case 'f':
		if (input[1]==' ') {
			char *q, *p = strdup (input+2);
			if (!p) {
				eprintf ("Cannot strdup\n");
				return 0;
			}
			q = strchr (p, ' ');
			if (q) {
				*q = 0;
				n = r_num_get (core->num, p);
				r_str_bits (out, (const ut8*)&n, sizeof (n), q+1);
				r_cons_printf ("%s\n", out);
			} else eprintf ("Usage: \"?b value bitstring\"\n");
			free (p);
		} else eprintf ("Whitespace expected after '?f'\n");
		break;
	case 'o':
		n = r_num_math (core->num, input+1);
		r_cons_printf ("0%"PFMT64o"\n", n);
		break;
	case 'u':
		{
			char unit[32];
			n = r_num_math (core->num, input+1);
			r_num_units (unit, n);
			r_cons_printf ("%s\n", unit);
		}
		break;
	case ' ':
		{
			char unit[32];
			ut32 n32, s, a;
			float f;
			n = r_num_math (core->num, input+1);
			n32 = (ut32)n;
			memcpy (&f, &n32, sizeof (f));
			/* decimal, hexa, octal */
			s = n>>16<<12;
			a = n & 0x0fff;
			r_num_units (unit, n);
			r_cons_printf ("%"PFMT64d" 0x%"PFMT64x" 0%"PFMT64o" %s %04x:%04x ",
				n, n, n, unit, s, a);
			if (n>>32) r_cons_printf ("%"PFMT64d" ", (st64)n);
			else r_cons_printf ("%d ", (st32)n);
			/* binary and floating point */
			r_str_bits (out, (const ut8*)&n, sizeof (n), NULL);
			r_cons_printf ("%s %.01lf %f\n", out, core->num->fvalue, f);
		}
		break;
	case 'v':
		n = (input[1] != '\0') ? r_num_math (core->num, input+2) : 0;
		switch (input[1]) {
		case 'i':
			if (n>>32) r_cons_printf ("%"PFMT64d"\n", (st64)n);
			else r_cons_printf ("%d\n", (st32)n);
			break;
		case 'd':
			r_cons_printf ("%"PFMT64d"\n", n);
			break;
		default:
			r_cons_printf ("0x%"PFMT64x"\n", n);
		}
		core->num->value = n; // redundant
		break;
	case '=': // set num->value
		if (input[1]) {
			r_num_math (core->num, input+1);
		} else r_cons_printf ("0x%"PFMT64x"\n", core->num->value);
		break;
	case '+':
		if (input[1]) {
			st64 n = (st64)core->num->value;
			if (n>0) r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%"PFMT64x"\n", core->num->value);
		break;
	case '-':
		if (input[1]) {
			st64 n = (st64)core->num->value;
			if (n<0) r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%"PFMT64x"\n", core->num->value);
		break;
	case '!': // ??
		if (input[1]) {
			if (!core->num->value)
				r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%"PFMT64x"\n", core->num->value);
		break;
	case '@':
		r_cons_printf (
		"Usage: ... @[(f|s|b)]:] [@iter] expr[!size] [~grep]\n"
		"Pipes:\n"
		"  x > foo       pipe output of 'x' command to 'foo' file\n"
		"  x >> foo      concatenate output of 'x' in 'foo' file\n"
		"  x | less      pipe output of 'x' command to less program\n"
		"Command evaluation:\n"
		"  `pdi~push:0[0]`  replace output of command inside the line\n"
		"  .!rabin2 -ri $FILE  load imports by running rabin2\n"
		"  .:8080        expect commands at tcp port 8080\n"
		"Special suffixes to temporary set the contents of block:\n"
		"  @f:/bin/ls    from file contents\n"
		"  @s:hello      from given string\n"
		"  @b:909192     from hex pairs string\n"
		"Temporary seeks:\n"
		"  @ 0x1024      seek to 0x1024 offset\n"
		"  @ sym.main+3  seek to the evaluated math expressions\n"
		"  @ 32!128      blocksize = 128, offset = 32\n"
		"Repeaters:\n"
		"  @@=1 2 3      repeat previous command at offsets 1, 2 and 3\n"
		"  @@ hit*       repeat command for all flags matching 'hit*' glob\n"
		// TODO: documentate @@ glob1 glob2 glob3
		"Internal grep:\n"
		"  ~?            count lines\n"
		"  ~mov          grep lines matching 'mov'\n"
		"  ~!mov         grep lines not matching 'mov'\n"
		"  ~mov[0]       get first column of lines matching 'mov'\n"
		"  ~mov:3[0]     get 1st column from the 4th line matching 'mov'\n"
		);
		return 0;
	case '$':
		r_cons_printf (
		"RNum $variables usable in math expressions:\n"
		" $$     here (current virtual seek)\n"
		" $o     here (current disk io offset)\n"
		" $s     file size\n"
		" $b     block size\n"
		" $w     get word size, 4 if asm.bits=32, 8 if 64, ...\n"
		" $c,$r  get width and height of terminal\n"
		" $S     section offset\n"
		" $SS    section size\n"
		" $j     jump address (e.g. jmp 0x10, jz 0x10 => 0x10)\n"
		" $f     jump fail address (e.g. jz 0x10 => next instruction)\n"
		" $I     number of instructions of current function\n"
		" $F     current function size \n"
		" $Jn    get nth jump of function\n"
		" $Cn    get nth call of function\n"
		" $Dn    get nth data reference in function\n"
		" $Xn    get nth xref of function\n"
		" $m     opcode memory reference (e.g. mov eax,[0x10] => 0x10)\n"
		" $l     opcode length\n"
		" $e     1 if end of block, else 0\n"
		" ${ev}  get value of eval config variable # TODO: use ?k too\n"
		" $?     last comparision value\n"
		);
		return R_TRUE;
	case 'V':
		if (!strcmp (R2_VERSION, GIT_TAP))
			r_cons_printf ("%s\n", R2_VERSION);
		else r_cons_printf ("%s aka %s\n", R2_VERSION, GIT_TAP);
		break;
	case 'l':
		for (input++; input[0]==' '; input++);
		core->num->value = strlen (input);
		break;
	case 'X':
		for (input++; input[0]==' '; input++);
		n = r_num_math (core->num, input);
		r_cons_printf ("%"PFMT64x"\n", n);
		break;
	case 'x':
		for (input++; input[0]==' '; input++);
		if (!memcmp (input, "0x", 2) || (*input>='0' && *input<='9')) {
			ut64 n = r_num_math (core->num, input);
			int bits = r_num_to_bits (NULL, n) / 8;
			for (i=0; i<bits; i++)
				r_cons_printf ("%02x", (ut8)((n>>(i*8)) &0xff));
			r_cons_newline ();
		} else {
			for (i=0; input[i]; i++)
				r_cons_printf ("%02x", input[i]);
			r_cons_newline ();
		}
		break;
	case 'e': // echo
		for (input++; *input==' '; input++);
		r_cons_printf ("%s\n", input);
		break;
	case 's': // sequence from to step
		{
		ut64 from, to, step;
		char *p, *p2;
		for (input++; *input==' '; input++);
		p = strchr (input, ' ');
		if (p) {
			*p='\0';
			from = r_num_math (core->num, input);
			p2 = strchr (p+1, ' ');
			if (p2) {
				*p2='\0';
				step = r_num_math (core->num, p2+1);
			} else step = 1;
			to = r_num_math (core->num, p+1);
			for (;from<=to; from+=step)
				r_cons_printf ("%"PFMT64d" ", from);
			r_cons_newline ();
		}
		}
		break;
	case 'P':
		if (core->io->va) {
			ut64 o, n = (input[0] && input[1])?
				r_num_math (core->num, input+2): core->offset;
			o = r_io_section_offset_to_vaddr (core->io, n);
			r_cons_printf ("0x%08"PFMT64x"\n", o);
		} else eprintf ("io.va is false\n");
		break;
	case 'p':
		if (core->io->va) {
			// physical address
			ut64 o, n = (input[0] && input[1])?
				r_num_math (core->num, input+2): core->offset;
			o = r_io_section_vaddr_to_offset (core->io, n);
			r_cons_printf ("0x%08"PFMT64x"\n", o);
		} else eprintf ("Virtual addresses not enabled!\n");
		break;
	case 'S': {
		// section name
		RIOSection *s;
		ut64 n = (input[0] && input[1])?
			r_num_math (core->num, input+2): core->offset;
		n = r_io_section_vaddr_to_offset (core->io, n);
		s = r_io_section_get (core->io, n);
		if (s && s->name)
			r_cons_printf ("%s\n", s->name);
		} break;
	case '_': // hud input
		free (core->yank_buf);
		for (input++; *input==' '; input++);
		core->yank_buf = (ut8*)r_cons_hud_file (input);
		core->yank_len = core->yank_buf? strlen (
			(const char *)core->yank_buf): 0;
		break;
	case 'k': // key=value utility
		switch (input[1]) {
		case ' ':
			p = strchr (input+1, '='); 
			if (p) {
				// set
				*p = 0;
				r_pair_set (core->kv, input+2, p+1);
			} else {
				// get
				p = r_pair_get (core->kv, input+2);
				if (p) {
					r_cons_printf ("%s\n", p);
					free (p);
				}
			}
			break;
		case 's':
			r_pair_save (core->kv, input+3);
			break;
		case 'l':
			r_pair_load (core->kv, input+3);
			break;
		case '\0':
			{ RListIter *iter;
			RPairItem *kv;
			RList *list = r_pair_list (core->kv, NULL);
			r_list_foreach (list, iter, kv) {
				r_cons_printf ("%s=%s\n", kv->k, kv->v);
			}
			}
			break;
		case '?':
			eprintf ("Usage: ?k [key[=value]]\n"
				" ?k foo=bar   # set value\n"
				" ?k foo       # show value\n"
				" ?k           # list keys\n"
				" ?kl ha.sdb   # load keyvalue from ha.sdb\n"
				" ?ks ha.sdb   # save keyvalue to ha.sdb\n");
			break;
		}
		break;
	case 'i': // input num
		if (!r_config_get_i (core->config, "scr.interactive")) {
			eprintf ("Not running in interactive mode\n");
		} else
		switch (input[1]) {
		case 'f':
			core->num->value = !r_num_conditional (core->num, input+2);
			eprintf ("%s\n", r_str_bool (!core->num->value));
			break;
		case 'm':
			r_cons_message (input+2);
			break;
		case 'p': {
			char *p = r_cons_hud_path (input+2, 0);
			core->yank_buf = (ut8*)p;
			core->yank_len = p? strlen (p): 0;
			core->num->value = (p != NULL);
			} break;
		case 'k':
			r_cons_any_key ();
			break;
		case 'y':
			for (input+=2; *input==' '; input++);
			core->num->value =
			r_cons_yesno (1, "%s? (Y/n)", input);
			break;
		case 'n':
			for (input+=2; *input==' '; input++);
			core->num->value =
			r_cons_yesno (0, "%s? (y/N)", input);
			break;
		default: {
			char foo[1024];
			r_cons_flush ();
			for (input++; *input==' '; input++);
			// TODO: use prompt input
			eprintf ("%s: ", input);
			fgets (foo, sizeof (foo)-1, stdin);
			foo[strlen (foo)-1] = 0;
			free (core->yank_buf);
			core->yank_buf = (ut8 *)strdup (foo);
			core->yank_len = strlen (foo);
			core->num->value = r_num_math (core->num, foo);
			}
			break;
		}
		break;
	case 't': {
		struct r_prof_t prof;
		r_prof_start (&prof);
		r_core_cmd (core, input+1, 0);
		r_prof_end (&prof);
		core->num->value = (ut64)(int)prof.result;
		eprintf ("%lf\n", prof.result);
		} break;
	case '?': // ???
		if (input[1]=='?') {
			if (input[2]=='?') {
				r_cons_printf ("What are you doing?\n");
				return 0;
			}
			if (input[2]) {
				if (core->num->value)
					r_core_cmd (core, input+1, 0);
				break;	
			}
			r_cons_printf (
			"Usage: ?[?[?]] expression\n"
			" ? eip-0x804800    show hex and dec result for this math expr\n"
			" ?= eip-0x804800   same as above without user feedback\n"
			" ??                show value of operation\n"
			" ?? [cmd]          ? == 0 run command when math matches\n"
			" ?_ hudfile        load hud menu with given file\n"
			" ?b [num]          show binary value of number\n"
			" ?B [elem]         show range boundaries like 'e?search.in\n"
			" ?d opcode         describe opcode for asm.arch\n"
			" ?e string         echo string\n"
			" ?f [num] [str]    map each bit of the number as flag string index\n"
			" ?h [str]          calculate hash for given string\n"
			" ?iy prompt        yesno input prompt\n"
			" ?i[ynmkp] arg     prompt for number or Yes,No,Msg,Key,Path and store in $$?\n"
			" ?in prompt        yesno input prompt\n"
			" ?im message       show message centered in screen\n"
			" ?ik               press any key input dialog\n"
			" ?k k[=v]          key-value temporal storage for the user\n"
			" ?l str            returns the length of string (0 if null)\n"
			" ?o num            get octal value\n"
			" ?p vaddr          get physical address for given virtual address\n"
			" ?P paddr          get virtual address for given physical one\n"
			" ?r [from] [to]    generate random number between from-to\n"
			" ?s from to step   sequence of numbers from to by steps\n"
			" ?S addr           return section name of given address\n"
			" ?t cmd            returns the time to run a command\n"
			" ?u num            get value in human units (KB, MB, GB, TB)\n"
			" ?v eip-0x804800   show hex value of math expr\n"
			" ?vi rsp-rbp       show decimal value of math expr\n"
			" ?V                show library version of r_core\n"
			" ?x num|0xnum|str  returns the hexpair of number or string\n"
			" ?X num|expr       returns the hexadecimal value numeric expr\n"
			" ?y [str]          show contents of yank buffer, or set with string\n"
			" ?! [cmd]          ? != 0\n"
			" ?+ [cmd]          ? > 0\n"
			" ?- [cmd]          ? < 0\n"
			" ???               show this help\n");
			return 0;
		} else
		if (input[1]) {
			if (core->num->value == UT64_MIN)
				r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("%"PFMT64d"\n", core->num->value);
		break;
	case '\0':
	default:
		r_cons_printf (
		"Usage: [.][times][cmd][~grep][@[@iter]addr!size][|>pipe] ; ...\n"
		"Append '?' to any char command to get detailed help\n"
		"Prefix with number to repeat command N times (f.ex: 3x)\n"
		" $alias=value          alias commands (simple macros)\n"
		" (macro arg0 arg1)     manage scripting macros\n"
		" .[ -|file|!sh|cmd]    interpret cparse, r2 or rlang file (see -?)\n"
		" = [cmd]               run this command via rap://\n"
		" /[xmp/]               search for bytes, regexps, patterns, ..\n"
		" ![cmd]                run given command as in system(3)\n"
		" #[algo] [len]         calculate hash checksum of current block\n"
		" a                     perform analysis of code\n"
		" b [bsz]               get or change block size\n"
		" c[dqxXfg] [arg]       compare block with given data\n"
		" C[Cf..]               code metadata management\n"
		" d[hrscb]              debugger commands\n"
		" e [a[=b]]             list/get/set config evaluable vars\n"
		" f [name][sz][at]      set flag at current address\n"
		" g[wcilper] [arg]      go compile shellcodes with r_egg\n"
		" i[acdeiIosSz] [file]  get info about opened file\n"
		" l[-num][ num| msg]    log utility\n"
		" m[lyogfdps]           mountpoints commands\n"
		" o [file] (addr)       open file at optional address\n"
		" p[?] [len]            print current block with format and length\n"
		" P[osi?]               project management utilities\n"
		" q [ret]               quit program with a return value\n"
		" r[+- ][len]           resize file\n"
		" s [addr]              seek to address (also for '0x', '0x1' == 's 0x1')\n"
		" S?[size] [vaddr]      io section manipulation information\n"
		" V[vcmds]              enter visual mode (vcmds=visualvisual  keystrokes)\n"
		" w[mode] [arg]         multiple write operations\n"
		" x [len]               alias for 'px' (print hexadecimal)\n"
		" y [len] [off]         yank/paste bytes from/to memory\n"
		" ?[??] [expr]          help or evaluate math expression\n"
		" ?$?                   show available '$' variables\n"
		" ?@?                   show help for '@' and '~' suffix\n"
		);
		break;
	}
	return 0;
}
