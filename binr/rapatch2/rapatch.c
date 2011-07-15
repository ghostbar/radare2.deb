/* radare - Copyright 2010-2011 - pancake<nopcode.org> */
#include <stdio.h>
#include <r_core.h>

int main(int argc, char **argv) {
	char *p, *p2, *q, str[200], str2[200];
	char *file = argv[1];
	char *patch = argv[2];
	ut64 noff;
	RCore *core;
	FILE *fd;
	if (argc<3) {
		eprintf ("Usage: rapatch [target] [patchfile ...]\n");
		return 1;
	}
	fd = fopen (patch, "r");
	if (fd==NULL) {
		eprintf ("Cannot open patch file\n");
		return 1;
	}

	core = r_core_new ();
	r_core_file_open (core, file, 2, 0LL);
	r_core_cmdf (core, ".!rabin2 -revSIsi %s", file);
	r_cons_flush ();

	while (!feof (fd)) {
		fgets (str, sizeof (str), fd);
		if (*str=='#' || *str=='\n' || *str=='\r')
			continue;
		if (*str=='.' || *str=='!') {
			r_core_cmd0 (core, str);
			continue;
		}
		p = strchr (str+1, ' ');
		if (p) {
			*p=0;
			for (++p;*p==' ';p++);
			switch (*p) {
			case '{': {
				FILE *fw = fopen ("out.rarc", "w");
				char *off = strdup (str);
				while (!feof (fd)) {
					fgets (str, sizeof (str), fd);
// TODO: replace ${..}
					if (*str=='}')
						break;
					if ((q=strstr (str, "${"))) {
						char *end = strchr (q+2,'}');
						if (end) {
							*q = *end = 0;
							noff = r_num_math (core->num, q+2);
							fwrite (str, strlen (str), 1, fw);
							fprintf (fw, "0x%08llx", noff);
							fwrite (end+1, strlen (end+1), 1, fw);
						}
					} else fwrite (str, strlen (str), 1, fw);
				}
				fclose (fw);
				r_sys_cmd ("rarc2 < out.rarc > out.rasm");
				
				noff = r_num_math (core->num, off);
				r_sys_cmdf ( "rasm2 -o 0x%llx -a x86.olly "
					"-f out.rasm | tee out.hex", noff);
				r_core_cmdf (core, "s %s", off);
				r_core_cmd0 (core, "wF out.hex");
				free (off);
				}
				break;
			case '"':
				p2 = strchr (p+1,'"');
				if (p2) *p2=0;
				r_core_cmdf (core, "s %s", str);
				r_core_cmdf (core, "\" %s\"", p+1);
				break;
			case ':':
				r_core_cmdf (core, "s %s", str);
				r_core_cmdf (core, "wa %s", p);
				break;
			default:
				r_core_cmdf (core, "s %s", str);
				r_core_cmdf (core, "wx %s", p);
				break;
			}
		}
	}
	fclose (fd);
	return 0;
}
