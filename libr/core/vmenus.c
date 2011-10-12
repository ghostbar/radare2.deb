#include "r_core.h"

R_API int r_core_visual_trackflags(RCore *core) {
	char cmd[1024];
	RListIter *iter;
	RFlagItem *flag;
#define MAX_FORMAT 2
	int format = 0;
	const char *fs = NULL;
	char *fs2 = NULL;
	int option = 0;
	int _option = 0;
	int delta = 7;
	int menu = 0;
	int hit, i, j, ch;

	for (;;) {
		r_cons_gotoxy (0, 0);
		r_cons_clear ();

		if (menu) {
			r_cons_printf ("Flags in flagspace '%s'. Press '?' for help.\n\n",
			(core->flags->space_idx==-1)?"*":core->flags->spaces[core->flags->space_idx]);
			hit = 0;
			i = j = 0;
			r_list_foreach (core->flags->flags, iter, flag) {
				/* filter per flag spaces */
				if ((core->flags->space_idx != -1) && 
					(flag->space != core->flags->space_idx))
					continue;
				if (option==i) {
					fs2 = flag->name;
					hit = 1;
				}
				if ((i>=option-delta) && ((i<option+delta)||((option<delta)&&(i<(delta<<1))))) {
					r_cons_printf (" %c  %03d 0x%08"PFMT64x" %4"PFMT64d" %s\n",
						(option==i)?'>':' ',
						i, flag->offset, flag->size, flag->name);
					j++;
				}
				i++;
			}
			if (!hit && i>0) {
				option = i-1;
				continue;
			}
			r_cons_printf ("\n Selected: %s\n\n", fs2);

			switch (format) {
			case 0: sprintf (cmd, "px @ %s:64", fs2); core->printidx = 0; break;
			case 1: sprintf (cmd, "pd 12 @ %s:64", fs2); core->printidx = 1; break;
			case 2: sprintf (cmd, "ps @ %s:64", fs2); core->printidx = 5; break;
			default: format = 0; continue;
			}
			if (*cmd) r_core_cmd (core, cmd, 0);
		} else {
			r_cons_printf ("Flag spaces:\n\n");
			hit = 0;
			for (j=i=0;i<R_FLAG_SPACES_MAX;i++) {
				if (core->flags->spaces[i]) {
					if (option==i) {
						fs = core->flags->spaces[i];
						hit = 1;
					}
					if ((i >=option-delta) && ((i<option+delta)|| \
							((option<delta)&&(i<(delta<<1))))) {
						r_cons_printf(" %c %02d %c %s\n",
							(option==i)?'>':' ', j, 
							(i==core->flags->space_idx)?'*':' ',
							core->flags->spaces[i]);
						j++;
					}
				}
			}
			{
				if (option == j) {
					fs = "*";
					hit = 1;
				}
				r_cons_printf (" %c %02d %c %s\n",
					(option==j)?'>':' ', j, 
					(i==core->flags->space_idx)?'*':' ',
					"*");
			}
			if (!hit && j>0) {
				option = j-1;
				continue;
			}
		}
		r_cons_flush ();
		ch = r_cons_readchar ();
		ch = r_cons_arrow_to_hjkl (ch); // get ESC+char, return 'hjkl' char
		switch (ch) {
		case 'J':
			option += 10;
			break;
		case 'o':
			r_flag_sort (core->flags, 0);
			break;
		case 'n':
			r_flag_sort (core->flags, 1);
			break;
		case 'j':
			option++;
			break;
		case 'k':
			if (--option<0)
				option = 0;
			break;
		case 'K':
			option-=10;
			if (option<0)
				option = 0;
			break;
		case 'h':
		case 'b': // back
			menu = 0;
			option = _option;
			break;
		case 'a':
			switch (menu) {
			case 0: // new flag space
				r_line_set_prompt ("add flagspace: ");
				strcpy (cmd, "fs ");
				if (r_cons_fgets (cmd+3, sizeof (cmd)-4, 0, NULL) > 0) {
					r_core_cmd (core, cmd, 0);
					r_cons_set_raw (1);
				}
				break;
			case 1: // new flag
				r_line_set_prompt ("add flag: ");
				strcpy (cmd, "f ");
				if (r_cons_fgets (cmd+2, sizeof (cmd)-3, 0, NULL) > 0) {
					r_core_cmd (core, cmd, 0);
					r_cons_set_raw (1);
				}
				break;
			}
			break;
		case 'd':
			r_flag_unset (core->flags, fs2, NULL);
			break;
		case 'e':
			/* TODO: prompt for addr, size, name */
			eprintf ("TODO\n");
			r_sys_sleep (1);
			break;
		case 'q':
			if (menu<=0) return R_TRUE; menu--;
			break;
		case '*':
			r_core_block_size (core, core->blocksize+16);
			break;
		case '+':
			r_core_block_size (core, core->blocksize+1);
			break;
		case '/':
			r_core_block_size (core, core->blocksize-16);
			break;
		case '-':
			r_core_block_size (core, core->blocksize-1);
			break;
		case 'r':
			if (menu == 1) {
				int len;
				r_cons_set_raw (0);
				// TODO: use r_flag_rename or wtf?..fr doesnt uses this..
				snprintf (cmd, sizeof (cmd), "fr %s ", fs2);
				len = strlen (cmd);
				eprintf ("Rename flag '%s' as:\n", fs2);
				r_line_set_prompt (":> ");
				if (r_cons_fgets (cmd+len, sizeof (cmd)-len-1, 0, NULL) <0)
					cmd[0]='\0';
				r_core_cmd (core, cmd, 0);
				r_cons_set_raw (1);
			}
			break;
		case 'P':
			if (--format<0)
				format = MAX_FORMAT;
			break;
		case 'p':
			format++;
			break;
		case 'l':
		case ' ':
		case '\r':
		case '\n':
			if (menu == 1) {
				sprintf (cmd, "s %s", fs2);
				r_core_cmd (core, cmd, 0);
				return R_TRUE;
			}
			r_flag_space_set (core->flags, fs);
			menu = 1;
			_option = option;
			option = 0;
			break;
		case '?':
			r_cons_clear00 ();
			r_cons_printf (
			"\nVt: Visual Track help:\n\n"
			" q     - quit menu\n"
			" j/k   - down/up keys\n"
			" h/b   - go back\n"
			" l/' ' - accept current selection\n"
			" a/d/e - add/delete/edit flag\n"
			" +/-   - increase/decrease block size\n"
			" o     - sort flags by offset\n"
			" r     - rename flag\n"
			" n     - sort flags by name\n"
			" p/P   - rotate print format\n"
			" :     - enter command\n");
			r_cons_flush ();
			r_cons_any_key ();
			break;
		case ':':
			r_cons_set_raw (0);
			cmd[0]='\0';
			r_line_set_prompt (":> ");
			if (r_cons_fgets (cmd, sizeof (cmd)-1, 0, NULL) <0)
				cmd[0]='\0';
			//line[strlen(line)-1]='\0';
			r_core_cmd (core, cmd, 1);
			r_cons_set_raw (1);
			if (cmd[0])
				r_cons_any_key ();
			//cons_gotoxy(0,0);
			r_cons_clear ();
			continue;
		}
	}
	return R_TRUE;
}

static void config_visual_hit_i(RCore *core, const char *name, int delta) {
	struct r_config_node_t *node;
	node = r_config_node_get (core->config, name);
	if (node && ((node->flags & CN_INT) || (node->flags & CN_OFFT)))
		r_config_set_i(core->config, name, r_config_get_i(core->config, name)+delta);
}

/* Visually activate the config variable */
static void config_visual_hit(RCore *core, const char *name, int editor) {
	char buf[1024];
	RConfigNode *node;

	if (!(node = r_config_node_get (core->config, name)))
		return;
	if (node->flags & CN_BOOL) {
		/* TOGGLE */
		node->i_value = !node->i_value;
		node->value = r_str_dup (node->value, node->i_value?"true":"false");
	} else {
		if (editor) {
			char * buf = r_core_editor (core, node->value);
			node->value = r_str_dup (node->value, buf);
			free (buf);
		} else {
			// FGETS AND SO
			r_cons_printf ("New value (old=%s): \n", node->value);
			r_cons_flush ();
			r_cons_set_raw (0);
			r_line_set_prompt (":> ");
			r_cons_fgets (buf, sizeof (buf)-1, 0, 0);
			r_cons_set_raw (1);
			node->value = r_str_dup (node->value, buf);
		}
	}
}

R_API void r_core_visual_config(RCore *core) {
	RListIter *iter;
	RConfigNode *bt;
	char cmd[1024];
#define MAX_FORMAT 2
	char *fs = NULL;
	char *fs2 = NULL;
	int option = 0;
	int _option = 0;
	int delta = 9;
	int menu = 0;
	int i,j, ch;
	int hit;
	int show;
	char old[1024];
	old[0]='\0';

	for (;;) {
		r_cons_gotoxy (0,0);
		r_cons_clear ();

		switch (menu) {
		case 0: // flag space
			r_cons_printf ("\n Eval spaces:\n\n");
			hit = 0;
			j = i = 0;
			r_list_foreach (core->config->nodes, iter, bt) {
				if (option==i) {
					fs = bt->name;
					hit = 1;
				}
				if (old[0]=='\0') {
					r_str_ccpy (old, bt->name, '.');
					show = 1;
				} else if (r_str_ccmp (old, bt->name, '.')) {
					r_str_ccpy (old, bt->name, '.');
					show = 1;
				} else show = 0;

				if (show) {
					if( (i >=option-delta) && ((i<option+delta)||((option<delta)&&(i<(delta<<1))))) {
						r_cons_printf(" %c  %s\n", (option==i)?'>':' ', old);
						j++;
					}
					i++;
				}
			}
			if (!hit && j>0) {
				option = j-1;
				continue;
			}
			r_cons_printf ("\n Sel:%s \n\n", fs);
			break;
		case 1: // flag selection
			r_cons_printf ("\n Eval variables: (%s)\n\n", fs);
			hit = 0;
			j = i = 0;
			// TODO: cut -d '.' -f 1 | sort | uniq !!!
			r_list_foreach (core->config->nodes, iter, bt) {
				if (option==i) {
					fs2 = bt->name;
					hit = 1;
				}
				if (!r_str_ccmp (bt->name, fs, '.')) {
					if ( (i>=option-delta) && ((i<option+delta)||((option<delta)&&(i<(delta<<1))))) {
						// TODO: Better align
						r_cons_printf (" %c  %s = %s\n", (option==i)?'>':' ', bt->name, bt->value);
						j++;
					}
					i++;
				}
			}
			if (!hit && j>0) {
				option = i-1;
				continue;
			}
			if (fs2 != NULL)
				r_cons_printf ("\n Selected: %s\n\n", fs2);
		}

		if (fs&&!memcmp (fs, "asm.", 4))
			r_core_cmd (core, "pd 5", 0);
		r_cons_flush ();
		ch = r_cons_readchar ();
		ch = r_cons_arrow_to_hjkl (ch); // get ESC+char, return 'hjkl' char

		switch (ch) {
		case 'j':
			option++;
			break;
		case 'k':
			if (--option<0)
				option = 0;
			break;
		case 'h':
		case 'b': // back
			menu = 0;
			option = _option;
			break;
		case 'q':
			if (menu<=0) return; menu--;
			break;
		case '*':
		case '+':
			if (fs2 != NULL)
				config_visual_hit_i (core, fs2, +1);
			continue;
		case '/':
		case '-':
			if (fs2 != NULL)
				config_visual_hit_i (core, fs2, -1);
			continue;
		case 'l':
		case 'E': // edit value
		case 'e': // edit value
		case ' ':
		case '\r':
		case '\n': // never happens
			if (menu == 1) {
				if (fs2 != NULL)
					config_visual_hit (core, fs2, (ch=='E'));
			} else {
				r_flag_space_set (core->flags, fs);
				menu = 1;
				_option = option;
				option = 0;
			}
			break;
		case '?':
			r_cons_clear00 ();
			r_cons_printf ("\nVe: Visual Eval help:\n\n");
			r_cons_printf (" q     - quit menu\n");
			r_cons_printf (" j/k   - down/up keys\n");
			r_cons_printf (" h/b   - go back\n");
			r_cons_printf (" e/' ' - edit/toggle current variable\n");
			r_cons_printf (" E     - edit variable with 'cfg.editor' (vi?)\n");
			r_cons_printf (" +/-   - increase/decrease numeric value\n");
			r_cons_printf (" :     - enter command\n");
			r_cons_flush ();
			r_cons_any_key ();
			break;
		case ':':
			r_cons_set_raw(0);
/* WTF READLINE?? WE DONT USE THAT!! */
#if HAVE_LIB_READLINE
			{
			char *ptr = readline(VISUAL_PROMPT);
			if (ptr) {
				strncpy(cmd, ptr, sizeof (cmd)-1);
				r_core_cmd(core, cmd, 1);
				free(ptr);
			}
			}
#else
			*cmd = '\0';
			if (r_cons_fgets (cmd, sizeof (cmd)-1, 0, NULL) <0)
				cmd[0]='\0';
			//line[strlen(line)-1]='\0';
			r_core_cmd (core, cmd, 1);
#endif
			r_cons_set_raw (1);
			if (cmd[0])
				r_cons_any_key ();
			//r_cons_gotoxy(0,0);
			r_cons_clear00 ();
			continue;
		}
	}
}

R_API void r_core_visual_mounts (RCore *core) {
	RList *list;
	RListIter *iter;
	RFSFile *file;
	RFSPartition *part;
	int i, ch, option, mode, partition, dir, delta = 7;
	char *str, path[4096], buf[1024], *root = NULL;
	const char *n, *p;

	dir = partition = option = mode = 0;
	for (;;) {
		/* Clear */
		r_cons_gotoxy (0,0);
		r_cons_clear ();

		/* Show */
		if (mode == 0) {
			r_cons_printf ("Partitions:\n\n");
			n = r_fs_partition_type_get (partition);
			list = r_fs_partitions (core->fs, n, 0);
			i = 0;
			if (list) {
				r_list_foreach (list, iter, part) {
					if ((option-delta <= i) && (i <= option+delta)) {
						if (option == i)
							r_cons_printf (" > ");
						else r_cons_printf ("   ");
						r_cons_printf ("%d %02x 0x%010"PFMT64x" 0x%010"PFMT64x"\n",
								part->number, part->type,
								part->start, part->start+part->length);
					}
					i++;
				}
				r_list_free (list);
			} else r_cons_printf ("Cannot read partition\n");
		} else if (mode == 1) {
			r_cons_printf ("Types:\n\n");
			for(i=0;;i++) {
				n = r_fs_partition_type_get (i);
				if (!n) break;
				r_cons_printf ("%s%s\n", (i==partition)?" > ":"   ", n);
			}
		} else {
			if (root) {
				list = r_fs_dir (core->fs, path);
				if (list) {
					r_cons_printf ("%s:\n\n", path);
					i = 0;
					r_list_foreach (list, iter, file) {
						if ((dir-delta <= i) && (i <= dir+delta)) {
							r_cons_printf ("%s%c %s\n", (dir == i)?" > ":"   ",
									file->type, file->name);
						}
						i++;
					}
					r_cons_printf ("\n");
					r_list_free (list);
				} else r_cons_printf ("Cannot open '%s' directory\n", root);
			} else r_cons_printf ("Root undefined\n");
		}
		if (mode==2) {
			r_str_chop_path (path);
			str = path + strlen (path);
			strncat (path, "/", sizeof (path)-strlen (path)-1);
			list = r_fs_dir (core->fs, path);
			file = r_list_get_n (list, dir);
			if (file && file->type != 'd')
				r_core_cmdf (core, "x @ 0x%"PFMT64x":32", file->off);
			*str='\0';
		}
		r_cons_flush ();

		/* Ask for option */
		ch = r_cons_readchar ();
		ch = r_cons_arrow_to_hjkl (ch);
		switch (ch) {
			case 'l':
			case '\r':
			case '\n':
				if (mode == 0) {
					n = r_fs_partition_type_get (partition);
					list = r_fs_partitions (core->fs, n, 0);
					if (!list) {
						r_cons_printf ("Unknown partition\n");
						r_cons_any_key ();
						r_cons_flush ();
						break;
					}
					part = r_list_get_n (list, option);
					if (!part) {
						r_cons_printf ("Unknown partition\n");
						r_cons_any_key ();
						r_cons_flush ();
						break;
					}
					p = r_fs_partition_type (n, part->type);
					if (p) {
						if (r_fs_mount (core->fs, p, "/root", part->start)) {
							if (root)
								free (root);
							root = strdup ("/root");
							strncpy (path, root, sizeof (path)-1);
							mode = 2;
						} else {
							r_cons_printf ("Cannot mount partition\n");
							r_cons_flush ();
							r_cons_any_key ();
						}
					} else {
						r_cons_printf ("Unknown partition type\n");
						r_cons_flush ();
						r_cons_any_key ();
					}
				} else if (mode == 2){
					r_str_chop_path (path);
					strncat (path, "/", sizeof (path)-strlen (path)-1);
					list = r_fs_dir (core->fs, path);
					file = r_list_get_n (list, dir);
					if (file) {
						if (file->type == 'd') {
							strncat (path, file->name, sizeof (path)-strlen (path)-1);
							r_str_chop_path (path);
							if (memcmp (root, path, strlen (root)-1))
								strncpy (path, root, sizeof (path)-1);
						} else {
							r_core_cmdf (core, "s 0x%"PFMT64x, file->off);
							r_fs_umount (core->fs, root);
							return;
						}
					} else {
						r_cons_printf ("Unknown file\n");
						r_cons_flush ();
						r_cons_any_key ();
					}
				}
				dir = partition = option = 0;
				break;
			case 'k':
				if (mode == 0) {
					if (option > 0)
						option--;
				} else if (mode == 1) {
					if (partition > 0)
						partition--;
				} else {
					if (dir>0)
						dir--;
				}
				break;
			case 'j':
				if (mode == 0) {
					n = r_fs_partition_type_get (partition);
					list = r_fs_partitions (core->fs, n, 0);
					if (option < r_list_length (list)-1)
						option++;
				} else if (mode == 1) {
					if (partition < r_fs_partition_get_size ()-1)
						partition++;
				} else {
					list = r_fs_dir (core->fs, path);
					if (dir < r_list_length (list)-1)
						dir++;
				}
				break;
			case 't':
				mode = 1;
				break;
			case 'h':
				if (mode == 2) {
					if (strcmp (path, root)) {
						strcat (path, "/..");
						r_str_chop_path (path);
					} else {
						r_fs_umount (core->fs, root);
						mode = 0;
					}
				} else if (mode == 1)
					mode = 0;
				else
					return;
				break;
			case 'q':
				if (mode == 2 && root) {
					r_fs_umount (core->fs, root);
					mode = 0;
				} else
					return;
				break;
			case 'g':
				if (mode == 2){
					r_str_chop_path (path);
					str = path + strlen (path);
					strncat (path, "/", sizeof (path)-strlen (path)-1);
					list = r_fs_dir (core->fs, path);
					file = r_list_get_n (list, dir);
					if (file) {
						strncat (path, file->name, sizeof (path)-strlen (path)-1);
						r_str_chop_path (path);
						if (memcmp (root, path, strlen (root)-1))
							strncpy (path, root, sizeof (path)-1);
						file = r_fs_open (core->fs, path);
						if (file) {
							r_fs_read (core->fs, file, 0, file->size);
							r_cons_set_raw (0);
							r_line_set_prompt ("Dump path (ej: /tmp/file): ");
							r_cons_fgets (buf, sizeof (buf)-1, 0, 0);
							r_cons_set_raw (1);
							r_file_dump (buf, file->data, file->size);
							r_fs_close (core->fs, file);
							r_cons_printf ("Done\n");
						} else {
							r_cons_printf ("Cannot dump file\n");
						}
					} else {
						r_cons_printf ("Cannot dump file\n");
					}
					r_cons_flush ();
					r_cons_any_key ();
					*str='\0';
				}
				break;
			case '?':
				r_cons_clear00 ();
				r_cons_printf ("\nVM: Visual Mount points help:\n\n");
				r_cons_printf (" q     - go back or quit menu\n");
				r_cons_printf (" j/k   - down/up keys\n");
				r_cons_printf (" h/l   - forward/go keys\n");
				r_cons_printf (" t     - choose partition type\n");
				r_cons_printf (" g     - dump file\n");
				r_cons_printf (" :     - enter command\n");
				r_cons_printf (" ?     - show this help\n");
				r_cons_flush ();
				r_cons_any_key ();
				break;
			case ':':
				r_cons_set_raw (0);
				r_line_set_prompt (":> ");
				r_cons_fgets (buf, sizeof (buf)-1, 0, 0);
				r_cons_set_raw (1);
				r_core_cmd (core, buf, 1);
				r_cons_any_key ();
				break;
		}
	}
}

#if 1
static void var_index_show(RAnal *anal, RAnalFcn *fcn, ut64 addr, int idx) {
	int i = 0;
	RAnalVar *v;
	RAnalVarAccess *x;
	RListIter *iter, *iter2;
	int window = 15;
	int wdelta = (idx>5)?idx-5:0;
	if (!fcn)
		return;
	r_list_foreach(fcn->vars, iter, v) {
		if (addr == 0 || (addr >= v->addr && addr <= v->eaddr)) {
			if (i>=wdelta) {
				if (i> window+wdelta) {
					r_cons_printf("...\n");
					break;
				}
				if (idx == i) r_cons_printf (" * ");
				else r_cons_printf ("   ");
				r_cons_printf ("0x%08llx - 0x%08llx type=%s type=%s name=%s delta=%d array=%d\n",
					v->addr, v->eaddr, r_anal_var_type_to_str (anal, v->type),
					v->vartype, v->name, v->delta, v->array);
				r_list_foreach (v->accesses, iter2, x) {
					r_cons_printf ("  0x%08llx %s\n", x->addr, x->set?"set":"get");
				}
			}
			i++;
		}
	}
}

// helper
static void function_rename(RCore *core, ut64 addr, const char *name) {
	RListIter *iter;
	RAnalFcn *fcn;

	r_list_foreach (core->anal->fcns, iter, fcn) {
		if (fcn->addr == addr) {
			r_flag_unset (core->flags, fcn->name, NULL);
			free (fcn->name);
			fcn->name = strdup (name);
			r_flag_set (core->flags, name, addr, fcn->size, 0);
			break;
		}
	}
}

static ut64 var_functions_show(RCore *core, int idx, int show) {
	int i = 0;
	ut64 seek = core->offset;
	ut64 addr = core->offset;
	int window = 15;
	int wdelta = (idx>5)?idx-5:0;
	RListIter *iter;
	RAnalFcn *fcn;

	r_list_foreach (core->anal->fcns, iter, fcn) {
		if (i>=wdelta) {
			if (i> window+wdelta) {
				r_cons_printf("...\n");
				break;
			}
			//if (seek >= fcn->addr && seek <= fcn->addr+fcn->size)
			if (idx == i)
				addr = fcn->addr;
			if (show)
				r_cons_printf ("%c%c 0x%08llx (%s)\n", 
					(seek == fcn->addr)?'>':' ',
					(idx==i)?'*':' ',
					fcn->addr, fcn->name);
		}
		i++;
	}
	return addr;
}

/* Like emenu but for real */
R_API void r_core_visual_anal(RCore *core) {
	int option = 0;
	int _option = 0;
	int ch, level = 0;
	char old[1024], *oprofile;
	ut64 addr = core->offset;
	old[0]='\0';
	RAnalFcn *fcn = r_anal_fcn_find (core->anal, core->offset, R_ANAL_FCN_TYPE_NULL);

	for (;;) {
		r_cons_gotoxy (0,0);
		r_cons_clear ();
		r_cons_printf ("Visual code analysis manipulation\n");

		if (!level)
			addr = var_functions_show (core, option, 0);

		oprofile = strdup (r_config_get (core->config, "asm.profile"));
		r_config_set (core->config, "asm.profile", "simple");
		r_core_cmdf (core, "pd @ 0x%"PFMT64x":32", addr);
		r_config_set (core->config, "asm.profile", oprofile);

		r_cons_column (32);
		switch (level) {
		case 0:
			r_cons_printf ("-[ functions ]---------------- \n"
				"(a) add     (x)xrefs    (q)quit\n"
				"(m) modify  (c)calls    (g)go\n"
				"(d) delete  (v)variables\n");
			addr = var_functions_show (core, option, 1);
			break;
		case 1:
			r_cons_printf ("-[ variables ]---------------- 0x%08"PFMT64x"\n"
				"(a) add     (x)xrefs     (q)quit\n"
				"(m) modify  (c)calls     (g)go\n"
				"(d) delete  (v)variables\n", addr);
			var_index_show (core->anal, fcn, addr, option);
			break;
		case 2:
			r_cons_printf ("-[ calls ]----------------------- 0x%08"PFMT64x" (TODO)\n", addr);
#if 0
			sprintf(old, "aCf@0x%08llx", addr);
			cons_flush();
			radare_cmd(old, 0);
#endif
			break;
		case 3:
			r_cons_printf ("-[ xrefs ]----------------------- 0x%08"PFMT64x"\n", addr);
			sprintf (old, "arl~0x%08"PFMT64x, addr);
			r_core_cmd0 (core, old);
			//cons_printf("\n");
			break;
		}
		r_cons_flush ();
// show indexable vars
		ch = r_cons_readchar ();
		ch = r_cons_arrow_to_hjkl (ch); // get ESC+char, return 'hjkl' char
		switch (ch) {
		case ':':
			r_core_visual_prompt (core);
			continue;
		case 'a':
			switch (level) {
			case 0:
eprintf ("TODO: Add new function manually\n");
/*
				r_cons_set_raw (R_FALSE);
				r_line_set_prompt ("Address: ");
				if (!r_cons_fgets (old, sizeof (old), 0, NULL)) break;
				old[strlen (old)-1] = 0;
				if (!*old) break;
				addr = r_num_math (core->num, old);
				r_line_set_prompt ("Size: ");
				if (!r_cons_fgets (old, sizeof (old), 0, NULL)) break;
				old[strlen (old)-1] = 0;
				if (!*old) break;
				size = r_num_math (core->num, old);
				r_line_set_prompt ("Name: ");
				if (!r_cons_fgets (old, sizeof (old), 0, NULL)) break;
				old[strlen (old)-1] = 0;
				r_flag_set (core->flags, old, addr, 0, 0);
				//XXX sprintf(cmd, "CF %lld @ 0x%08llx", size, addr);
				// XXX r_core_cmd0(core, cmd);
				r_cons_set_raw (R_TRUE);
*/
				break;
			case 1:
				break;
			}
			break;
		case 'm':
			r_cons_set_raw (R_FALSE);
			r_line_set_prompt ("New name: ");
			if (!r_cons_fgets (old, sizeof (old), 0, NULL)) break;
			//old[strlen (old)-1] = 0;
			function_rename (core, addr, old);
		
			r_cons_set_raw (R_TRUE);
			break;
		case 'd':
			switch (level) {
			case 0:
				eprintf ("TODO\n");
				//data_del(addr, DATA_FUN, 0);
				// XXX correcly remove all the data contained inside the size of the function
				//flag_remove_at(addr);
				break;
			}
			break;
		case 'x':
			level = 3;
			break;
		case 'c':
			level = 2;
			break;
		case 'v':
			level = 1;
			break;
		case 'j':
			option++;
			break;
		case 'k':
			if (--option<0)
				option = 0;
			break;
		case 'g': // go!
			r_core_seek (core, addr, SEEK_SET);
			return;
		case ' ':
		case 'l':
			level = 1;
			_option = option;
			break;
		case 'h':
		case 'b': // back
			level = 0;
			option = _option;
			break;
		case 'q':
			if (level==0)
				return;
			level = 0;
			break;
		}
	}
}
#endif

R_API void r_core_seek_next(RCore *core, const char *type) {
	RListIter *iter;
	ut64 next = UT64_MAX;
	if (strstr (type, "opc")) {
		RAnalOp aop;
		if (r_anal_op (core->anal, &aop, core->offset, core->block, core->blocksize))
			next = core->offset + aop.length;
		else eprintf ("Invalid opcode\n");
	} else
	if (strstr (type, "fun")) {
		RAnalFcn *fcni;
		r_list_foreach (core->anal->fcns, iter, fcni) {
			if (fcni->addr < next && fcni->addr > core->offset)
				next = fcni->addr;
		}
	} else
	if (strstr (type, "hit")) {
		const char *pfx = r_config_get (core->config, "search.prefix");
		RFlagItem *flag;
		r_list_foreach (core->flags->flags, iter, flag) {
			if (!memcmp (flag->name, pfx, strlen (pfx)))
				if (flag->offset < next && flag->offset > core->offset)
					next = flag->offset;
		}
	} else { // flags
		RFlagItem *flag;
		r_list_foreach (core->flags->flags, iter, flag) {
			if (flag->offset < next && flag->offset > core->offset)
				next = flag->offset;
		}
	}
	if (next!=UT64_MAX)
		r_core_seek (core, next, 1);
}

R_API void r_core_seek_previous (RCore *core, const char *type) {
	RListIter *iter;
	ut64 next = 0;
	if (strstr (type, "opc")) {
		eprintf ("TODO: r_core_seek_previous (opc)\n");
	} else
	if (strstr (type, "fun")) {
		RAnalFcn *fcni;
		r_list_foreach (core->anal->fcns, iter, fcni) {
			if (fcni->addr > next && fcni->addr < core->offset)
				next = fcni->addr;
		}
	} else
	if (strstr (type, "hit")) {
		RFlagItem *flag;
		const char *pfx = r_config_get (core->config, "search.prefix");
		r_list_foreach (core->flags->flags, iter, flag) {
			if (!memcmp (flag->name, pfx, strlen (pfx)))
				if (flag->offset > next && flag->offset< core->offset)
					next = flag->offset;
		}
	} else { // flags
		RFlagItem *flag;
		r_list_foreach (core->flags->flags, iter, flag) {
			if (flag->offset > next && flag->offset < core->offset)
				next = flag->offset;
		}
	}
	if (next!=0)
		r_core_seek (core, next, 1);
}

R_API void r_core_visual_define (RCore *core) {
	int ch, ntotal = 0;
	ut64 off = core->offset;
	ut8 *p = core->block;
	int plen = core->blocksize;
	if (core->print->cur_enabled) {
		off += core->print->cur;
		p += core->print->cur;
		plen -= core->print->cur;
	}
	r_cons_printf ("Define current block as:\n"
		" d  - set as data\n"
		" c  - set as code\n"
		" s  - set string\n"
		" S  - set strings in current block\n"
		" f  - analyze function\n"
		" u  - undefine metadata here\n"
		" q  - quit/cancel operation\n"
		"TODO: add support for data, string, code ..\n");
	r_cons_flush ();

	// get ESC+char, return 'hjkl' char
	ch = r_cons_arrow_to_hjkl (r_cons_readchar ());

	switch (ch) {
	case 'S':
		do {
			char *name;
			int n = r_str_nlen ((const char*)p+ntotal, plen-ntotal)+1;
			name = malloc (n+10);
			strcpy (name, "str.");
			strncpy (name+4, (const char *)p+ntotal, n);
			r_flag_set (core->flags, name, off, n, 0);
			r_meta_add (core->anal->meta, R_META_TYPE_STRING,
				off+ntotal, off+n+ntotal, (const char *)p+ntotal);
			free (name);
			if (n<2) break;
			ntotal+= n;
		} while (ntotal<core->blocksize);
		break;
	case 's':
		{
			char *name;
			int n = r_str_nlen ((const char*)p, plen)+1;
			name = malloc (n+10);
			strcpy (name, "str.");
			strncpy (name+4, (const char *)p, n);
			r_flag_set (core->flags, name, off, n, 0);
			r_meta_add (core->anal->meta, R_META_TYPE_STRING, off, off+n, (const char *)p);
			free (name);
		}
		break;
	case 'd': // TODO: check
		r_meta_add (core->anal->meta, R_META_TYPE_DATA, off, off+core->blocksize, "");
		break;
	case 'c': // TODO: check 
		r_meta_add (core->anal->meta, R_META_TYPE_CODE, off, off+core->blocksize, "");
		break;
	case 'u':
		r_meta_del (core->anal->meta, R_META_TYPE_ANY, off, 1, "");
		r_flag_unset_i (core->flags, off, NULL);
		r_anal_fcn_del (core->anal, off);
		break;
	case 'f':
		r_core_anal_fcn (core, off, -1, R_ANAL_REF_TYPE_NULL,
				r_config_get_i (core->config, "anal.depth"));
		break;
	case 'q':
	default:
		break;
	}
}
