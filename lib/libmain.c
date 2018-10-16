// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, uvpd, and uvpt.

#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	///////////////////////MAGENDANZ/////////////////////////
	uint32_t call = SYS_getenvid;
	uint32_t envid;
	asm(	"movl %1, %%eax\n\t"
		"int $0x30\n\t"
		"movl %%eax, %0\n\t"
		: "=r"(envid)
		: "r"(call)
		: "eax");
	thisenv = &(envs[ENVX(envid)]);
	///////////////////////////////////////////////////////

	// save the name of the program so that panic() can use it
	if (argc > 0)
		binaryname = argv[0];

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	exit();
}

