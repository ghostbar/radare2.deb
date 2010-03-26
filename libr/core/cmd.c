/* radare - LGPL - Copyright 2009-2010 */
/*   nibble<.ds@gmail.com> */
/*   pancake<nopcode.org> */

#include "r_core.h"
#include "r_flags.h"
#include "r_hash.h"
#include "r_asm.h"
#include "r_anal.h"
#include "r_util.h"
#include "r_bp.h"

#include <sys/types.h>
#include <stdarg.h>
#if __UNIX__
// TODO: move this outside
#include <sys/wait.h>
#endif

static int cmd_io_system(void *data, const char *input);

static int cmd_iopipe(void *data, const char *input) {
	RCore *core = (RCore *)data;
	switch (input[0]) {
	case '\0':
		r_lib_list (&core->lib);
		r_io_handle_list (&core->io);
		break;
	default:
		cmd_io_system (data, input);
		break;
	}
	return R_TRUE;
}

static void cmd_reg (struct r_core_t *core, const char *str) {
	struct r_reg_item_t *r;
	const char *name;
	char *arg;
	int size, i, type = R_REG_TYPE_GPR;
	switch (str[0]) {
	case '?':
		if (str[1]) {
			r_debug_reg_sync (&core->dbg, R_REG_TYPE_GPR, R_FALSE);
			r = r_reg_get (core->dbg.reg, str+1, R_REG_TYPE_GPR);
			if (r == NULL) eprintf ("Unknown register (%s)\n", str+1);
			else r_cons_printf ("0x%08llx\n", r_reg_get_value (core->dbg.reg, r));
		} else
		eprintf ("Usage: dr[*] [type] [size] - get/set registers\n"
			" dr?        display this help message\n"
			" dr         show registers in rows\n"
			" dr=        show registers in columns\n"
			" dr?eax     show value of eax register\n"
			" .dr*       include common register values in flags\n"
			" .dr-       unflag all registers\n"
			" drp [file] load register metadata file\n"
			" drp        display current register profile\n"
			" dr         show 'gpr' registers\n"
			" drt        show all register types\n"
			" drn [pc]   get register name for pc,sp,bp,a0-3\n"
			" dr all     show all registers\n"
			" dr flg 1   show flag registers ('flg' is type, see drt)\n"
			" dr 16      show 16 bit registers\n"
			" dr 32      show 32 bit registers\n"
			" dr eax=33  set register value. eax = 33\n");
		break;
	case 'p':
		if (str[1]) {
			eprintf ("profile: \n");
			if (core->dbg.reg_profile)
				r_cons_printf ("%s\n", core->dbg.reg_profile);
			else eprintf ("No register profile defined. Try 'dr.'\n");
		} else r_reg_set_profile (core->dbg.reg, str+2);
		break;
	case 't':
		for (i=0; (name=r_reg_get_type (i));i++)
			r_cons_printf ("%s\n", name);
		break;
	case 'n':
		name = r_reg_get_name (core->dbg.reg, r_reg_get_name_idx (str+2));
		if (name && *name)
			r_cons_printf ("%s\n", name);
		else eprintf ("Oops. try dn [pc|sp|bp|a0|a1|a2|a3]\n");
		break;
	case '=':
		r_debug_reg_sync (&core->dbg, R_REG_TYPE_GPR, R_FALSE);
		r_debug_reg_list (&core->dbg, R_REG_TYPE_GPR, 32, 2); // XXX detect which one is current usage
		r_debug_reg_list (&core->dbg, R_REG_TYPE_GPR, 64, 2);
		break;
	case '*':
		r_debug_reg_sync (&core->dbg, R_REG_TYPE_GPR, R_FALSE);
		r_debug_reg_list (&core->dbg, R_REG_TYPE_GPR, 32, 1); // XXX detect which one is current usage
		r_debug_reg_list (&core->dbg, R_REG_TYPE_GPR, 64, 1);
		break;
	case '\0':
		r_debug_reg_sync (&core->dbg, R_REG_TYPE_GPR, R_FALSE);
		r_debug_reg_list (&core->dbg, R_REG_TYPE_GPR, 32, 0);
		r_debug_reg_list (&core->dbg, R_REG_TYPE_GPR, 64, 0);
		break;
	case ' ':
		arg = strchr (str+1, '=');
		if (arg) {
			*arg = 0;
			r = r_reg_get (core->dbg.reg, str+1, R_REG_TYPE_GPR);
			if (r) {
				//eprintf ("SET(%s)(%s)\n", str, arg+1);
				r_cons_printf ("0x%08llx ->", str,
					r_reg_get_value (core->dbg.reg, r));
				r_reg_set_value (core->dbg.reg, r,
					r_num_math (&core->num, arg+1));
				r_debug_reg_sync (&core->dbg, R_REG_TYPE_GPR, R_TRUE);
				r_cons_printf ("0x%08llx\n",
					r_reg_get_value (core->dbg.reg, r));
			} else eprintf ("Unknown register '%s'\n", str+1);
			return;
		}
		size = atoi (str+1);
		if (size==0) {
			arg = strchr (str+1, ' ');
			if (arg && size==0) {
				*arg='\0';
				size = atoi (arg);
			} else size = 32;
			eprintf ("ARG(%s)\n", str+1);
			type = r_reg_type_by_name (str+1);
		}
		//printf("type = %d\nsize = %d\n", type, size);
		if (type != R_REG_TYPE_LAST) {
			r_debug_reg_sync (&core->dbg, type, R_FALSE);
			r_debug_reg_list (&core->dbg, type, size, str[0]=='*');
		} else eprintf ("cmd_reg: Unknown type\n");
	}
}

static void r_core_cmd_bp (struct r_core_t *core, const char *input) {
	if (input[1]==' ')
		input++;
	switch (input[1]) {
	case 't':
		{
		int i = 0;
		RList *list = r_debug_frames (&core->dbg);
		RListIter *iter = r_list_iterator (list);
		while (r_list_iter_next (iter)) {
			RDebugFrame *frame = r_list_iter_get (iter);
			r_cons_printf ("%d  0x%08llx  %d\n", i++, frame->addr, frame->size);
		}
		r_list_destroy (list);
		}
		break;
	case '\0':
		r_bp_list (core->dbg.bp, input[1]=='*');
		break;
	case '-':
		r_bp_del (core->dbg.bp, r_num_math (&core->num, input+2));
		break;
	case 'e':
		r_bp_enable (core->dbg.bp, r_num_math(&core->num, input+2), 1);
		break;
	case 'd':
		r_bp_enable (core->dbg.bp, r_num_math(&core->num, input+2), 0);
		break;
	case 'h':
		if (input[2]==' ') {
			if (!r_bp_use (core->dbg.bp, input+3))
				eprintf ("Invalid name: '%s'.\n", input+3);
		} else r_bp_handle_list (core->dbg.bp);
		break;
	case '?':
		r_cons_printf (
		"Usage: db [[-]addr] [len] [rwx] [condstring]\n"
		"db              ; list breakpoints\n"
		"db sym.main     ; add breakpoint into sym.main\n"
		"db 0x804800     ; add breakpoint\n"
		"db -0x804800    ; remove breakpoint\n"
		"dbe 0x8048000   ; enable breakpoint\n"
		"dbd 0x8048000   ; disable breakpoint\n"
		"dbh x86         ; set/list breakpoint plugin handlers\n"
		"Unrelated:\n"
		"dbt             ; debug backtrace\n");
		break;
	default:
		r_bp_add_sw (core->dbg.bp, r_num_math (&core->num, input+1),
			1, R_BP_PROT_EXEC);
		break;
	}
}

/* TODO: this should be moved to the core->yank api */
static int cmd_yank_to(struct r_core_t *core, char *arg) {
	ut64 src = core->offset;
	ut64 len = 0;
	ut64 pos = -1;
	char *str;
	ut8 *buf;

	while (*arg==' ')
		arg = arg+1;
	str = strchr (arg, ' ');
	if (str) {
		str[0]='\0';
		len = r_num_math (&core->num, arg);
		pos = r_num_math (&core->num, str+1);
		str[0]=' ';
	}
	if ((str == NULL) || (pos == -1) || (len == 0)) {
		eprintf("Usage: yt [len] [dst-addr]\n");
		return 1;
	}
#if 0
	if (!config_get("file.write")) {
		eprintf("You are not in read-write mode.\n");
		return 1;
	}
#endif
	buf = (ut8*)malloc (len);
	r_core_read_at (core, src, buf, len);
	r_core_write_at (core, pos, buf, len);
	free(buf);

	core->offset = src;
	r_core_block_read (core, 0);
	return 0;
}

static int cmd_yank(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	switch (input[0]) {
	case ' ':
		r_core_yank(core, core->offset, atoi(input+1));
		break;
	case 'y':
		r_core_yank_paste(core, r_num_math(&core->num, input+2), 0);
		break;
	case 't':
		{ /* hacky implementation */
			char *arg = strdup(input+1);
			cmd_yank_to(core, arg);
			free(arg);
		}
		break;
	case '\0':
		if (core->yank) {
			int i;
			r_cons_printf ("0x%08llx %d ",
				core->yank_off, core->yank_len);
			for(i=0;i<core->yank_len;i++)
				r_cons_printf ("%02x", core->yank[i]);
			r_cons_newline ();
		} else eprintf ("No buffer yanked already\n");
		break;
	default:
		r_cons_printf (
		"Usage: y[y] [len] [[@]addr]\n"
		" y            ; show yank buffer information (srcoff len bytes)\n"
		" y 16         ; copy 16 bytes into clipboard\n"
		" y 16 0x200   ; copy 16 bytes into clipboard from 0x200\n"
		" y 16 @ 0x200 ; copy 16 bytes into clipboard from 0x200\n"
		" yy 0x3344    ; paste clipboard\n");
		break;
	}
	return R_TRUE;
}

static int cmd_quit(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	switch (input[0]) {
	case '?':
		r_cons_printf (
		"Usage: q[!] [retvalue]\n"
		" q     ; quit program\n"
		" q!    ; force quit (no questions)\n"
		" q 1   ; quit with return value 1\n"
		" q a-b ; quit with return value a-b\n");
		break;
	case ' ':
	case '!':
		input++;
	case '\0':
		// TODO
	default:
		r_line_hist_save (".radare2_history");
		if (*input)
			r_num_math (&core->num, input);
		else core->num.value = 0LL;
		//exit (*input?r_num_math (&core->num, input+1):0);
		return -2;
	}
	return R_FALSE;
}

static int cmd_interpret(void *data, const char *input) {
	char *str, *ptr, *eol;
	RCore *core = (RCore *)data;
	switch (input[0]) {
	case '\0':
		/* repeat last command */
		/* XXX: already handled in r_core_cmd with ugly strcmp */
		break;
	case ' ':
		/* interpret file */
		r_core_cmd_file (core, input+1);
		break;
	case '!':
		/* from command */
		r_core_cmd_command (core, input+1);
		break;
	case '(':
		//eprintf ("macro call (%s)\n", input+1);
		r_cmd_macro_call (&core->cmd.macro, input+1);
		break;
	case '?':
		r_cons_printf (
		"Usage: . [file] | [!command] | [(macro)]\n"
		" . foo.rs          ; interpret r script\n"
		" .!rabin -ri $FILE ; interpret output of command\n"
		" .(foo 1 2 3)      ; run macro 'foo' with args 1, 2, 3\n"
		" ./m ELF           ; interpret output of command /m ELF as r. commands\n");
		break;
	default:
		ptr = str = r_core_cmd_str (core, input);
		for (;;) {
			eol = strchr (ptr, '\n');
			if (eol) eol[0]='\0';
			r_core_cmd (core, ptr, 0);
			if (!eol) break;
			ptr = eol+1;
		}
		free (str);
		break;
	}
	return 0;
}

static int cmd_section(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	switch (input[0]) {
	case '?':
		r_cons_printf (
		" S                ; list sections\n"
		" S*               ; list sections (in radare commands\n"
		" S=               ; list sections (in nice ascii-art bars)\n"
		" S [offset] [vaddr] [size] [vsize] [name] ; adds new section\n"
		" S -[offset]      ; remove this section definition\n");
		break;
	case ' ':
		switch (input[1]) {
		case '-': // remove
			if (input[2]=='?' || input[2]=='\0')
				eprintf ("Usage: S -#   ; where # is the section index\n");
			else r_io_section_rm (&core->io, atoi (input+1));
			break;
		default:
			{
			int i;
			char *ptr = strdup(input+1);
			const char *name = NULL;
			ut64 vaddr = 0LL;
			ut64 offset = 0LL;
			ut64 size = 0LL;
			ut64 vsize = 0LL;
			
			i = r_str_word_set0 (ptr);
			switch(i) {
			case 5: // get name
				name = r_str_word_get0 (ptr, 4);
			case 4: // get vsize
				vsize = r_num_math (&core->num, r_str_word_get0 (ptr, 3));
			case 3: // get size
				size = r_num_math (&core->num, r_str_word_get0 (ptr, 2));
			case 2: // get vaddr
				vaddr = r_num_math (&core->num, r_str_word_get0 (ptr, 1));
			case 1: // get offset
				offset = r_num_math (&core->num, r_str_word_get0 (ptr, 0));
			}
			r_io_section_add (&core->io, offset, vaddr, size, vsize, 7, name);
			free (ptr);
			}
			break;
		}
		break;
	case '=':
		r_io_section_list_visual (&core->io, core->offset, core->blocksize);
		break;
	case '\0':
		r_io_section_list (&core->io, core->offset, 0);
		break;
	case '*':
		r_io_section_list (&core->io, core->offset, 1);
		break;
	}
	return 0;
}

static int cmd_seek(void *data, const char *input) {
	RCore *core = (RCore *)data;
	char *cmd, *p; 
	ut64 off;

	if (input[0]=='r') {
		if (input[1] && input[2]) {
			off = r_debug_reg_get (&core->dbg, input+2);
			r_core_seek (core, off, 1);
		} else eprintf ("Usage: 'sr pc' ; seek to register\n");
	} else
	if (input[0] && input[1]) {
		st32 delta = (input[1]==' ')?2:1;
		off = r_num_math (&core->num, input + delta); 
		if (input[0]==' ' && (input[1]=='+'||input[1]=='-'))
			input = input+1;
		switch (input[0]) {
		case ' ':
			r_core_seek (core, off, 1);
			break;
		case '+':
			if (input[1]=='+') delta = core->blocksize; else delta = off;
			r_core_seek_delta (core, delta);
			break;
		case '-':
			if (input[1]=='-') delta = -core->blocksize; else delta = -off;
			r_core_seek_delta (core, delta);
			break;
		case 'a':
			off = core->blocksize;
			if (input[1]&&input[2]) {
				cmd = strdup (input);
				p = strchr (cmd+2, ' ');
				if (p) {
					off = r_num_math (&core->num, p+1);;
					*p = '\0';
				}
				cmd[0] = 's';
				// perform real seek if provided
				r_cmd_call (&core->cmd, cmd);
				free (cmd);
			}
			r_core_seek_align (core, off, 0);
			break;
		case 'b':
			r_core_anal_bb_seek (core, off);
			break;
		case '?':
			r_cons_printf (
			"Usage: s[+-] [addr]\n"
			" s 0x320    ; seek to this address\n"
			" s++        ; seek blocksize bytes forward\n"
			" s--        ; seek blocksize bytes backward\n"
			" s+ 512     ; seek 512 bytes forward\n"
			" s- 512     ; seek 512 bytes backward\n"
			" sa [[+-]a] [asz] ; seek asz (or bsize) aligned to addr\n"
			" sb         ; seek aligned to bb start\n");
			break;
		}
	} else r_cons_printf ("0x%llx\n", core->offset);
	return 0;
}

static int cmd_help(void *data, const char *input) {
	RCore *core = (RCore *)data;
	ut64 n;
	switch (input[0]) {
	case ' ':
		n = r_num_math (&core->num, input+1);
		r_cons_printf ("%lld 0x%llx\n", n,n);
		break;
	case '=':
		r_num_math (&core->num, input+1);
		break;
	case '+':
		if (input[1]) {
			if (core->num.value & UT64_GT0)
				r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%llx\n", core->num.value);
		break;
	case '-':
		if (input[1]) {
			if (core->num.value & UT64_LT0)
				r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%llx\n", core->num.value);
		break;
	case '!': // ??
		if (input[1]) {
			if (&core->num.value != UT64_MIN)
				r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%llx\n", core->num.value);
		break;
	case '$':
		return cmd_help (data, " $?");
	case 'z':
		for (input=input+1;input[0]==' ';input=input+1);
		core->num.value = strlen(input);
		break;
	case 't': {
		struct r_prof_t prof;
		r_prof_start (&prof);
		r_core_cmd (core, input+1, 0);
		r_prof_end (&prof);
		core->num.value = (ut64)prof.result;
		eprintf ("%lf\n", prof.result);
		} break;
	case '?': // ???
		if (input[1]=='?') {
			r_cons_printf (
			"Usage: ?[?[?]] expression\n"
			" ? eip-0x804800  ; calculate result for this math expr\n"
			" ?= eip-0x804800 ; same as above without user feedback\n"
			" ?? [cmd]        ; ? == 0  run command when math matches\n"
			" ?z str          ; returns the length of string (0 if null)\n"
			" ?t cmd          ; returns the time to run a command\n"
			" ?! [cmd]        ; ? != 0\n"
			" ?+ [cmd]        ; ? > 0\n"
			" ?- [cmd]        ; ? < 0\n"
			" ???             ; show this help\n"
			"$variables:\n"
			" $$  = here (current seek)\n"
			" $s  = file size\n"
			" $b  = block size\n"
			" $j  = jump address\n"
			" $f  = address of next opcode\n"
			" $r  = opcode reference pointer\n"
			" $e  = 1 if end of block, else 0\n"
			" ${eval} = get value of eval variable\n"
			" $?  = last comparision value\n");
			return 0;
		} else
		if (input[1]) {
			if (&core->num.value == UT64_MIN)
				r_core_cmd (core, input+1, 0);
		} else r_cons_printf ("0x%llx\n", core->num.value);
		break;
	case '\0':
	default:
		r_cons_printf (
		"Usage:\n"
		" a                 ; perform analysis of code\n"
		" b [bsz]           ; get or change block size\n"
		" C[CFf..]          ; Code metadata management\n"
		" d[hrscb]          ; debugger commands\n"
		" e [a[=b]]         ; list/get/set config evaluable vars\n"
		" f [name][sz][at]  ; set flag at current address\n"
		" s [addr]          ; seek to address\n"
		" S?[size] [vaddr]  ; IO section manipulation information\n"
		" i [file]          ; get info about opened file\n"
		" o [file] (addr)   ; open file at optional address\n"
		" p?[len]           ; print current block with format and length\n"
		" V[vcmds]          ; enter visual mode (vcmds=visualvisual  keystrokes)\n"
		" w[mode] [arg]     ; multiple write operations\n"
		" x [len]           ; alias for 'px' (print hexadecimal\n"
		" y [len] [off]     ; yank/paste bytes from/to memory\n"
		" ? [expr]          ; help or evaluate math expression\n"
		" /[xmp/]           ; search for bytes, regexps, patterns, ..\n"
		" |[cmd]            ; run this command thru the io pipe (no args=list)\n"
		" #[algo] [len]     ; calculate hash checksum of current block\n"
		" .[ file|!cmd|cmd|(macro)]  ; interpret as radare cmds\n"
		" (macro arg0 arg1) ; define scripting macros\n"
		" q [ret]           ; quit program with a return value\n"
		"Use '?$' to get help for the variables\n"
		"Use '?""?""?' for extra help about '?' subcommands.\n"
		"Append '?' to any char command to get detailed help\n");
		break;
	}
	return 0;
}

static int cmd_bsize(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	switch (input[0]) {
	case '\0':
		r_cons_printf ("0x%x\n", core->blocksize);
		break;
	default:
		//input = r_str_clean(input);
		r_core_block_size (core, r_num_math (&core->num, input));
		break;
	}
	return 0;
}

static int cmd_cmp(void *data, const char *input) {
#if 0
	int ret;
	FILE *fd;
	unsigned int off;
	unsigned char *buf;
	RCore *core = data;
#endif

	switch (*input) {
#if 0
	case 'c':
		radare_compare_code (
			r_num_math (&core->num, input+1),
			core->block, core->blocksize);
		break;
	case 'd':
		off = (unsigned int) get_offset(input+1);
		radare_compare((u8*)&off, config.block, 4);
		break;
	case 'f':
		if (input[1]!=' ') {
			eprintf("Please. use 'cf [file]'\n");
			return 0;
		}
		fd = fopen(input+2, "r");
		if (fd == NULL) {
			eprintf("Cannot open file '%s'\n",input+2);
			return 0;
		}
		buf = (unsigned char *)malloc(config.block_size);
		fread(buf, 1, config.block_size, fd);
		fclose(fd);
		radare_compare(buf, config.block, config.block_size);
		free(buf);
		break;
	case 'x':
		if (input[1]!=' ') {
			eprintf("Please. use 'wx 00 11 22'\n");
			return 0;
		}
		buf = (unsigned char *)malloc(strlen(input+2));
		ret = hexstr2binstr(input+2, buf);
		radare_compare(buf, config.block, ret);
		free(buf);
		break;
	case 'X':
		{
		u8 *buf = malloc(config.block_size);
		radare_read_at(get_math(input+1), buf, config.block_size);
		radare_compare_hex(config.seek+config.vaddr, buf, config.block, config.block_size);
		free(buf);
		}
		break;
	case ' ':
		radare_compare((unsigned char*)input+1,config.block, strlen(input+1)+1);
		break;
	case 'D':
		{
		char cmd[1024];
		sprintf(cmd, "radiff -b %s %s", ".curblock", input+2);
		file_dump(".curblock", config.block, config.block_size);
		radare_system(cmd);
		unlink(".curblock");
		}
		break;
#endif
	case '?':
		r_cons_strcat (
		"Usage: c[?cdfx] [argument]\n"
		" c  [string]   Compares a plain with escaped chars string\n"
		" cc [offset]   Code bindiff current block against offset\n"
		" cd [value]    Compare a doubleword from a math expression\n"
		" cx [hexpair]  Compare hexpair string\n"
		" cX [addr]     Like 'cc' but using hexdiff output\n"
		" cf [file]     Compare contents of file at current seek\n"
		" cD [file]     Like above, but using radiff -b\n");
		break;
	default:
		eprintf ("Usage: c[?Ddxf] [argument]\n");
	}

	return 0;
}

static int cmd_info(void *data, const char *input) {
	RCore *core = (RCore *)data;
	char buf[1024];
	switch (*input) {
	case 's':
	case 'i':
	case 'I':
	case 'e':
	case 'S':
	case 'z':
		snprintf (buf, sizeof (buf), "rabin2 -%c%s%s '%s'", input[0],
			input[1]=='*'?"r":"", core->io.va?"v":"", core->file->filename);
		eprintf ("(%s)\n", buf);
		r_sys_cmd (buf);
		break;
	case '?':
		r_cons_printf (
		"Usage: i[eiIsSz]*      ; get info from opened file (rabin2)\n"
		"; Append a '*' to get the output in radare commands\n"
		" ii    ; imports\n"
		" iI    ; binary info\n"
		" ie    ; entrypoint\n"
		" is    ; symbols\n"
		" iS    ; sections\n"
		" iz    ; strings\n");
		break;
	case '*':
		break;
	default:
		r_cons_printf ("uri: %s\n", core->file->uri);
		r_cons_printf ("filesize: 0x%x\n", core->file->size);
		r_cons_printf ("blocksize: 0x%x\n", core->blocksize);
	}
	return 0;
}

static int cmd_print(void *data, const char *input) {
	RCore *core = (RCore *)data;
	int l, len = core->blocksize;
	ut32 tbs = core->blocksize;
	int adistrick = r_config_get_i (&core->config, "asm.middle"); // TODO: find better name
	int show_offset = r_config_get_i (&core->config, "asm.offset");
	int show_bytes = r_config_get_i (&core->config, "asm.bytes");
	int show_color = r_config_get_i (&core->config, "scr.color");
	int show_lines = r_config_get_i (&core->config, "asm.lines");
	int show_dwarf = r_config_get_i (&core->config, "asm.dwarf");
	int nb, nbytes = r_config_get_i (&core->config, "asm.nbytes");
	int linesout = r_config_get_i (&core->config, "asm.linesout");
	int show_comments = r_config_get_i (&core->config, "asm.comments");
	int pseudo = r_config_get_i (&core->config, "asm.pseudo");
	int filter = r_config_get_i (&core->config, "asm.filter");
	int linesopts = 0;
	nb = nbytes*2;

	if (r_config_get_i (&core->config, "asm.linesstyle"))
		linesopts |= R_ANAL_REFLINE_STYLE;
	if (r_config_get_i (&core->config, "asm.lineswide"))
		linesopts |= R_ANAL_REFLINE_WIDE;

	/* XXX: This is only for pd/pD ??? */
	if (input[0] && input[1]) {
		l = (int) r_num_math (&core->num, input+2);
		// exceptions are: disasm and memoryfmt */
		if (input[0] != 'd' && input[0] != 'm') {
			if (l>0) len = l;
			if (l>tbs) r_core_block_size (core, l);
			l = 9999;
		}
	} else l = 9999;
	
	switch (input[0]) {
	case 'D':
	case 'd':
		// TODO: move to a function...we need a flag instead of thousand config_foo's
		{
			int ret, idx, i; 
			int middle = 0;
			ut8 *buf = core->block;
			char str[128];
			char line[128];
			char *comment;
			char *opstr;
			char *osl = NULL; // old source line
			RAsmAop asmop;
			RAnalAop analop;
			RAnalRefline *reflines;
			RFlagItem *flag;
		
			//r_anal_set_pc(&core->anal, core->offset);
			r_asm_set_pc (&core->assembler, core->offset);

			reflines = r_anal_reflines_get (&core->anal, core->offset,
				buf, len, -1, linesout);
			for (i=idx=ret=0; idx < len && i<l; idx+=ret,i++) {
				ut64 addr = core->offset + idx;
				r_asm_set_pc (&core->assembler, addr);
				//r_anal_set_pc (&core->anal, core->anal.pc + ret);
				if (show_comments) {
					comment = r_meta_get_string (&core->meta, R_META_COMMENT, addr);
					if (comment) {
						r_cons_strcat (comment);
						free (comment);
					}
				}
				r_anal_reflines_str (&core->anal, reflines, addr, line, linesopts);
				ret = r_asm_disassemble (&core->assembler, &asmop, buf+idx, len-idx);
				if (ret<1) {
					ret = 1;
					eprintf ("** invalid opcode at 0x%08llx **\n", core->assembler.pc + ret);
					continue;
				}
				r_anal_aop (&core->anal, &analop, addr, buf+idx, (int)(len-idx));
			
				if (adistrick)
					middle = r_anal_reflines_middle (&core->anal,
							reflines, addr, analop.length);

				if (show_lines)
					r_cons_strcat (line);
				if (show_offset)
					r_cons_printf ("0x%08llx  ", core->offset + idx);
				flag = r_flag_get_i (&core->flags, core->offset+idx);
				if (flag || show_bytes) {
					char *extra = " ";
					char *str;
					if (flag) str = strdup (flag->name);
					else {
						str = strdup (asmop.buf_hex);
						if (strlen (str) > nb) {
							str[nb] = '.';
							str[nb+1] = '\0';
							extra = "";
						}
					}
					if (flag) {
						if (show_color)
							r_cons_printf (Color_BWHITE"*[ %*s]  "Color_RESET, (nb)-4, str);
						else r_cons_printf ("*[ %*s]  ", (nb)-4, str);
					} else r_cons_printf ("%*s %s", nb, str, extra);
					free (str);
				} else r_cons_printf ("%*s  ", (nb), "");
				if (show_color) {
					switch (analop.type) {
					case R_ANAL_OP_TYPE_NOP:
						r_cons_printf (Color_BLUE);
						break;
					case R_ANAL_OP_TYPE_JMP:
					case R_ANAL_OP_TYPE_UJMP:
					case R_ANAL_OP_TYPE_CJMP:
						r_cons_printf (Color_GREEN);
						break;
					case R_ANAL_OP_TYPE_CALL:
						r_cons_printf (Color_BGREEN);
						break;
					case R_ANAL_OP_TYPE_SWI:
						r_cons_printf (Color_MAGENTA);
						break;
					case R_ANAL_OP_TYPE_RET:
						r_cons_printf (Color_RED);
						break;
					case R_ANAL_OP_TYPE_LOAD:
						r_cons_printf (Color_YELLOW);
						break;
					case R_ANAL_OP_TYPE_STORE:
						r_cons_printf (Color_BYELLOW);
						break;
					}
				}
				if (pseudo) {
					r_parse_parse (&core->parser, asmop.buf_asm, str);
					opstr = str;
				} else if (filter) {
					r_parse_filter (&core->parser, &core->flags, asmop.buf_asm, str);
					opstr = str;
				} else opstr = asmop.buf_asm;
				r_cons_strcat (opstr);
				if (show_color)
					r_cons_strcat (Color_RESET);
				if (show_dwarf) {
					char *sl = r_bin_meta_get_source_line (&core->bin, addr);
					int len = strlen (opstr);
					if (len<30)
						len = 30-len;
					if (sl)
					if (!osl || (osl && strcmp (sl, osl))) {
						while (len--)
							r_cons_strcat (" ");
						if (show_color)
							r_cons_printf (Color_TURQOISE"  ; %s"Color_RESET, sl);
						else r_cons_printf ("  ; %s\n", sl);
						free (osl);
						osl = sl;
					}
				}
				if (middle != 0) {
					ret = ret-middle;
					r_cons_printf (" ;  *middle* %d", ret);
				}
				r_cons_newline ();
				if (show_lines && analop.type == R_ANAL_OP_TYPE_RET) {
					if (strchr (line, '>'))
						memset (line, ' ', strlen (line));
					r_cons_strcat (line);
					r_cons_strcat ("; ------------------------------------\n");
				}
			}
			free (reflines);
			free (osl);
		}
		break;
	case 's':
		r_print_string (&core->print, core->offset, core->block, len, 0, 1, 0); //, 78, 1);
		break;
	case 'S':
		r_print_string (&core->print, core->offset, core->block, len, 1, 1, 0); //, 78, 1);
		break;
	case 'u':
		r_print_string (&core->print, core->offset, core->block, len, 0, 1, 1); //, 78, 1);
		break;
	case 'U':
		r_print_string (&core->print, core->offset, core->block, len, 1, 1, 1); //, 78, 1);
		break;
	case 'c':
		r_print_code (&core->print, core->offset, core->block, len); //, 78, 1);
		break;
	case 'r':
		r_print_raw (&core->print, core->block, len);
		break;
	case 'o':
        	r_print_hexdump (&core->print, core->offset, core->block, len, 8, 1); //, 78, !(input[1]=='-'));
		break;
	case 'x':
        	r_print_hexdump (&core->print, core->offset, core->block, len, 16, 1); //, 78, !(input[1]=='-'));
		break;
	case '8':
		r_print_bytes (&core->print, core->block, len, "%02x");
		break;
	case 'm':
		r_print_format (&core->print, core->offset, core->block, len, input+1);
		break;
	case 'n': // easter penis
		for (l=0; l<10; l++) {
			printf ("\r8");
			for (len=0;len<l;len++)
				printf ("=");
			printf ("D");
			r_sys_usleep (100000);
			fflush (stdout);
		}
		for (l=0; l<3; l++) {
			printf ("~");
			fflush (stdout);
			r_sys_usleep (100000);
		}
		printf ("\n");
		break;
	default:
		r_cons_printf (
		"Usage: p[fmt] [len]\n"
		" p8 [len]    8bit hexpair list of bytes\n"
		" px [len]    hexdump of N bytes\n"
		" po [len]    octal dump of N bytes\n"
		" pc [len]    output C format\n"
		" ps [len]    print string\n"
		" pm [fmt]    print formatted memory\n" // TODO: rename to pf??
		" pS [len]    print wide string\n"
		" pd [len]    disassemble N opcodes\n"
		" pD [len]    disassemble N bytes\n"
		" pr [len]    print N raw bytes\n"
		" pu [len]    print N url encoded bytes\n"
		" pU [len]    print N wide url encoded bytes\n");
		break;
	}
	if (tbs != core->blocksize)
		r_core_block_size (core, tbs);
	return 0;
}

static int cmd_hexdump(void *data, const char *input) {
	return cmd_print (data, input-1);
}

static int cmd_flag(void *data, const char *input) {
	RCore *core = (RCore *)data;
	int len = strlen (input)+1;
	char *str = alloca (len);
	memcpy (str, input+1, len);

	switch (input[0]) {
	case '+':
		r_flag_set (&core->flags, str, core->offset, core->blocksize, 1);
		break;
	case ' ': {
		char *s = NULL, *s2 = NULL;
		ut64 seek = core->offset;
		ut32 bsze = core->blocksize;
		s = strchr (str, ' ');
		if (s) {
			*s = '\0';
			s2 = strchr(s+1, ' ');
			if (s2) {
				*s2 = '\0';
				seek = r_num_math (&core->num, s2+1);
			}
			bsze = r_num_math (&core->num, s+1);
		}
		r_flag_set (&core->flags, str, seek, bsze, 0);
		if (s) *s=' ';
		if (s2) *s2=' ';
		}
		break;
	case '-':
		r_flag_unset (&core->flags, input+1);
		break;
	case 's':
		if (input[1]==' ') r_flag_space_set (&core->flags, input+2);
		else r_flag_space_list (&core->flags);
		break;
	case 'o':
		{
			char *file = PREFIX"/share/doc/radare2/fortunes";
			char *line = r_file_slurp_random_line (file);
			r_cons_printf (" -- %s\n", line);
			free (line);
		}
		break;
	case '*':
		r_flag_list (&core->flags, 1);
		break;
	case '\0':
		r_flag_list (&core->flags, 0);
		break;
	case '?':
		r_cons_printf (
		"Usage: f[?] [flagname]\n"
		" f name 12 @ 33   ; set flag 'name' with size 12 at 33\n"
		" f name 12 33     ; same as above\n"
		" f+name 12 @ 33   ; like above but creates new one if doesnt exist\n"
		" f-name           ; remove flag 'name'\n"
		" f                ; list flags\n"
		" f*               ; list flags in r commands\n");
		break;
	}
	return 0;
}

static void cmd_syscall_do(RCore *core, int num) {
	int i;
	RSyscallItem *item = r_syscall_get (&core->syscall, num, -1);
	r_cons_printf ("%d = %s (", item->num, item->name);
	// TODO: move this to r_syscall
	for (i=0; i<item->args; i++) {
		ut64 arg = r_debug_arg_get (&core->dbg, R_TRUE, i+1);
		if (item->sargs==NULL)
			r_cons_printf ("0x%08llx", arg);
		else
		switch (item->sargs[i]) {
		case 'p': // pointer
			r_cons_printf ("0x%08llx", arg);
			break;
		case 'i':
			r_cons_printf ("%lld", arg);
			break;
		case 'z':
			{ char str[64];
			r_io_read_at (&core->io, arg, (ut8*)str, sizeof (str));
			// TODO: filter zero terminated string
			str[63] = '\0';
			r_str_filter (str, strlen (str));
			r_cons_printf ("\"%s\"", str);
			}
			break;
		default:
			r_cons_printf ("0x%08llx", arg);
			break;
		}
		if (i+1<item->args)
			r_cons_printf (", ");
	}
	r_cons_printf (")\n");
}

static int cmd_anal(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	int l, len = core->blocksize;
	ut32 tbs = core->blocksize;

	if (input[0] && input[1]) {
		l = (int) r_num_get (&core->num, input+2);
		if (l>0) len = l;
		if (l>tbs) {
			r_core_block_size (core, l);
			len = l;
		}
	}
	
	switch (input[0]) {
	case 'h':
		if (input[1]) {
			if (!r_anal_use (&core->anal, input+2))
				eprintf ("Cannot use '%s' anal plugin.\n", input+2);
		} else r_anal_list (&core->anal);
		break;
	case 'o':
		r_cons_printf ("TODO\n");
#if 0
		{
			/* XXX hardcoded */
			int ret, idx; 
			ut8 *buf = core->block;
			struct r_anal_aop_t aop;
			r_anal_use (&core->anal, "anal_x86_bea");
			
			for(idx=ret=0; idx < len; idx+=ret)
				ret = r_anal_aop(&core->anal, &aop,
					core->offset+idx, buf + idx, (len-idx));
		}
#endif
		break;
	case 'b':
		switch (input[1]) {
		case '-':
			r_core_anal_bb_clean (core, r_num_math (&core->num, input+2));
			break;
		case '+':
			{
			char *ptr = strdup(input+3);
			ut64 addr = -1LL;
			ut64 size = 0LL;
			ut64 jump = -1LL;
			ut64 fail = -1LL;
			
			switch(r_str_word_set0 (ptr)) {
			case 4: // get fail
				fail = r_num_math (&core->num, r_str_word_get0 (ptr, 3));
			case 3: // get jump
				jump = r_num_math (&core->num, r_str_word_get0 (ptr, 2));
			case 2: // get size
				size = r_num_math (&core->num, r_str_word_get0 (ptr, 1));
			case 1: // get addr
				addr = r_num_math (&core->num, r_str_word_get0 (ptr, 0));
			}
			if (!r_core_anal_bb_add (core, addr, size, jump, fail))
				eprintf ("Cannot add bb (duplicated or overlaped)\n");
			free (ptr);
			}
			break;
		case 'l':
			r_core_anal_bb_list (core, 0);
			break;
		case '*':
			r_core_anal_bb_list (core, 1);
			break;
		case '?':
			r_cons_printf (
			"Usage: ab[?+-l*]\n"
			" ab @ [addr]     ; Analyze basic blocks (start at addr)\n"
			" ab+ addr size [jump] [fail] ; Add basic block\n"
			" ab- [addr]      ; Clean all basic block data (or bb at addr and childs)\n"
			" abl             ; List basic blocks\n"
			" ab*             ; Output radare commands\n");
			break;
		default:
			r_core_anal_bb (core, core->offset,
					r_config_get_i (&core->config, "anal.depth"));
		}
		break;
	case 'f':
		switch (input[1]) {
		case '-':
			r_core_anal_fcn_clean (core, r_num_math (&core->num, input+2));
			break;
		case '+':
			{
			char *ptr = strdup(input+3);
			const char *name = NULL;
			ut64 addr = -1LL;
			ut64 size = 0LL;
			
			if (r_str_word_set0 (ptr) == 3) {
				name = r_str_word_get0 (ptr, 2);
				size = r_num_math (&core->num, r_str_word_get0 (ptr, 1));
				addr = r_num_math (&core->num, r_str_word_get0 (ptr, 0));
				if (!r_core_anal_fcn_add (core, addr, size, name))
					eprintf ("Cannot add function (duplicated or overlaped)\n");
			}
			free (ptr);
			}
			break;
		case 'l':
			r_core_anal_fcn_list (core, 0);
			break;
		case '*':
			r_core_anal_fcn_list (core, 1);
			break;
		case '?':
			r_cons_printf (
			"Usage: af[?+-l*]\n"
			" af @ [addr]     ; Analyze functions (start at addr)\n"
			" af+ addr size name ; Add function\n"
			" af- [addr]      ; Clean all function analysis data (or function at addr)\n"
			" afl             ; List functions\n"
			" af*             ; Output radare commands\n");
			break;
		default:
			r_core_anal_fcn (core, core->offset, -1,
					r_config_get_i (&core->config, "anal.depth"));
		}
		break;
	case 'g':
		switch (input[1]) {
		case 'c':
			r_core_anal_refs (core, r_num_math (&core->num, input+2), 1);
			break;
		case 'l':
			r_core_anal_graph (core, r_num_math (&core->num, input+2), R_CORE_ANAL_GRAPHLINES);
			break;
		case 'f':
			{
			char *fname = r_str_word_get_first (input+(input[2]=='l'?3:2));
			r_core_anal_graph_fcn (core, fname, input[2]=='l');
			free (fname);
			}
			break;
		case 'a':
			r_core_anal_graph (core, r_num_math (&core->num, input+2), 0);
			break;
		case '?':
			r_cons_printf (
			"Usage: ag[?f]\n"
			" ag [addr]       ; Output graphviz code (bb at addr and childs)\n"
			" aga [addr]      ; Idem, but only addresses\n"
			" agc [addr]      ; Output graphviz call graph of function\n"
			" agl [fcn name]  ; Output graphviz code using meta-data\n"
			" agf [fcn name]  ; Output graphviz code of function\n"
			" agfl [fcn name] ; Output graphviz code of function using meta-data\n");
			break;
		default:
			r_core_anal_graph (core, r_num_math (&core->num, input+2), R_CORE_ANAL_GRAPHBODY);
		}
		break;
	case 's':
		switch (input[1]) {
		case 'l':
			r_syscall_list (&core->syscall);
			break;
		case '\0': {
			int a0 = (int)r_debug_reg_get (&core->dbg, "oeax"); //XXX
			cmd_syscall_do (core, a0);
			} break;
		case ' ':
			cmd_syscall_do (core, (int)r_num_get (&core->num, input+2));
			break;
		default:
		case '?':
			r_cons_printf (
			"Usage: as[?]\n"
			" as       Display syscall and arguments\n"
			" as 4     Show syscall 4 based on asm.os\n"
			" asl      List of syscalls by asm.os and asm.arch\n");
			break;
		}
		break;
	default:
		r_cons_printf (
		"Usage: a[?hobfg]\n"
		" ah [handle]     ; Use this analysis plugin\n"
		" as [num]        ; Analyze syscall using dbg.reg\n"
		" ao [len]        ; Analyze raw bytes\n"
		" ab[?+-l*]       ; Analyze basic blocks\n"
		" af[?+-l*]       ; Analyze functions\n"
		" ag[?f]          ; Output graphviz code\n");
		break;
	}
	if (tbs != core->blocksize)
		r_core_block_size (core, tbs);
	return 0;
}

/* TODO: simplify using r_write */
static int cmd_write(void *data, const char *input) {
	int i, len = strlen(input);
	char *tmp, *str = alloca(len)+1;
	struct r_core_t *core = (struct r_core_t *)data;
	memcpy(str, input+1, len);
	switch(input[0]) {
	case ' ':
		/* write string */
		len = r_str_escape (str);
		r_io_set_fd (&core->io, core->file->fd);
		r_io_write_at (&core->io, core->offset, (const ut8*)str, len);
		r_core_block_read (core, 0);
		break;
	case 't': {
			/* TODO: Support user defined size? */
			int len = core->blocksize;
			const char *arg = (const char *)(input+(input[1]==' ')?2:1);
			const ut8 *buf = core->block;
			r_file_dump (arg, buf, len);
		} break;
	case 'T':
		eprintf ("TODO\n");
		break;
	case 'f': {
			int size;
			const char *arg = (const char *)(input+(input[1]==' ')?2:1);
			ut8 *buf = (ut8*) r_file_slurp (arg, &size);
			if (buf) {
				r_io_set_fd (&core->io, core->file->fd);
				r_io_write_at (&core->io, core->offset, buf, size);
				free(buf);
			} else eprintf ("Cannot open file '%s'\n", arg);
		} break;
	case 'F': {
			int size;
			const char *arg = (const char *)(input+(input[1]==' ')?2:1);
			ut8 *buf = r_file_slurp_hexpairs (arg, &size);
			if (buf == NULL) {
				r_io_set_fd (&core->io, core->file->fd);
				r_io_write_at (&core->io, core->offset, buf, size);
				free (buf);
			} else eprintf ("Cannot open file '%s'\n", arg);
		} break;
	case 'w':
		str = str+1;
		len = len-1;
		len *= 2;
		tmp = alloca(len);
		for (i=0;i<len;i++) {
			if (i%2) tmp[i] = 0;
			else tmp[i] = str[i>>1];
		}
		str = tmp;

		// write strifng
		r_io_set_fd (&core->io, core->file->fd);
		r_io_write_at (&core->io, core->offset, (const ut8*)str, len);
		r_core_block_read (core, 0);
		break;
	case 'x':
		{
		int len = strlen (input);
		ut8 *buf = alloca (len);
		len = r_hex_str2bin (input+1, buf);
		r_core_write_at (core, core->offset, buf, len);
		r_core_block_read (core, 0);
		}
		// write hexpairs
		break;
	case 'a':
		{
		struct r_asm_code_t *acode;
		/* XXX ULTRAUGLY , needs fallback support in rasm */
		r_asm_use (&core->assembler, "x86.olly");
		r_asm_set_pc (&core->assembler, core->offset);
		if (input[1]==' ') input=input+1;
		acode = r_asm_massemble (&core->assembler, input+1);
		eprintf ("Written %d bytes (%s)=wx %s\n", acode->len, input+1, acode->buf_hex);
		r_core_write_at (core, core->offset, acode->buf, acode->len);
		r_asm_code_free (acode);
		r_core_block_read (core, 0);
		r_asm_use (&core->assembler, "x86"); /* XXX */
		}
		break;
	case 'b':
		{
		int len = strlen (input);
		ut8 *buf = alloca (len);
		len = r_hex_str2bin (input+1, buf);
		r_mem_copyloop (core->block, buf, core->blocksize, len);
		r_core_write_at (core, core->offset, core->block, core->blocksize);
		r_core_block_read (core, 0);
		}
		break;
	case 'm':
		{
		int len = r_hex_str2bin (input+1, (ut8*)str);
		switch (input[1]) {
		case '\0':
			eprintf ("Current write mask: TODO\n");
			// TODO
			break;
		case '?':
			break;
		case '-':
			r_io_set_write_mask(&core->io, 0, 0);
			eprintf ("Write mask disabled\n");
			break;
		case ' ':
			if (len == 0) {
				eprintf ("Invalid string\n");
			} else {
				r_io_set_fd(&core->io, core->file->fd);
				r_io_set_write_mask(&core->io, (const ut8*)str, len);
				eprintf ("Write mask set to '");
				for (i=0;i<len;i++)
					eprintf ("%02x", str[i]);
				eprintf ("'\n");
			}
			break;
		}
		}
		break;
	case 'v':
		{
		ut64 off = r_num_math(&core->num, input+1);
		r_io_set_fd (&core->io, core->file->fd);
		r_io_seek (&core->io, core->offset, R_IO_SEEK_SET);
		if (off&UT64_32U) {
			/* 8 byte addr */
			ut64 addr8;
			memcpy((ut8*)&addr8, (ut8*)&off, 8); // XXX needs endian here
		//	endian_memcpy((ut8*)&addr8, (ut8*)&off, 8);
			r_io_write(&core->io, (const ut8 *)&addr8, 8);
		} else {
			/* 4 byte addr */
			ut32 addr4, addr4_ = (ut32)off;
			//drop_endian((ut8*)&addr4_, (ut8*)&addr4, 4); /* addr4_ = addr4 */
			//endian_memcpy((ut8*)&addr4, (ut8*)&addr4_, 4); /* addr4 = addr4_ */
			memcpy ((ut8*)&addr4, (ut8*)&addr4_, 4); // XXX needs endian here too
			r_io_write (&core->io, (const ut8 *)&addr4, 4);
		}
		r_core_block_read (core, 0);
		}
		break;
	case 'o':
                switch (input[1]) {
                case 'a':
                case 's':
                case 'A':
                case 'x':
                case 'r':
                case 'l':
                case 'm':
                case 'd':
                case 'o':
                        if (input[2]!=' ') {
                                eprintf ("Usage: 'wo%c 00 11 22'\n", input[1]);
                                return 0;
                        }
                case '2':
                case '4':
                        r_core_write_op(core, input+3, input[1]);
                        break;
                case '\0':
                case '?':
                default:
                        r_cons_printf (
                        "Usage: wo[xrlasmd] [hexpairs]\n"
                        "Example: wox 90    ; xor cur block with 90\n"
                        "Example: woa 02 03 ; add 2, 3 to all bytes of cur block\n"
                        "Supported operations:\n"
                        "  woa  addition            +=\n"
                        "  wos  substraction        -=\n"
                        "  wom  multiply            *=\n"
                        "  wod  divide              /=\n"
                        "  wox  xor                 ^=\n"
                        "  woo  or                  |=\n"
                        "  woA  and                 &=\n"
                        "  wor  shift right         >>=\n"
                        "  wol  shift left          <<=\n"
                        "  wo2  2 byte endian swap  2=\n"
                        "  wo4  4 byte endian swap  4=\n"
                        );
                        break;
                }
                break;
	default:
	case '?':
		if (core->oobi) {
			eprintf ("Writing oobi buffer!\n");
			r_io_set_fd (&core->io, core->file->fd);
			r_io_write (&core->io, core->oobi, core->oobi_len);
		} else
			r_cons_printf(
			"Usage: w[x] [str] [<file] [<<EOF] [@addr]\n"
			" w foobar    ; write string 'foobar'\n"
			" ww foobar   ; write wide string 'f\\x00o\\x00o\\x00b\\x00a\\x00r\\x00'\n"
			" wa push ebp ; write opcode, separated by ';' (use '\"' around the command)\n"
			" wb 010203   ; fill current block with cyclic hexpairs\n"
			" wx 9090     ; write two intel nops\n"
			" wv eip+34   ; write 32-64 bit value\n"
			" wo[] hex    ; write in block with operation. 'wo?' fmi\n"
			" wm f0ff     ; cyclic write mask\n"
			" wf file     ; write contents of file at current offset\n"
			" wF file     ; write contents of hexpairs file here\n"
			" wt file     ; write current block to file\n");
			//TODO: add support for offset+seek
			// " wf file o s ; write contents of file from optional offset 'o' and size 's'.\n"
		break;
	}
	return 0;
}

static const char *cmdhit = NULL;
static int __cb_hit(struct r_search_kw_t *kw, void *user, ut64 addr) {
	struct r_core_t *core = (struct r_core_t *)user;

	r_cons_printf ("f hit%d_%d %d 0x%08llx\n",
		kw->kwidx, kw->count, kw->keyword_length, addr);

	if (!strnull (cmdhit)) {
		ut64 here = core->offset;
		r_core_seek (core, addr, R_FALSE);
		r_core_cmd (core, cmdhit, 0);
		r_core_seek (core, here, R_TRUE);
	}

	return R_TRUE;
}

static int cmd_search(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	ut64 at;
	ut32 n32;
	int ret, dosearch = 0;
	ut8 *buf;
	switch (input[0]) {
	case '/':
		r_search_begin (core->search);
		dosearch = 1;
		break;
	case 'v':
		r_search_free (core->search);
		core->search = r_search_new (R_SEARCH_KEYWORD);
		n32 = r_num_math (&core->num, input+1);
		r_search_kw_add_bin (core->search, (const ut8*)&n32, 4, NULL, 0);
		r_search_begin (core->search);
		dosearch = 1;
		break;
	case ' ': /* search string */
		r_search_free(core->search);
		core->search = r_search_new(R_SEARCH_KEYWORD);
		r_search_kw_add(core->search, input+1, "");
		r_search_begin(core->search);
		dosearch = 1;
		break;
	case 'm': /* match regexp */
		{
		char *inp = strdup(input+2);
		char *res = r_str_lchr(inp+1, inp[0]);
		char *opt = NULL;
		if (res > inp) {
			opt = strdup(res+1);
			res[1]='\0';
		}
		r_search_free(core->search);
		core->search = r_search_new(R_SEARCH_REGEXP);
		r_search_kw_add(core->search, inp, opt);
		r_search_begin(core->search);
		dosearch = 1;
		free(inp);
		free(opt);
		}
		break;
	case 'x': /* search hex */
		r_search_free(core->search);
		core->search = r_search_new(R_SEARCH_KEYWORD);
		r_search_kw_add_hex(core->search, input+2, "");
		r_search_begin(core->search);
		dosearch = 1;
		break;
	default:
		r_cons_printf (
		"Usage: /[xm/] [arg]\n"
		" / foo     ; search for string 'foo'\n"
		" /m /E.F/i ; match regular expression\n"
		" /x ff0033 ; search for hex string\n"
		" //        ; repeat last search\n");
		break;
	}
	if (core->search->n_kws==0) {
		eprintf ("No keywords defined\n");
	} else
	if (dosearch) {
		/* set callback */
		/* TODO: handle last block of data */
		/* TODO: handle ^C */
		/* TODO: launch search in background support */
		buf = (ut8 *)malloc(core->blocksize);
		r_search_set_callback(core->search, &__cb_hit, core);
		cmdhit = r_config_get(&core->config, "cmd.hit");
		r_cons_break(NULL, NULL);
		for(at = core->offset; at < core->file->size; at += core->blocksize) {
			if (r_cons_singleton()->breaked)
				break;
			r_io_set_fd(&core->io, core->file->fd);
			ret = r_io_read_at(&core->io, at, buf, core->blocksize);
			if (ret != core->blocksize)
				break;
			if (r_search_update(core->search, &at, buf, ret) == -1) {
				printf("search:update error\n");
				break;
			}
		}
		r_cons_break_end ();
	}
	return R_TRUE;
}

static int cmd_eval(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	switch (input[0]) {
	case '\0':
		r_config_list (&core->config, NULL, 0);
		break;
	case '!':
		input = r_str_chop_ro(input+1);
		if (!r_config_swap (&core->config, input))
			eprintf ("r_config: '%s' is not a boolean variable.\n", input);
		break;
	case '-':
		r_core_config_init (core);
		eprintf ("BUG: 'e-' command locks the eval hashtable. patches are welcome :)\n");
		break;
	case '*':
		r_config_list (&core->config, NULL, 1);
		break;
	case '?':
		r_cons_printf (
		"Usage: e[?] [var[=value]]\n"
		"  e     ; list config vars\n"
		"  e-    ; reset config vars\n"
		"  e*    ; dump config vars in r commands\n"
		"  e!a   ; invert the boolean value of 'a' var\n"
		"  e a   ; get value of var 'a'\n"
		"  e a=b ; set var 'a' the 'b' value\n");
		//r_cmd_help(&core->cmd, "e");
		break;
	default:
		r_config_eval (&core->config, input);
	}
	return 0;
}

static int cmd_hash(void *data, const char *input) {
	char algo[32];
	struct r_core_t *core = (struct r_core_t *)data;
	ut32 len = core->blocksize;
	const char *ptr;

	if (input[0]=='!') {
#if 0
		#!lua < file
		#!lua <<EOF
		#!lua
		#!lua foo bar
#endif
		if (input[1]=='\0') {
			r_lang_list (&core->lang);
			return R_TRUE;
		}
		// TODO: set argv here
		r_lang_use (&core->lang, input+1);
		if (core->oobi)
			r_lang_run (&core->lang,(const char *)
				core->oobi, core->oobi_len);
		else r_lang_prompt (&core->lang);
		return R_TRUE;
	}

	ptr = strchr (input, ' ');
	sscanf (input, "%31s", algo);
	if (ptr != NULL)
		len = r_num_math(&core->num, ptr+1);

	/* TODO: support all hash algorithms and so */
	if (!r_str_ccmp(input, "crc32", ' ')) {
		r_cons_printf("%04x\n", r_hash_crc32(core->block, len));
	} else
	if (!r_str_ccmp(input, "crc16", ' ')) {
		r_cons_printf("%02x\n", r_hash_crc16(0, core->block, len));
	} else {
		r_cons_printf(
		"Usage: #algo <size> @ addr\n"
		" #crc32               ; calculate crc32 of current block\n"
		" #crc32 < /etc/fstab  ; calculate crc32 of this file\n"
		" #md5 128K @ edi      ; calculate md5 of 128K from 'edi'\n"
		"Usage #!interpreter [<args>] [<file] [<<eof]\n"
		" #!                   ; list all available interpreters\n"
		" #!python             ; run python commandline\n"
		" #!python < foo.py    ; run foo.py python script\n"
		" #!python <<EOF       ; get python code until 'EOF' mark\n"
		" #!python arg0 a1 <<q ; set arg0 and arg1 and read until 'q'\n"
		"Comments:\n"
		" # this is a comment  ; note the space after the sharp sign\n");
	}

	return 0;
}

static int cmd_visual(void *data, const char *input) {
	return r_core_visual ((RCore *)data, input);
}

static int cmd_system(void *data, const char *input) {
	return r_sys_cmd (input);
}

static int cmd_open(void *data, const char *input) {
	RCore *core = (RCore*)data;
	RCoreFile *file;
	ut64 addr, size;
	char *ptr;

	switch (*input) {
	case '\0':
		r_core_file_list (core);
		break;
	default:
	case '?':
		eprintf ("Usage: o [file] ([offset])\n"
		" o                   ; list opened files\n"
		" o /bin/ls           ; open /bin/ls file\n"
		" o /bin/ls 0x8048000 ; map file\n"
		" o-1                 ; close file index 1\n");
		break;
	case ' ':
		ptr = strchr (input+1, ' ');
		if (ptr)
			*ptr = '\0';
		file = r_core_file_open (core, input+1, R_IO_READ);
		if (file) {
			if (ptr) {
				addr = r_num_math (&core->num, ptr+1);
				size = r_io_size (&core->io, file->fd);
				r_io_map_add (&core->io, file->fd, R_IO_READ, 0, addr, size);
				eprintf ("Map '%s' in 0x%08llx with size 0x%llx\n",
					input+1, addr, size);
			}
		} else eprintf ("Cannot open file '%s'\n", input+1);
		break;
	case '-':
		file = r_core_file_get_fd (core, atoi (input+1));
		if (file)
			r_core_file_close (core, file);
		else eprintf ("Unable to find filedescriptor %d\n", atoi (input+1));
		break;
	}
	return 0;
}

static int cmd_meta(void *data, const char *input) {
	RCore *core = (RCore*)data;
	int i, ret, line = 0;
	char file[1024];
	switch (input[0]) {
	case '*':
		r_meta_list (&core->meta, R_META_ANY);
		break;
	case 'L': // debug information of current offset
		ret = r_bin_meta_get_line (&core->bin, core->offset, file, 1023, &line);
		if (ret) {
			r_cons_printf ("file %s\nline %d\n", file, line);
			ret = (line<5)? 5-line: 5;
			line -= 2;
			for (i = 0; i<ret; i++) {
				char *row = r_file_slurp_line (file, line+i, 0);
				r_cons_printf ("%c %.3x  %s\n", (i==2)?'>':' ', line+i, row);
			}
		} else eprintf ("Cannot find meta information at 0x%08llx\n", core->offset);
		break;
	case 'C': /* add comment */
		// TODO: do we need to get the size? or the offset?
		// TODO: is this an exception compared to other C? commands?
		if (input[1]==' ') input = input+1;
		if (input[1]=='-')
			r_meta_del (&core->meta, R_META_COMMENT, core->offset, 1, input+2);
		else r_meta_add (&core->meta, R_META_COMMENT, core->offset, 1, input+1);
		break;
	case 'S':
	case 's':
	case 'm': /* struct */
	case 'x': /* code xref */
	case 'X': /* data xref */
	case 'F': /* add function */
		{
		ut64 addr = core->offset;
		char fun_name[128];
		int size = atoi(input);
		int type = R_META_FUNCTION;
		char *t, *p = strchr (input+1, ' ');
		if (p) {
			t = strdup (p+1);
			//eprintf ("T=(%s)\n", t);
			p = strchr (t, ' ');
			if (p) {
				*p='\0';
				strncpy (fun_name, p+1, sizeof (fun_name));
			} else sprintf (fun_name, "sub_%08llx", addr);
			addr = r_num_math (&core->num, t);
			free(t);
		}
		r_meta_add (&core->meta, type, addr, size, fun_name);
		}
		break;
	case '\0':
	case '?':
		eprintf (
		"Usage: C[CDF?] [arg]\n"
		" CL [addr]               ; show 'code line' information (bininfo)\n"
		" CF [size] [name] [addr] [name] ; register function size here (TODO)\n"
		" CC [string]             ; add comment (TODO)\n");
	}
	return R_TRUE;
}

static int cmd_undowrite(void *data, const char *input) {
	//struct r_core_t *core = (struct r_core_t *)data;
	// TODO:
	return 0;
}

static int cmd_io_system(void *data, const char *input) {
	struct r_core_t *core = (struct r_core_t *)data;
	r_io_set_fd(&core->io, core->file->fd);
	return r_io_system(&core->io, input);
}

static int cmd_macro(void *data, const char *input) {
	RCore *core = (RCore*)data;
	switch(input[0]) {
	case ')':
		r_cmd_macro_break (&core->cmd.macro, input+1);
		break;
	case '-':
		r_cmd_macro_rm (&core->cmd.macro, input+1);
		break;
	case '\0':
		r_cmd_macro_list (&core->cmd.macro);
		break;
	case '?':
		eprintf (
		"Usage: (foo\\n..cmds..\\n)\n"
		" Record macros grouping commands\n"
		" (foo args\\n ..)  ; define a macro\n"
		" (-foo)            ; remove a macro\n"
		" .(foo)            ; to call it\n"
		" ()                ; break inside macro\n"
		"Argument support:\n"
		" (foo x y\\n$1 @ $2) ; define fun with args\n"
		" .(foo 128 0x804800) ; call it with args\n"
		"Iterations:\n"
		" .(foo\\n() $@)      ; define iterator returning iter index\n"
		" x @@ .(foo)         ; iterate over them\n"
		);
		break;
	default:
		r_cmd_macro_add (&core->cmd.macro, input);
		break;
	}
	return 0;
}

static int r_core_cmd_pipe(struct r_core_t *core, char *radare_cmd, char *shell_cmd) {
#if __UNIX__
	int fds[2];
	int stdout_fd, status;

	stdout_fd = dup (1);
	pipe (fds);
	radare_cmd = r_str_trim_head (radare_cmd);
	shell_cmd = r_str_trim_head (shell_cmd);
	if (fork()) {
		dup2(fds[1], 1);
		close (fds[1]);
		close (fds[0]);
		r_core_cmd (core, radare_cmd, 0);
		r_cons_flush ();
		close (1);
		wait (&status);
		dup2 (stdout_fd, 1);
		close (stdout_fd);
	} else {
		close (fds[1]);
		dup2 (fds[0], 0);
		dup2 (2, 1);
		execl ("/bin/sh", "sh", "-c", shell_cmd, NULL);
	}
	return status;
#else
#warning r_core_cmd_pipe UNIMPLEMENTED FOR THIS PLATFORM
	return -1;
#endif
}

static int r_core_cmd_subst(struct r_core_t *core, char *cmd) {
	char *ptr, *ptr2, *str;
	int i, len = strlen(cmd), pipefd, ret;

	if (!*cmd || cmd[0]=='\0')
		return 0;

	cmd = r_str_trim_head_tail (cmd);

	/* quoted / raw command */
	if (cmd[0] =='"') {
		if (cmd[len-1] != '"') {
			eprintf ("parse: Missing ending '\"'\n");
			return -1;
		}
		cmd[len-1]='\0';
		ret = r_cmd_call (&core->cmd, cmd+1);
		return ret;
	}

	/* multiple commands */
	ptr = strrchr (cmd, ';');
	if (ptr) {
		ptr[0]='\0';
		if (r_core_cmd_subst (core, cmd) == -1) 
			return -1;
		cmd = ptr+1;
		r_cons_flush ();
	}

	/* pipe console to shell process */
	ptr = strchr (cmd, '|');
	if (ptr) {
		ptr[0] = '\0';
		r_core_cmd_pipe (core, cmd, ptr+1);
		return 0;
	}

	/* bool conditions */
	ptr = strchr(cmd, '&');
	while (ptr&&ptr[1]=='&') {
		ptr[0]='\0';
		ret = r_cmd_call (&core->cmd, cmd);
		if (ret == -1){
			eprintf ("command error(%s)\n", cmd);
			return ret;
		}
		for (cmd=ptr+2;cmd&&cmd[0]==' ';cmd=cmd+1);
		ptr = strchr (cmd, '&');
	}

	/* Out Of Band Input */
	free (core->oobi);
	core->oobi = NULL;
	ptr = strchr (cmd, '<');
	if (ptr) {
		ptr[0] = '\0';
		if (ptr[1]=='<') {
			/* this is a bit mess */
			const char *oprompt = r_line_singleton ()->prompt;
			oprompt = ">";
			for (str=ptr+2; str[0]==' '; str=str+1);
			eprintf ("==> Reading from stdin until '%s'\n", str);
			free (core->oobi);
			core->oobi = malloc (1);
			core->oobi[0] = '\0';
			core->oobi_len = 0;
			for (;;) {
				char buf[1024];
				int ret;
				printf("> "); fflush(stdout);
				fgets(buf, 1023, stdin); // XXX use r_line ??
				if (feof (stdin))
					break;
				buf[strlen (buf)-1]='\0';
				ret = strlen (buf);
				core->oobi_len+=ret;
				core->oobi = realloc (core->oobi, core->oobi_len+1);
				if (!strcmp (buf, str))
					break;
				strcat ((char *)core->oobi, buf);
			}
			r_line_singleton ()->prompt = oprompt;
		} else {
			for (str=ptr+1;str[0]== ' ';str=str+1);
			eprintf ("SLURPING FILE '%s'\n", str);
			core->oobi = (ut8*)r_file_slurp (str, &core->oobi_len);
			if (core->oobi == NULL)
				eprintf ("Cannot open file\n");
			else if (ptr == cmd)
				return r_core_cmd_buffer (core, (const char *)core->oobi);
		}
	}

	/* pipe console to file */
	ptr = strchr (cmd, '>');
	if (ptr) {
		ptr[0] = '\0';
		str = r_str_trim_head_tail (ptr+1+(ptr[1]=='>'));
		pipefd = r_cons_pipe_open (str, ptr[1]=='>');
		ret = r_core_cmd_subst (core, cmd);
		r_cons_flush ();
		r_cons_pipe_close (pipefd);
		return ret;
	}

	/* sub commands */
	ptr = strchr (cmd, '`');
	if (ptr) {
		ptr2 = strchr (ptr+1, '`');
		if (!ptr2) {
			eprintf ("parse: Missing '´' in expression.\n");
			return -1;
		} else {
			ptr[0] = '\0';
			ptr2[0] = '\0';
			str = r_core_cmd_str (core, ptr+1);
			for(i=0;str[i];i++)
				if (str[i]=='\n') str[i]=' ';
			cmd = r_str_concat (strdup (cmd), r_str_concat (str, ptr2+1));
			ret = r_core_cmd_subst (core, cmd);
			free (cmd);
			free (str);
			return ret;
		}
	}

	/* grep the content */
	ptr = strchr (cmd, '~');
	if (ptr) {
		ptr[0]='\0';
		r_cons_grep (ptr+1);
	} else r_cons_grep (NULL);

	/* seek commands */
	ptr = strchr (cmd, '@');
	if (ptr) {
		ptr[0]='\0';
		ut64 tmpoff = core->offset;
		if (ptr[1]=='@') {
			// TODO: remove temporally seek (should be done by cmd_foreach)
			ret = r_core_cmd_foreach (core, cmd, ptr+2);
			//ret = -1; /* do not run out-of-foreach cmd */
		} else {
			r_core_seek (core, r_num_math (&core->num, ptr+1), 1);
			ret = r_cmd_call (&core->cmd, r_str_trim_head (cmd));
		}
		r_core_seek (core, tmpoff, 1);
		return ret;
	}

	ret = r_cmd_call (&core->cmd, r_str_trim_head (cmd));
	//r_cons_grep_end ();
	return ret;
}

R_API int r_core_cmd_foreach(RCore *core, const char *cmd, char *each) {
	int i=0,j;
	char ch;
	char *word = NULL;
	char *str, *ostr;
	struct list_head *pos;
	ut64 oseek, addr;

	for (; *each==' '; each++);
	for (; *cmd==' '; cmd++);

	oseek = core->offset;
	ostr = str = strdup(each);
	//radare_controlc();

	switch (each[0]) {
	case '?':
		r_cons_printf (
		"Foreach '@@' iterator command:\n"
		" This command is used to repeat a command over a list of offsets.\n"
		" x @@ sym.           Run 'x' over all flags matching 'sym.'\n"
		" x @@.file           \"\" over the offsets specified in the file (one offset per line)\n"
		" x @@=off1 off2 ..   Manual list of offsets\n"
		" x @@=`pdf~call[0]`  Run 'x' at every call offset of the current function\n");
		break;
	case '=':
		/* foreach list of items */
		each = str+1;
		do {
			while (each[0]==' ')
				each = each+1;
			if (!*each) break;
			str = strchr (each, ' ');
			if (str) {
				str[0]='\0';
				addr = r_num_math (&core->num, each);
				str[0]=' ';
			} else addr = r_num_math (&core->num, each);
			eprintf ("; 0x%08llx:\n", addr);
			each = str+1;
			r_core_seek (core, addr, 1);
			r_core_cmd (core, cmd, 0);
			r_cons_flush ();
		} while (str != NULL);
		break;
	case '.':
		if (each[1]=='(') {
			char cmd2[1024];
			// TODO: use controlc() here
			// XXX whats this 999 ?
			for(core->cmd.macro.counter=0;i<999;core->cmd.macro.counter++) {
				r_cmd_macro_call (&core->cmd.macro, each+2);
				if (core->cmd.macro.brk_value == NULL)
					break;

				addr = core->cmd.macro._brk_value;
				sprintf (cmd2, "%s @ 0x%08llx", cmd, addr);
				eprintf ("0x%08llx (%s)\n", addr, cmd2);
				r_core_seek (core, addr, 1);
				r_core_cmd (core, cmd2, 0);
				i++;
			}
		} else {
			char buf[1024];
			char cmd2[1024];
			FILE *fd = fopen (each+1, "r");
			if (fd) {
				core->cmd.macro.counter=0;
				while (!feof (fd)) {
					buf[0] = '\0';
					if (fgets (buf, 1024, fd) == NULL)
						break;
					addr = r_num_math (&core->num, buf);
					eprintf ("0x%08llx: %s\n", addr, cmd);
					sprintf (cmd2, "%s @ 0x%08llx", cmd, addr);
					r_core_seek (core, addr, 1); // XXX
					r_core_cmd (core, cmd2, 0);
					core->cmd.macro.counter++;
				}
				fclose (fd);
			} else eprintf ("Cannot open file '%s' to read offsets\n", each+1);
		}
		break;
	default:
		core->cmd.macro.counter = 0;
		//while(str[i]) && !core->interrupted) {
		while (str[i]) {
			j = i;
			for (;str[j]&&str[j]==' ';j++); // skip spaces
			for (i=j;str[i]&&str[i]!=' ';i++); // find EOS
			ch = str[i];
			str[i] = '\0';
			word = strdup (str+j);
			if (word == NULL)
				break;
			str[i] = ch;
			if (strchr (word, '*')) {
#if 0
				/* for all flags in current flagspace */
				list_for_each(pos, &flags) {
					flag_t *flag = (flag_t *)list_entry(pos, flag_t, list);
					//if (core->interrupted)
					//	break;
					/* filter per flag spaces */
	//				if ((flag_space_idx != -1) && (flag->space != flag_space_idx))
	//					continue;

					core->offset = flag->offset;
					radare_read(0);
					cons_printf("; @@ 0x%08llx (%s)\n", core->offset, flag->name);
					radare_cmd(cmd,0);
				}
#else
				eprintf ("No flags foreach implemented\n");
#endif
			} else {
				/* for all flags in current flagspace */
				list_for_each (pos, &core->flags.flags) {
					RFlagItem *flag = (RFlagItem *)list_entry(pos, RFlagItem, list);

					if (r_cons_singleton()->breaked)
						break;
					/* filter per flag spaces */
					if ((core->flags.space_idx != -1) && (flag->space != core->flags.space_idx))
						continue;
					if (word[0]=='\0' || strstr(flag->name, word) != NULL) {
						r_core_seek (core, flag->offset, 1);
						r_cons_printf ("; @@ 0x%08llx (%s)\n", core->offset, flag->name);
						r_core_cmd (core, cmd, 0);
					}
				}
	#if 0
				/* ugly copypasta from tmpseek .. */
				if (strstr(word, each)) {
					if (word[i]=='+'||word[i]=='-')
						core->offset = core->offset + r_num_math (get_math(&core->num, word);
					else	core->offset = r_num_math (get_math(&core->num, word);
					radare_read(0);
					cons_printf("; @@ 0x%08llx\n", core->offset);
					radare_cmd(cmd,0);
				}
	#endif
				r_cons_break (NULL, NULL);

				core->cmd.macro.counter++ ;
				free (word);
				word = NULL;
			}
		}
	}
	r_cons_break_end ();
	// XXX: use r_core_seek here
	core->offset = oseek;

	free (word);
	free (ostr);
	return R_TRUE;
}

R_API int r_core_cmd(RCore *core, const char *command, int log) {
	int len, rep, ret = R_FALSE;
	char *cmd, *ocmd;
	if (command != NULL) {
		len = strlen (command)+1;
		ocmd = cmd = malloc (len+8192);
		if (ocmd == NULL)
			return R_FALSE;
		memcpy (cmd, command, len);
		cmd = r_str_trim_head_tail (cmd);

		rep = atoi (cmd);
		if (rep<1) rep = 1;
		if (rep>0) {
			ret = R_TRUE;
			while (*cmd>='0'&&*cmd<='9')
				cmd++;
			while (rep--) {
				ret = r_core_cmd_subst (core, cmd);
				if (ret<0)
					break;
			}
		}

		if (log) r_line_hist_add (command);

		free (core->oobi);
		free (ocmd);
		core->oobi = NULL;
		core->oobi_len = 0;
	}
	return ret;
}

R_API int r_core_cmd_file(struct r_core_t *core, const char *file) {
	char buf[1024];
	FILE *fd = fopen (file, "r");
	if (fd == NULL) {
		eprintf ("r_core_cmd_file: Cannot open '%s'\n", file);
		return -1;
	}
	while (!feof (fd)) {
		if (fgets (buf, 1023, fd) != NULL) {
			buf[strlen (buf)-1]='\0';
			if (r_core_cmd (core, buf, 0) == -1) {
				eprintf ("Error running command '%s'\n", buf);
				break;
			}
		}
	}
	fclose (fd);
	return 0;
}

R_API int r_core_cmd_command(struct r_core_t *core, const char *command) {
	int len;
	char *buf, *rcmd, *ptr;
	rcmd = ptr = buf = r_sys_cmd_str (command, 0, &len);
	if (buf == NULL)
		return -1;
	while ((ptr = strstr (rcmd, "\n"))) {
		*ptr = '\0';
		if (r_core_cmd (core, rcmd, 0) == -1) {
			eprintf ("Error running command '%s'\n", rcmd);
			break;
		}
		rcmd += strlen (rcmd)+1;
	}
	r_str_free(buf);
	return 0;
}

static void cmd_dm(RCore *core, const char *input) {
	switch (input[0]) {
	case '?':
		r_cons_printf (
		"Usage: dm [size]\n"
		" dm         List memory maps of target process\n"
		" dm*        Same as above but in radare commands\n"
		" dm 4096    Allocate 4096 bytes in child process\n"
		" dm-0x8048  Deallocate memory map of address 0x8048\n"
		//" dm rw- esp 9K  set 9KB of the stack as read+write (no exec)\n"
		"TODO: map files in process memory.\n");
		break;
	case '*':
	case '-':
	case ' ':
		eprintf ("TODO\n");
		break;
	default:
		r_debug_map_sync (&core->dbg); // update process memory maps
		r_debug_map_list (&core->dbg, core->offset);
		break;
	}
}

static int step_until(RCore *core, ut64 addr) {
	ut64 off = r_debug_reg_get (&core->dbg, "pc");
	if (off == 0LL) {
		eprintf ("Cannot 'drn pc'\n");
		return R_FALSE;
	}
	if (addr == 0LL) {
		eprintf ("Cannot continue until address 0\n");
		return R_FALSE;
	}
	do {
		r_debug_step (&core->dbg, 1);
		off = r_debug_reg_get (&core->dbg, "pc");
		// check breakpoint here
	} while (off != addr);
	return R_TRUE;
	
}

static int step_line(RCore *core, int times) {
	char file[512], file2[512];
	int find_meta, line, line2;
	ut64 off;

	off = r_debug_reg_get (&core->dbg, "pc");
	if (off == 0LL) {
		eprintf ("Cannot 'drn pc'\n");
		return R_FALSE;
	}
	if (r_bin_meta_get_line (&core->bin, off, file, sizeof (file), &line)) {
		eprintf ("--> 0x%08llx %s : %d\n", off, file, line);
		eprintf ("--> %s\n", r_file_slurp_line (file, line, 0));
		find_meta = R_FALSE;
	} else {
		eprintf ("--> Stepping until dwarf line\n");
		find_meta = R_TRUE;
	}
	do {
		r_debug_step (&core->dbg, 1);
		off = r_debug_reg_get (&core->dbg, "pc");
		if (!r_bin_meta_get_line (&core->bin, off, file2, sizeof (file2), &line2)) {
			if (find_meta)
				continue;
			eprintf ("Cannot retrieve dwarf info at 0x%08llx\n", off);
			return R_FALSE;
		}
	} while (!strcmp (file, file2) && line == line2);
	eprintf ("--> 0x%08llx %s : %d\n", off, file2, line2);
	eprintf ("--> %s\n", r_file_slurp_line (file2, line2, 0));
	return R_TRUE;
}

static void cmd_debug_pid(RCore *core, const char *input) {
	const char *ptr;
	int pid, sig;
	if (input[1] == 'k') {
		/* XXX: not for threads? signal is for a whole process!! */
		/* XXX: but we want fine-grained access to process resources */
		pid = atoi (input+2);
		ptr = strchr (input, ' ');
		if (ptr) sig = atoi (ptr+1);
		else sig = 0;
		if (pid > 0) {
			eprintf ("Sending signal '%d' to pid '%d'\n",
				sig, pid);
			r_debug_kill (&core->dbg, sig);
		} else eprintf ("Invalid arguments\n");
	} else
	if (input[1] == 't') {
		if (input[2]=='=' || input[2]==' ')
			r_debug_select (&core->dbg,
				(int) r_num_math (&core->num, input+3),
				(int) r_num_math (&core->num, input+3));
		else r_debug_thread_list (&core->dbg, core->dbg.pid);
	} else
	if (input[1]=='?')
		r_cons_printf ("Usage: dp[=][pid]\n"
			" dp      list current pid and childrens\n"
			" dp 748  list childs of pid\n"
			" dp*     list all attachable pids\n"
			" dpa 377 attach and select this pid\n"
			" dp=748  select this pid\n"
			" dpt     List threads of current pid\n"
			" dpt 74  List threads of given process\n"
			" dpt=64  Attach to thread\n"
			" dpk P S send signal S to P process id\n");
	else
	if (input[1]=='a') {
		r_debug_attach (&core->dbg,
			(int) r_num_math (&core->num, input+2));
		r_debug_select (&core->dbg,
			(int) r_num_math (&core->num, input+2),
			(int) r_num_math (&core->num, input+2));
	} else
	if (input[1]=='=')
		r_debug_select (&core->dbg,
			(int) r_num_math (&core->num, input+2),
			(int) r_num_math (&core->num, input+2));
	else
	if (input[1]=='*')
		r_debug_pid_list (&core->dbg, 0);
	else
	if (input[1]==' ')
		r_debug_pid_list (&core->dbg,
			(int) r_num_math (&core->num, input+2));
	else r_debug_pid_list (&core->dbg, core->dbg.pid);
}

static int cmd_debug(void *data, const char *input) {
	RCore *core = (RCore *)data;
	struct r_anal_aop_t analop;
	int len, times, sig;
	ut64 addr;
	char *ptr;

	switch (input[0]) {
	case 'x':
		r_debug_execute (&core->dbg, (ut8*)
			"\xc7\xc0\x03\x00\x00\x00\x33\xdb\x33"
			"\xcc\xc7\xc2\x10\x00\x00\x00\xcd\x80", 18);
		break;
	case 't':
		// TODO: Add support to change the tag
		if (input[1]=='r')
			r_debug_trace_reset (&core->dbg, R_TRUE);
		else r_debug_trace_list (&core->dbg, -1);
		break;
	case 'd':
		eprintf ("TODO: dd: file descriptors\n");
		break;
	case 's':
		times = atoi (input+2);
		if (times<1) times = 1;
		if (input[1]=='?')
			r_cons_printf ("Usage: ds[ol] [count]\n"
				" ds       step one instruction\n"
				" ds 4     step 4 instructions\n"
				" dso 3    step over 3 instructions\n"
				" dsu addr step until address\n"
				" dsl      step one source line\n"
				" dsl 40   step 40 source lines\n");
		else if (input[1]=='u') {
			step_until (core, r_num_math (&core->num, input+2)); // XXX dupped by times
		} else if (input[1]=='o')
			r_debug_step_over (&core->dbg, times);
		else if (input[1]=='l')
			step_line (core, times);
		else r_debug_step (&core->dbg, times);
		break;
	case 'b':
		r_core_cmd_bp (core, input);
		break;
	case 'H':
		eprintf ("TODO: transplant process\n");
		break;
	case 'c':
		switch (input[1]) {
		case '?':
			eprintf("Usage: dc[?]  -- continue execution\n"
				" dc?              show this help\n"
				" dc               continue execution of all childs\n"
				" dct [len]        traptrace from curseek to len, no argument to list\n"
				" dcu [addr]       continue until address\n"
				" dco [num]        step over N instructions\n"
				" dcs [num]        continue until syscall\n"
				" dck [sig] [pid]  continue sending kill 9 to process\n"
				" dc [pid]         continue execution of pid\n"
				" dc[-pid]         stop execution of pid\n"
				"TODO: support for threads?\n");
			break;
		case 'k':
			// select pid and r_debug_continue_kill (&core->dbg, 
			ptr = strchr (input+3, ' ');
			if (ptr) {
				int old_pid = core->dbg.pid;
				int pid = atoi (ptr+1);
				*ptr = 0;
				r_debug_select (&core->dbg, pid, pid);
				r_debug_continue_kill (&core->dbg, atoi (input+2));
				r_debug_select (&core->dbg, old_pid, old_pid);
			} else r_debug_continue_kill (&core->dbg, atoi (input+2));
			break;
		case 's':
			sig = r_num_math (&core->num, input+2);
			eprintf ("Continue until syscall %d\n", sig);
			r_debug_continue_syscall (&core->dbg, sig);
			/* TODO : use r_syscall here, to retrieve syscall info */
			break;
		case 'u':
			addr = r_num_math (&core->num, input+2);
			if (addr) {
				eprintf ("Continue until 0x%08llx\n", addr);
				r_bp_add_sw (core->dbg.bp, addr, 1, R_BP_PROT_EXEC);
				r_debug_continue (&core->dbg);
				r_bp_del (core->dbg.bp, addr);
			} else eprintf ("Cannot continue until address 0\n");
			break;
		case ' ':
			do {
				int old_pid = core->dbg.pid;
				int pid = atoi (input+2);
				r_debug_select (&core->dbg, pid, pid);
				r_debug_continue (&core->dbg);
				r_debug_select (&core->dbg, old_pid, old_pid);
			} while (0);
			break;
		case 't':
			len = r_num_math (&core->num, input+2);
			if (len == 0) {
				r_bp_traptrace_list (core->dbg.bp);
			} else {
				eprintf ("Trap tracing 0x%08llx-0x%08llx\n", core->offset, core->offset+len);
				r_bp_traptrace_reset (core->dbg.bp, R_TRUE);
				r_bp_traptrace_add (core->dbg.bp, core->offset, core->offset+len);
				r_bp_traptrace_enable (core->dbg.bp, R_TRUE);
				do {
					ut8 buf[32];
					r_debug_continue (&core->dbg);
					addr = r_debug_reg_get (&core->dbg, "pc");
					if (addr == 0LL) {
						eprintf ("pc=0\n");
						break;
					}
					/* XXX Bottleneck..we need to reuse the bytes read by traptrace */
					// XXX Do asm.arch should define the max size of opcode?
					r_core_read_at (core, addr, buf, 32); // XXX longer opcodes?
					r_anal_aop (&core->anal, &analop, addr, buf, sizeof (buf));
				} while (r_bp_traptrace_at (core->dbg.bp, addr, analop.length));
				r_bp_traptrace_enable (core->dbg.bp, R_FALSE);
			}
			break;
		default:
			eprintf ("continue\n");
			r_debug_continue (&core->dbg);
		}
		break;
	case 'm':
		cmd_dm (core, input+1);
		break;
	case 'r':
		cmd_reg (core, input+1);
		//r_core_cmd (core, "|reg", 0);
		break;
	case 'p':
		cmd_debug_pid (core, input);
		break;
	case 'h':
		if (input[1]==' ')
			r_debug_use (&core->dbg, input+2);
		else r_debug_handle_list (&core->dbg);
		break;
	default:
		r_cons_printf ("Usage: d[sbhcrbo] [arg]\n"
		" dh [handler]   list or set debugger handler\n"
		" dH [handler]   transplant process to a new handler\n"
		" dd             file descriptors\n"
		" ds[ol] N       step, over, source line\n"
		" dp[=*?t][pid]  list, attach to process or thread id\n"
		" dc[?]          continue execution. dc? for more\n"
		" dr[?]          cpu registers, dr? for extended help\n"
		" db[?]          breakpoints\n"
		" dbt            display backtrace\n"
		" dt[r] [tag]    display instruction traces (dtr=reset)\n"
		" dm             show memory maps\n");
		break;
	}
	return 0;
}

R_API int r_core_cmd_buffer(void *user, const char *buf) {
	char *str = strdup (buf);
	char *ptr = strchr (str, '\n');
	char *optr = str;
	while (ptr) {
		ptr[0]='\0';
		r_core_cmd (user, optr, 0);
		optr = ptr+1;
		ptr = strchr (str,'\n');
	}
	r_core_cmd (user, optr, 0);
	free (str);
	return R_TRUE;
}

R_API int r_core_cmdf(void *user, const char *fmt, ...) {
	char string[1024];
	int ret;
	va_list ap;
	va_start (ap, fmt);
	vsnprintf (string, 1023, fmt, ap);
	ret = r_core_cmd ((struct r_core_t *)user, string, 0);
	va_end(ap);
	return ret;
}

R_API int r_core_cmd0(void *user, const char *cmd) {
	return r_core_cmd ((struct r_core_t *)user, cmd, 0);
}

/*
 * return: pointer to a buffer with the output of the command.
 */
R_API char *r_core_cmd_str(struct r_core_t *core, const char *cmd) {
	char *retstr = NULL;
	r_cons_reset ();
	if (r_core_cmd (core, cmd, 0) == -1) {
		eprintf ("Invalid command: %s\n", cmd);
		retstr = strdup ("");
	} else {
		const char *static_str = r_cons_get_buffer();
		if (static_str==NULL)
			retstr = strdup("");
		else retstr = strdup(static_str);
		r_cons_reset();
	}
	return retstr;
}

int r_core_cmd_init(struct r_core_t *core) {
	r_cmd_init (&core->cmd);
	core->cmd.macro.num = &core->num;
	core->cmd.macro.user = core;
	core->cmd.macro.cmd = r_core_cmd0;
	r_cmd_set_data (&core->cmd, core);
	r_cmd_add (&core->cmd, "x",        "alias for px", &cmd_hexdump);
	r_cmd_add (&core->cmd, "analysis", "analysis", &cmd_anal);
	r_cmd_add (&core->cmd, "flag",     "get/set flags", &cmd_flag);
	r_cmd_add (&core->cmd, "debug",    "debugger operations", &cmd_debug);
	r_cmd_add (&core->cmd, "info",     "get file info", &cmd_info);
	r_cmd_add (&core->cmd, "cmp",      "compare memory", &cmd_cmp);
	r_cmd_add (&core->cmd, "seek",     "seek to an offset", &cmd_seek);
	r_cmd_add (&core->cmd, "Section",  "setup section io information", &cmd_section);
	r_cmd_add (&core->cmd, "bsize",    "change block size", &cmd_bsize);
	r_cmd_add (&core->cmd, "eval",     "evaluate configuration variable", &cmd_eval);
	r_cmd_add (&core->cmd, "print",    "print current block", &cmd_print);
	r_cmd_add (&core->cmd, "write",    "write bytes", &cmd_write);
	r_cmd_add (&core->cmd, "Code",     "code metadata", &cmd_meta);
	r_cmd_add (&core->cmd, "open",     "open or map file", &cmd_open);
	r_cmd_add (&core->cmd, "yank",     "yank bytes", &cmd_yank);
	r_cmd_add (&core->cmd, "Visual",   "enter visual mode", &cmd_visual);
	r_cmd_add (&core->cmd, "undo",     "undo writes", &cmd_undowrite);
	r_cmd_add (&core->cmd, "!",        "run system command", &cmd_system);
	r_cmd_add (&core->cmd, "|",        "run io system command", &cmd_io_system);
	r_cmd_add (&core->cmd, "#",        "calculate hash", &cmd_hash);
	r_cmd_add (&core->cmd, "?",        "help message", &cmd_help);
	r_cmd_add (&core->cmd, ".",        "interpret", &cmd_interpret);
	r_cmd_add (&core->cmd, "/",        "search kw, pattern aes", &cmd_search);
	r_cmd_add (&core->cmd, "(",        "macro", &cmd_macro);
	r_cmd_add (&core->cmd, "|",        "io pipe", &cmd_iopipe);
	r_cmd_add (&core->cmd, "quit",     "exit program session", &cmd_quit);

	return 0;
}
