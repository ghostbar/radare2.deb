#include "r_asm.h"

static void r_asm_list(RAsm *a) {
	RAsmPlugin *h;
	RListIter *iter;
	r_list_foreach (a->plugins, iter, h)
		printf ("asm %s\t %s\n", h->name, h->desc);
}

int main(int argc, char **argv) {
	struct r_asm_t *a;
	const char *arg;
	int num, i = 0;
	if (argc<2) {
		printf("Usage: fastcall [nargs]\n");
		return 1;
	}
	num = atoi(argv[1]);
	a = r_asm_new();

	printf("Supported plugins:\n");
	r_asm_list(a);
	r_asm_use(a, "x86.nasm");

	printf("Fastcall args for %d\n", atoi(argv[1]));

	printf("Using plugin: %s\n", a->cur->name);
	do {	arg = r_asm_fastcall(a, i++, num);
		if (arg) printf("%s\n", arg);
	} while(arg);
	return 0;
}
