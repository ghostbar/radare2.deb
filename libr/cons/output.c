/* radare - LGPL - Copyright 2009-2010 pancake<nopcode.org> */

#include <r_cons.h>

#if __WINDOWS__
int r_cons_w32_print(ut8 *ptr)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	int esc = 0;
	int bg = 0;
	ut8 *str = ptr;
	int len = 0;
	int inv = 0;

	for (;ptr[0]; ptr = ptr + 1) {
		if (ptr[0] == 0x1b) {
			esc = 1;
			write(1, str, ptr-str);
			str = ptr + 1;
			continue;
		}
		if (esc == 1) {
			// \x1b[2J
			if (ptr[0] != '[') {
				eprintf("Oops invalid escape char\n");
				esc = 0;
				str = ptr + 1;
				continue;
			}
			esc = 2;
			continue;
		} else 
		if (esc == 2) {
			if (ptr[0]=='2'&&ptr[1]=='J') {
				ptr = ptr +1;
				r_cons_clear();
				esc = 0;
				str = ptr;
				continue;
			} else
			if (ptr[0]=='0'&&ptr[1]==';'&&ptr[2]=='0') {
				ptr = ptr + 4;
				r_cons_gotoxy(0,0);
				esc = 0;
				str = ptr;
				continue;
			} else
			if (ptr[0]=='0'&&ptr[1]=='m') {
				SetConsoleTextAttribute(hConsole, 1|2|4|8);
				ptr = ptr + 1;
				str = ptr + 1;
				inv = 0;
				esc = 0;
				continue;
				// reset color
			} else
			if (ptr[0]=='7'&&ptr[1]=='m') {
				SetConsoleTextAttribute(hConsole, 128);
				inv = 128;
				ptr = ptr + 1;
				str = ptr + 1;
				esc = 0;
				continue;
				// reset color
			} else
			if (ptr[0]=='3' && ptr[2]=='m') {
				// http://www.betarun.com/Pages/ConsoleColor/
				switch(ptr[1]) {
				case '0': // BLACK
					SetConsoleTextAttribute(hConsole, bg|0|inv);
					break;
				case '1': // RED
					SetConsoleTextAttribute(hConsole, bg|4|inv);
					break;
				case '2': // GREEN
					SetConsoleTextAttribute(hConsole, bg|2|inv);
					break;
				case '3': // YELLOW
					SetConsoleTextAttribute(hConsole, bg|2|4|inv);
					break;
				case '4': // BLUE
					SetConsoleTextAttribute(hConsole, bg|1|inv);
					break;
				case '5': // MAGENTA
					SetConsoleTextAttribute(hConsole, bg|1|4|inv);
					break;
				case '6': // TURQOISE
					SetConsoleTextAttribute(hConsole, bg|1|2|8|inv);
					break;
				case '7': // WHITE
					SetConsoleTextAttribute(hConsole, bg|1|2|4|inv);
					break;
				case '8': // GRAY
					SetConsoleTextAttribute(hConsole, bg|8|inv);
					break;
				case '9': // ???
					break;
				}
				ptr = ptr + 1;
				str = ptr + 2;
				esc = 0;
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
			}
		} 
		len++;
	}
	write (1, str, ptr-str);
	return len;
}
#endif
