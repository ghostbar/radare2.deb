/* radare - LGPL - Copyright 2007-2011 pancake<nopcode.org> */

#include <r_util.h>

static int flags = 0;

static int format_output (char mode, ut64 n);
static int help ();
static int rax (char *str, int last);
static int use_stdin ();

static int format_output (char mode, ut64 n) {
	char *str = (char*) &n;
	char strbits[65];

	if (flags & 2)
		r_mem_copyendian ((ut8*) str, (ut8*) str, 4, 0);
	switch (mode) {
	case 'I':
		printf ("%"PFMT64d"\n", n);
		break;
	case '0':
		printf ("0x%"PFMT64x"\n", n);
		break;
	case 'F':
		printf ("%ff\n", (float)(ut32)n);
		break;
	case 'B':
		if (n) {
			r_num_to_bits (strbits, n);
			printf ("%sb\n", strbits);
		} else printf ("0b\n");
		break;
	case 'O':
		printf ("%"PFMT64o"\n", n);
		break;
	}
	return R_TRUE;
}

static int help () {
	printf (
		"  int   ->  hex           ;  rax2 10\n"
		"  hex   ->  int           ;  rax2 0xa\n"
		"  -int  ->  hex           ;  rax2 -77\n"
		"  -hex  ->  int           ;  rax2 0xffffffb3\n"
		"  int   ->  bin           ;  rax2 b30\n"
		"  bin   ->  int           ;  rax2 1010d\n"
		"  float ->  hex           ;  rax2 3.33f\n"
		"  hex   ->  float         ;  rax2 Fx40551ed8\n"
		"  oct   ->  hex           ;  rax2 35o\n"
		"  hex   ->  oct           ;  rax2 Ox12 (O is a letter)\n"
		"  bin   ->  hex           ;  rax2 1100011b\n"
		"  hex   ->  bin           ;  rax2 Bx63\n"
		"  -e    swap endianness   ;  rax2 -e 0x33\n"
		"  -b    binstr -> bin     ;  rax2 -b 01000101 01110110\n"
		"  -s    hexstr -> bin     ;  rax2 -s 43 4a 50\n"
		"  -S    bin -> hexstr     ;  rax2 -S C  J  P\n"
		"  -V    version           ;  rax2 -V\n"
		"  -h    help              ;  rax2 -h\n");
	return R_TRUE;
}

static int rax (char *str, int last) {
	float f;
	char *p, *buf, out_mode = '0';
	int i;

	if (*str=='-') {
		switch (str[1]) {
		case 's':
			flags ^= 1;
			break;
		case 'e':
			flags ^= 2;
			break;
		case 'S':
			flags ^= 4;
			break;
		case 'b':
			flags ^= 8;
			break;
		case 'V':
			printf ("rax2 v"R2_VERSION"\n");
			break;
		case '\0':
			return use_stdin ();
		default:
			printf ("Usage: rax2 [options] [expression]\n");
			return help ();
		}
		if (last)
			return use_stdin ();
		return R_TRUE;
	} else
	if (*str=='q')
		return R_FALSE;
	else
	if (*str=='h' || *str=='?')
		return help ();

	if (flags & 1) {
		ut64 n = ((strlen (str))>>1)+1;
		buf = malloc (sizeof (char) * n);
		memset (buf, '\0', n);
		n = r_hex_str2bin (str, (ut8*)buf);
		write (1, buf, n);
		free (buf);
		return R_TRUE;
	}
	if (flags & 4) {
		for (i=0; str[i]; i++)
			printf ("%02x", str[i]);
		printf ("\n");
		return R_TRUE;
	}
	if (flags & 8) {
		int i, len;
		ut8 buf[4096];
		len = r_str_binstr2bin (str, buf, sizeof (buf));
		if (len>0)
			for (i=0; i<len; i++)
				printf ("%c", buf[i]);
		return R_TRUE;
	}

	if (str[0]=='0' && str[1]=='x') {
		out_mode = 'I';
	} else if (str[0]=='b') {
		out_mode = 'B';
		str++;
	} else if (str[strlen(str)-1]=='d') {
		out_mode = 'I';
		str[strlen(str)-1] = 'b';
	} else if (str[0]=='F' && str[1]=='x') {
		out_mode = 'F';
		*str = '0';
	} else if (str[0]=='B' && str[1]=='x') {
		out_mode = 'B';
		*str = '0';
	} else if (str[0]=='O' && str[1]=='x') {
		out_mode = 'O';
		*str = '0';
	//TODO: Move print into format_output
	} else if (str[strlen(str)-1]=='f') {
		unsigned char *p = (unsigned char *)&f;
		sscanf (str, "%f", &f);
		printf ("Fx%02x%02x%02x%02x\n", p[0], p[1], p[2], p[3]);
		return R_TRUE;
	}
	while ((p = strchr (str, ' '))) {
		*p = 0;
		format_output (out_mode, r_num_math (NULL, str));
		str = p+1;
	}
	if (*str)
		format_output (out_mode, r_num_math (NULL, str));
	return R_TRUE;
}

static int use_stdin () {
	char buf[4096]; // TODO: remove this limit
	while (!feof (stdin)) {
		fgets (buf, sizeof (buf), stdin);
		if (feof (stdin)) break;
		buf[strlen (buf)-1] = '\0';
		if (!rax (buf, 0)) break;
	}
	return 0;
}

int main (int argc, char **argv) {
	int i;
	if (argc == 1)
		return use_stdin ();
	for (i=1; i<argc; i++)
		rax (argv[i], (i+1)==argc);
	return 0;
}
