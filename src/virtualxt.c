// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "vxt.h"
#include "toscan.h"

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>
#include <assert.h>

#define VERSION_STRING "0.0.1"

#ifdef _WIN32
	#include <windows.h>
	#include <WinUser.h>
	#include <io.h>
	#include <conio.h>
	
	#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
		#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
	#endif

	static vxt_key_t term_getkey(void *ud) {
		INPUT_RECORD event;
		DWORD count = 0;
		HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);

		while (1)
		{
			vxt_key_t key = {.scancode = VXT_KEY_INVALID, .ascii = 0x20};
			GetNumberOfConsoleInputEvents(hstdin, &count);
			if (!count) return key;

			ReadConsoleInput(hstdin, &event, 1, &count);
			if (count > 0 && event.EventType == KEY_EVENT)
			{
				KEY_EVENT_RECORD *ke = &event.Event.KeyEvent;
				key.scancode = ke->bKeyDown ? 0 : VXT_MASK_KEY_UP;

				switch (ke->wVirtualKeyCode)
				{
					case VK_ESCAPE: key.scancode |= VXT_KEY_ESCAPE; key.ascii = 0x1B; return key;
					case VK_BACK: key.scancode |= VXT_KEY_BACKSPACE; key.ascii = 0x8; return key;
					case VK_TAB: key.scancode |= VXT_KEY_TAB; key.ascii = '\t'; return key;
					case VK_RETURN: key.scancode |= VXT_KEY_ENTER; key.ascii = '\r'; return key;
					case VK_CONTROL: key.scancode |= VXT_KEY_CONTROL; return key;
					case VK_SHIFT: case VK_LSHIFT: key.scancode |= VXT_KEY_LSHIFT; return key;
					case VK_RSHIFT: key.scancode |= VXT_KEY_RSHIFT; return key;
					case VK_PRINT: key.scancode |= VXT_KEY_PRINT; return key;
					case VK_MENU: key.scancode |= VXT_KEY_ALT; return key;
					case VK_CAPITAL: key.scancode |= VXT_KEY_CAPSLOCK; return key;
					case VK_NUMLOCK: key.scancode |= VXT_KEY_NUMLOCK; return key;
					case VK_SCROLL: key.scancode |= VXT_KEY_SCROLLOCK; return key;
					case VK_HOME: case VK_NUMPAD7: key.scancode |= VXT_KEY_KP_HOME_7; return key;
					case VK_UP: case VK_NUMPAD8: key.scancode |= VXT_KEY_KP_UP_8; return key;
					case VK_NUMPAD9: key.scancode |= VXT_KEY_KP_PAGEUP_9; return key;
					//case VK_NUMPAD9: key.scancode |= VXT_KEY_KP_MINUS; return key;
					case VK_LEFT: case VK_NUMPAD4: key.scancode |= VXT_KEY_KP_LEFT_4; return key;
					case VK_NUMPAD5: key.scancode |= VXT_KEY_KP_5; return key;
					case VK_RIGHT: case VK_NUMPAD6: key.scancode |= VXT_KEY_KP_RIGHT_6; return key;
					//case VK_NUMPAD9: key.scancode |= VXT_KEY_KP_PLUS; return key;
					case VK_END: case VK_NUMPAD1: key.scancode |= VXT_KEY_KP_END_1; return key;
					case VK_DOWN: case VK_NUMPAD2: key.scancode |= VXT_KEY_KP_DOWN_2; return key;
					case VK_NUMPAD3: key.scancode |= VXT_KEY_KP_PAGEDOWN_3; return key;
					case VK_INSERT: key.scancode |= VXT_KEY_KP_INSERT; return key;
					case VK_F1: key.scancode |= VXT_KEY_F1; return key;
					case VK_F2: key.scancode |= VXT_KEY_F2; return key;
					case VK_F3: key.scancode |= VXT_KEY_F3; return key;
					case VK_F4: key.scancode |= VXT_KEY_F4; return key;
					case VK_F5: key.scancode |= VXT_KEY_F5; return key;
					case VK_F6: key.scancode |= VXT_KEY_F6; return key;
					case VK_F7: key.scancode |= VXT_KEY_F7; return key;
					case VK_F8: key.scancode |= VXT_KEY_F8; return key;
					case VK_F9: key.scancode |= VXT_KEY_F9; return key;
					case VK_F10: key.scancode |= VXT_KEY_F10; return key;
					default:
					{
						char ch = ke->uChar.AsciiChar;
						if (ch >= 0x20 && ch <= 0x7F) {
							key.scancode |= ascii2scan[ch - 0x20];
							key.ascii = ch;
							return key;
						}
					}
				}
			}
		}
	}

#else
	#include <fcntl.h>
	
	unsigned char unix_key = 0;
	static int ___kbhit(void *ud) { return read(0, &unix_key, 1); }
	static int ___getch(void *ud) { return (int)unix_key; }
#endif

static void term_putchar(void *ud, unsigned char ch) { putchar(ch); }

vxt_terminal_t term = {
	.userdata = 0,
	.getkey = term_getkey,
	.putchar = term_putchar
};

char bios_buff[0xFFFF];

#ifndef NO_SDL

#include <SDL2/SDL.h>

// This is an issue with the MinGW. /aj
#ifdef _WIN32
	#ifdef main
		#undef main
	#endif
#endif

SDL_Window *sdl_window = 0;
SDL_Surface *sdl_surface = 0;
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 1, 0, 128};

static vxt_key_t sdl_getkey(void *ud)
{
	vxt_key_t key = {.scancode = VXT_KEY_INVALID, .ascii = 0x20};
	if (sdl_window)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			if (ev.type != SDL_KEYDOWN && ev.type != SDL_KEYUP)
				continue;	 

			key.scancode = ev.type == SDL_KEYDOWN ? 0 : VXT_MASK_KEY_UP;
			key.scancode |= sdl_to_scancode(ev.key.keysym.sym);
			key.ascii = sdl_to_ascii(ev.key.keysym.sym);
			return key;
		}
	}
	return key;
}

static void *open_window(void *ud, vxt_mode_t m, int x, int y)
{
	SDL_Init(SDL_INIT_VIDEO);
	sdl_window = SDL_CreateWindow(m == VXT_CGA ? "VirtualXT (CGA)" : "VirtualXT (Hercules)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, x, y, SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MOUSE_FOCUS);
	sdl_surface = SDL_CreateRGBSurface(0, x, y, 8, 0xE0, 0x1C, 0x3, 0x0);
	term.getkey = sdl_getkey;
}

static void close_window(void *ud, void *w)
{
	SDL_FreeSurface(sdl_surface), sdl_surface = 0;
	SDL_DestroyWindow(sdl_window), sdl_window = 0;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	term.getkey = term_getkey;
}

static unsigned char *video_buffer(void *ud, void *w)
{
	SDL_BlitSurface(sdl_surface, 0, SDL_GetWindowSurface(sdl_window), 0);
	SDL_UpdateWindowSurface(sdl_window);
	return sdl_surface->pixels;
}

static void quit_sdl() { if (sdl_window || sdl_surface) close_window(0, 0); SDL_Quit(); }

#endif

static void clear_screen() { printf("\033[2J\033[H"); }

static size_t ___read(void *ud, void* buf, size_t count) { return (size_t)read((int)(intptr_t)ud, buf, count); }
static size_t ___write(void *ud, const void *buf, size_t count) { return (size_t)write((int)(intptr_t)ud, buf, count); }
static size_t ___seek(void *ud, size_t offset, int whence) { return (size_t)lseek((int)(intptr_t)ud, offset, whence); }

static struct tm *___localtime(void *ud) { time((time_t*)ud); return localtime((time_t*)ud); }
static unsigned short ___millitm(void *ud) { struct timeb c; ftime(&c); return c.millitm; }

// Serial port COM1, 0x3F8 - 0x3FF
static int com1_filter(void *ud, unsigned short addr, int in) { return addr >= 0x3F8 && addr <= 0x3FF; }
static unsigned char com1_in(void *ud, unsigned short addr) { return 0; }
static void com1_out(void *ud,unsigned short addr, unsigned char data) {}

vxt_emulator_t *e = 0;
static void close_emulator() { if (e) vxt_close(e); }

static void print_help()
{
	printf("VirtualXT - IBM PC/XT Emulator\n");
	printf("By Andreas T Jonsson\n\n");
	printf("Version: " VERSION_STRING "\n\n");

	printf("TODO: Add help text.\n");
}

int main(int argc, char *argv[])
{
	int hdboot_arg = 0;
	const char *fd_arg = 0, *hd_arg = 0, *bios_arg = 0;
	while (--argc && ++argv) {
		if (!strcmp(*argv, "-h")) { print_help(); return 0; }
		if (!strcmp(*argv, "-v")) { printf(VERSION_STRING "\n"); return 0; }
		if (!strcmp(*argv, "-f")) { fd_arg = argc-- ? *(++argv) : fd_arg; continue; }
		if (!strcmp(*argv, "-d")) { hd_arg = argc-- ? *(++argv) : hd_arg; continue; }
		if (!strcmp(*argv, "--hdboot")) { hdboot_arg = 1; continue; }
		if (!strcmp(*argv, "--bios")) { bios_arg = argc-- ? *(++argv) : bios_arg; continue; }
		printf("Invalid parameter: %s\n", *argv); return -1;
	}

	#ifdef _WIN32
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE) return GetLastError();
		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode)) return GetLastError();
		if (!SetConsoleMode(hOut, dwMode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return GetLastError();
	#endif

	time_t clock_buf;
	vxt_clock_t clock = {
		.userdata = &clock_buf,
		.localtime = ___localtime,
		.millitm = ___millitm
	};

	e = vxt_open(&term, &clock, VXT_INTERNAL_MEMORY);
	atexit(close_emulator);

	vxt_port_map_t com1 = {
		.userdata = 0,
		.filter = com1_filter,
		.in = com1_in,
		.out = com1_out
	};
	vxt_set_port_map(e, &com1);

	vxt_drive_t fd = {
		.boot = !hdboot_arg,
		.read = ___read,
		.write = ___write,
		.seek = ___seek
	};

	if (fd_arg)
	{
		int f = open(fd_arg, 32898);
		if (f == -1) { printf("Can't open FD image: %s\n", fd_arg); return -1; }
		fd.userdata = (void*)(intptr_t)f,
		vxt_replace_floppy(e, &fd);
	}

	vxt_drive_t hd = fd;

	if (hd_arg)
	{
		int f = open(hd_arg, 32898);
		if (f == -1) { printf("Can't open HD image: %s\n", hd_arg); return -1; }
		hd.userdata = (void*)(intptr_t)f,
		hd.boot = hdboot_arg;
		vxt_set_harddrive(e, &hd);
	}

	if (bios_arg)
	{
		FILE *fp = fopen(bios_arg, "rb");
		if (!fp) { printf("Can't open BIOS image: %s\n", bios_arg); return -1; }
		vxt_load_bios(e, bios_buff, fread(bios_buff, 1, sizeof(bios_buff), fp));
		fclose(fp);
	}

	#ifndef NO_SDL
		SDL_Init(SDL_INIT_AUDIO);
		atexit(quit_sdl);

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

	clear_screen();
	atexit(clear_screen);

	while (vxt_step(e))
	{
		#ifndef NO_SDL
			SDL_PumpEvents();
		#endif
	}
}
