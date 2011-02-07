/* radare - LGPL - Copyright 2009-2010 pancake<nopcode.org> */

#include <r_cons.h>

#if __WINDOWS__
R_API int r_cons_w32_print(ut8 *ptr) {
	HANDLE hConsole = GetStdHandle (STD_OUTPUT_HANDLE);
	int esc = 0;
	int bg = 0, fg = 1|2|4|8;
	ut8 *str = ptr;
	int len = 0;
	int inv = 0;

	if (ptr && hConsole)
	for (;ptr[0]; ptr = ptr + 1) {
		if (ptr[0] == 0x1b) {
			write (1, str, ptr-str);
			esc = 1;
			str = ptr + 1;
			continue;
		}
		if (esc == 1) {
			// \x1b[2J
			if (ptr[0] != '[') {
				eprintf ("Oops invalid escape char\n");
				esc = 0;
				str = ptr + 1;
				continue;
			}
			esc = 2;
			continue;
		} else 
		if (esc == 2) {
			if (ptr[0]=='2'&&ptr[1]=='J') {
				r_cons_clear ();
				esc = 0;
				ptr = ptr + 1;
				str = ptr + 1;
				continue;
			} else
			if (ptr[0]=='0'&&ptr[1]==';'&&ptr[2]=='0') {
				r_cons_gotoxy (0,0);
				esc = 0;
				ptr = ptr + 4;
				str = ptr + 1;
				continue;
			} else
			if (ptr[0]=='0'&&ptr[1]=='m') {
				SetConsoleTextAttribute (hConsole, 1|2|4|8);
				fg = 1|2|4|8;
				bg = 0;
				inv = 0;
				esc = 0;
				ptr = ptr + 1;
				str = ptr + 1;
				continue;
				// reset color
			} else
			if (ptr[0]=='2'&&ptr[1]=='7'&&ptr[2]=='m') {
				SetConsoleTextAttribute (hConsole, bg|fg);
				inv = 0;
				esc = 0;
				ptr = ptr + 2;
				str = ptr + 1;
				continue;
				// invert off
			} else
			if (ptr[0]=='7'&&ptr[1]=='m') {
				SetConsoleTextAttribute (hConsole, bg|fg|128);
				inv = 128;
				esc = 0;
				ptr = ptr + 1;
				str = ptr + 1;
				continue;
				// invert
			} else
			if (ptr[0]=='3' && ptr[2]=='m') {
				// http://www.betarun.com/Pages/ConsoleColor/
				switch(ptr[1]) {
				case '0': // BLACK
					fg = 0;
					break;
				case '1': // RED
					fg = 4;
					break;
				case '2': // GREEN
					fg = 2;
					break;
				case '3': // YELLOW
					fg = 2|4;
					break;
				case '4': // BLUE
					fg = 1;
					break;
				case '5': // MAGENTA
					fg = 1|4;
					break;
				case '6': // TURQOISE
					fg = 1|2|8;
					break;
				case '7': // WHITE
					fg = 1|2|4;
					break;
				case '8': // GRAY
					fg = 8;
					break;
				case '9': // ???
					break;
				}
				SetConsoleTextAttribute (hConsole, bg|fg|inv);
				esc = 0;
				ptr = ptr + 2;
				str = ptr + 1;
				continue;
			} else
			if (ptr[0]=='4' && ptr[2]=='m') {
				/* background color */
				switch(ptr[1]) {
				case '0': // BLACK
					bg = 0;
					break;
				case '1': // RED
					bg = 40;
					break;
				case '2': // GREEN
					bg = 20;
					break;
				case '3': // YELLOW
					bg = 20|40;
					break;
				case '4': // BLUE
					bg = 10;
					break;
				case '5': // MAGENTA
					bg = 10|40;
					break;
				case '6': // TURQOISE
					bg = 10|20|80;
					break;
				case '7': // WHITE
					bg = 10|20|40;
					break;
				case '8': // GRAY
					bg = 80;
					break;
				case '9': // ???
					break;
				}
				esc = 0;
				ptr = ptr + 2;
				str = ptr + 1;
				continue;
			}
		} 
		len++;
	}
	write (1, str, ptr-str);
	return len;
}
#endif
