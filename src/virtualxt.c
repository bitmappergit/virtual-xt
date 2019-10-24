// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "libvxt.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

#ifdef _WIN32
	#include <windows.h>
	#include <conio.h>
	#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
		#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
	#endif
#endif

#ifndef NO_SDL

#include <ctype.h>
#include <SDL2/SDL.h>

// This is an issue with the MinGW.
#ifdef _WIN32
	#ifdef main
		#undef main
	#endif
#endif

SDL_Window *sdl_window = 0;
SDL_Surface *sdl_surface = 0;
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 1, 0, 128};

static void *open_window(void *ud, vxt_mode_t m, int x, int y)
{
	SDL_Init(SDL_INIT_VIDEO);
	sdl_window = SDL_CreateWindow(m == VXT_CGA ? "VirtualXT (CGA)" : "VirtualXT (Hercules)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, x, y, 0);
	sdl_surface = SDL_CreateRGBSurface(0, x, y, 8, 0xE0, 0x1C, 0x3, 0x0);

	//SDL_EnableUNICODE(1);
	//SDL_EnableKeyRepeat(500, 30);
}

static void close_window(void *ud, void *w)
{
	SDL_FreeSurface(sdl_surface), sdl_surface = 0;
	SDL_DestroyWindow(sdl_window), sdl_window = 0;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static unsigned char *video_buffer(void *ud, void *w) {
	SDL_BlitSurface(sdl_surface, 0, SDL_GetWindowSurface(sdl_window), 0);
	SDL_UpdateWindowSurface(sdl_window);
	return sdl_surface->pixels;
}

#endif

static size_t ___read(void *ud, void* buf, size_t count) { return (size_t)read((int)(intptr_t)ud, buf, count); }
static size_t ___write(void *ud, const void *buf, size_t count) { return (size_t)write((int)(intptr_t)ud, buf, count); }
static size_t ___seek(void *ud, size_t offset, int whence) { return (size_t)lseek((int)(intptr_t)ud, offset, whence); }

static int ___kbhit(void *ud) { return kbhit(); }
static int ___getch(void *ud) { return getch(); }
static void ___putchar(void *ud, int ch) { putchar(ch); }

static struct tm *___localtime(void *ud) { time((time_t*)ud); return localtime((time_t*)ud); }
static unsigned short ___millitm(void *ud) { struct timeb c; ftime(&c); return c.millitm; }

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

	time_t clock_buf;
	vxt_clock_t clock = {
		.userdata = &clock_buf,
		.localtime = ___localtime,
		.millitm = ___millitm
	};

	vxt_emulator_t *e = vxt_init(&term, &clock, &fd, VXT_DEFAULT_ALLOCATOR);

	#ifndef NO_SDL
		SDL_Init(SDL_INIT_AUDIO);
		sdl_audio.callback = (SDL_AudioCallback)vxt_audio_callback;
		sdl_audio.userdata = (void*)e;
		#ifdef _WIN32
			sdl_audio.samples = 512;
		#endif
		SDL_OpenAudio(&sdl_audio, 0);

		vxt_set_audio_control(e, (vxt_pause_audio_t)SDL_PauseAudio);
		vxt_set_audio_silence(e, sdl_audio.silence);

		vxt_video_t video = {
			.userdata = 0,
			.open = open_window,
			.close = close_window,
			.buffer = video_buffer
		};
		vxt_set_video(e, &video);
	#endif

	while (vxt_step(e))
	{
		#ifndef NO_SDL
			SDL_PumpEvents();
		#endif
	}

	#ifndef NO_SDL
		if (sdl_window || sdl_surface) close_window(0, 0);
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