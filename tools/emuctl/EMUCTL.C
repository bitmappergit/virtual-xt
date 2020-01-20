/* Build with TurboC 2.01 and TASM. */

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARAM(p) (strcmp(*argv, "/"#p) == 0)

/* This function never returns. */
static void quitemu() {
    asm mov al, 0
    asm db 0x0f, 0x00
}

static void screenon() {
    asm mov al, 2
    asm db 0x0f, 0x00
}

static void dsphelp() {
    printf("Lets you control the VirtualXT emulator from DOS.\n\n");
	printf("EMUCTL [/Q]\n\n");
	printf("\t/Q\tShutdown the emulator.\n");
	printf("\t/S\tTurn on screen.\n");
}

int main(int argc, char *argv[]) {
	int quit = 0, screen = 0;

	if (argc < 2) { dsphelp(); return -1; }
	while (--argc && ++argv) {
		if (PARAM(?)) { dsphelp(); return 0; }
		if (PARAM(q) || PARAM(Q)) { quit = 1; continue; }
		if (PARAM(s) || PARAM(S)) { screen = 1; continue; }
		dsphelp(); return -1;
	}

	if (quit) quitemu();
	if (screen) screenon();
	return 0;
}
