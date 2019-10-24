// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "libvxt.h"

#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
	#include <conio.h>
	#include <windows.h>
	#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
		#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
	#endif
#endif

size_t ___read(void *ud, void* buf, size_t count) { return (size_t)read((int)(intptr_t)ud, buf, count); }
size_t ___write(void *ud, const void *buf, size_t count) { return (size_t)write((int)(intptr_t)ud, buf, count); }
size_t ___seek(void *ud, size_t offset, int whence) { return (size_t)lseek((int)(intptr_t)ud, offset, whence); }

int ___kbhit(void *ud) { return kbhit(); }
int ___getch(void *ud) { return getch(); }
void ___putchar(void *ud, int ch) { putchar(ch); }

int main(int argc, char *argv[])
{
	#ifdef _WIN32
		DWORD dwMode = 0;
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE) return GetLastError();
		if (!GetConsoleMode(hOut, &dwMode)) return GetLastError();
		if (!SetConsoleMode(hOut, dwMode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return GetLastError();
	#endif

	vxt_terminal_t term = {
		.userdata = 0,
		.kbhit = ___kbhit,
		.getch = ___getch,
		.putchar = ___putchar
	};

	vxt_drive_t fd = {
		.userdata = (void*)(intptr_t)open(argv[1], 32898),
		.boot = 1,
		.read = ___read,
		.write = ___write,
		.seek = ___seek
	};

	vxt_emulator_t *e = vxt_init(&term, &fd, 0, 0);
	while (vxt_step(e));

	#ifndef NO_GRAPHICS
		SDL_Quit();
	#endif
}

/*
// Keyboard driver for console. This may need changing for UNIX/non-UNIX platforms
#ifdef _WIN32
#define KEYBOARD_DRIVER  e->term->kbhit(e->term->userdata) && (e->mem[0x4A6] = e->term->getch(e->term->userdata), pc_interrupt(e, 7))
#else
#define KEYBOARD_DRIVER read(0, mem + 0x4A6, 1) && (e->int8_asap = (mem[0x4A6] == 0x1B), pc_interrupt(e, 7))
#endif

// Keyboard driver for SDL
#ifdef NO_GRAPHICS
#define SDL_KEYBOARD_DRIVER KEYBOARD_DRIVER
#else
//#define SDL_KEYBOARD_DRIVER sdl_screen ? SDL_PollEvent(&sdl_event) && (sdl_event.type == SDL_KEYDOWN || sdl_event.type == SDL_KEYUP) && (scratch_uint = sdl_event.key.keysym.unicode, scratch2_uint = sdl_event.key.keysym.mod, CAST(short)mem[0x4A6] = 0x400 + 0x800*!!(scratch2_uint & KMOD_ALT) + 0x1000*!!(scratch2_uint & KMOD_SHIFT) + 0x2000*!!(scratch2_uint & KMOD_CTRL) + 0x4000*(sdl_event.type == SDL_KEYUP) + ((!scratch_uint || scratch_uint > 0x7F) ? sdl_event.key.keysym.sym : scratch_uint), pc_interrupt(7)) : (KEYBOARD_DRIVER)

// This needs to be fixed. /aj
#define SDL_KEYBOARD_DRIVER sdl_screen ? SDL_PollEvent(&sdl_event) && (sdl_event.type == SDL_KEYDOWN || sdl_event.type == SDL_KEYUP) && (e->mem[0x4A6] = (unsigned char)tolower(*SDL_GetKeyName(sdl_event.key.keysym.sym)), pc_interrupt(e, 7)) : (KEYBOARD_DRIVER) 
*/