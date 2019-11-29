// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "vxt.h"

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>
#include <assert.h>

#ifdef _WIN32
	#include <windows.h>
	#include <io.h>
	#include <conio.h>
	
	#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
		#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
	#endif
#endif

#define VERSION_STRING "0.0.1"

vxt_emulator_t *e = 0;
vxt_drive_t fd = {0};

static void replace_floppy()
{
	int f = -1;
	char buf[256] = {0};

	#if defined(_WIN32) && !defined(NO_SDL)
		OPENFILENAME ofn;        
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize     = sizeof(ofn);
		ofn.lpstrFilter     = "Floppy Image Files\0*.img\0\0";    
		ofn.lpstrFile       = buf;
		ofn.nMaxFile        = sizeof(buf);
		ofn.lpstrTitle      = "Select Floppy Image";
		ofn.Flags           = OFN_NONETWORKBUTTON|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;

		if (!GetOpenFileName(&ofn)) return;
		if ((f = open(buf, 32898)) == -1) { MessageBox(0, "Could not open image file!", "Alert!", MB_OK|MB_ICONEXCLAMATION); return; }
		goto insert;
	#endif

	printf("\nFloppy image: ");
	gets(buf);

	if (strcmp(buf, ""))
	{		
		if ((f = open(buf, 32898)) == -1) { printf("Can't open floppy image!\n"); return; }
		printf("Image mounted!\n");
	}
	else
		printf("Floppy image unmounted!\n");

insert:

	if (fd.userdata) close((int)(intptr_t)fd.userdata);
	fd.userdata = (void*)(intptr_t)f;
	vxt_replace_floppy(e, &fd);
}

// Keyboard helpers.
#include "kb.h"

#ifndef _WIN32
	#error Not supported!

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

SDL_Surface *sdl_surface = 0;
SDL_Texture *sdl_texture = 0;
SDL_Renderer *sdl_renderer = 0;
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 1, 0, 128};

static void *open_window(void *ud, vxt_mode_t m, int x, int y)
{
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

	SDL_Init(SDL_INIT_VIDEO);
	int h = (int)((float)x / (4.f / 3.f));
	SDL_CreateWindowAndRenderer(x, h, SDL_WINDOW_RESIZABLE|SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MOUSE_FOCUS, &sdl_window, &sdl_renderer);
	SDL_SetWindowTitle(sdl_window, m == VXT_CGA ? "VirtualXT (CGA)" : "VirtualXT (Hercules)");
	SDL_RenderSetLogicalSize(sdl_renderer, x, h);

	sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, x, y);	
	sdl_surface = SDL_CreateRGBSurface(0, x, y, 8, 0xE0, 0x1C, 0x3, 0x0);
	term.getkey = sdl_getkey;
}

static void close_window(void *ud, void *w)
{
	SDL_FreeSurface(sdl_surface), sdl_surface = 0;
	SDL_DestroyTexture(sdl_texture), sdl_texture = 0;
	SDL_DestroyRenderer(sdl_renderer), sdl_renderer = 0;
	SDL_DestroyWindow(sdl_window), sdl_window = 0;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	term.getkey = term_getkey;
}

static unsigned char *video_buffer(void *ud, void *w)
{
	SDL_UpdateTexture(sdl_texture, 0, sdl_surface->pixels, sdl_surface->pitch);
	SDL_RenderCopy(sdl_renderer, sdl_texture, 0, 0);
	SDL_RenderPresent(sdl_renderer);
	return sdl_surface->pixels;
}

static void quit_sdl() { if (sdl_window || sdl_surface) close_window(0, 0); SDL_Quit(); }

#endif

static void clear_screen() { printf("\033[2J\033[H"); }

static size_t io_read(void *ud, void* buf, size_t count) { return (size_t)read((int)(intptr_t)ud, buf, count); }
static size_t io_write(void *ud, const void *buf, size_t count) { return (size_t)write((int)(intptr_t)ud, buf, count); }
static size_t io_seek(void *ud, size_t offset, int whence) { return (size_t)lseek((int)(intptr_t)ud, offset, whence); }

static struct tm *get_localtime(void *ud) { time((time_t*)ud); return localtime((time_t*)ud); }
static unsigned short get_millitm(void *ud) { struct timeb c; ftime(&c); return c.millitm; }

static void close_emulator() { if (e) vxt_close(e); }

static void print_help()
{
	printf("VirtualXT - IBM PC/XT Emulator\n");
	printf("By Andreas T Jonsson\n\n");
	printf("Version: " VERSION_STRING "\n\n");

	printf("TODO: Add help text. :)\n");
}

int main(int argc, char *argv[])
{
	int hdboot_arg = 0, noaudio_arg = 0;
	const char *fd_arg = 0, *hd_arg = 0, *bios_arg = 0;
	while (--argc && ++argv) {
		if (!strcmp(*argv, "-h")) { print_help(); return 0; }
		if (!strcmp(*argv, "-v")) { printf(VERSION_STRING "\n"); return 0; }
		if (!strcmp(*argv, "-f")) { fd_arg = argc-- ? *(++argv) : fd_arg; continue; }
		if (!strcmp(*argv, "-d")) { hd_arg = argc-- ? *(++argv) : hd_arg; continue; }
		if (!strcmp(*argv, "--hdboot")) { hdboot_arg = 1; continue; }
		if (!strcmp(*argv, "--noaudio")) { noaudio_arg = 1; continue; }
		if (!strcmp(*argv, "--bios")) { bios_arg = argc-- ? *(++argv) : bios_arg; continue; }
		printf("Invalid parameter: %s\n", *argv); return -1;
	}

	#ifdef _WIN32
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE) return GetLastError();
		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode)) return GetLastError();
		if (!SetConsoleMode(hOut, dwMode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return GetLastError();
	
		// Try to adjust the console size
	    COORD const size = { 80, 25 };
		SMALL_RECT const window = { 0, 0, size.X - 1, size.Y - 1 };
		SetConsoleWindowInfo(hOut, 1, &window);
		SetConsoleScreenBufferSize(hOut, size);
		SetConsoleWindowInfo(hOut, 1, &window); // Needs to be done again
	#endif

	time_t clock_buf;
	vxt_clock_t clock = {.userdata = &clock_buf, .localtime = get_localtime, .millitm = get_millitm};
	e = vxt_open(&term, &clock, VXT_INTERNAL_MEMORY);
	atexit(close_emulator);

	fd.boot = !hdboot_arg;
	fd.read = io_read;
	fd.write = io_write;
	fd.seek = io_seek;

	vxt_drive_t hd = fd;

	if (fd_arg)
	{
		int f = open(fd_arg, 32898);
		if (f == -1) { printf("Can't open FD image: %s\n", fd_arg); return -1; }
		fd.userdata = (void*)(intptr_t)f,
		vxt_replace_floppy(e, &fd);
	}

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
		if (!noaudio_arg)
		{
			SDL_Init(SDL_INIT_AUDIO);
			sdl_audio.callback = (SDL_AudioCallback)vxt_audio_callback;
			sdl_audio.userdata = (void*)e;
			#ifdef _WIN32
				sdl_audio.samples = 512;
			#endif
			SDL_OpenAudio(&sdl_audio, 0);

			vxt_set_audio_control(e, (vxt_pause_audio_t)SDL_PauseAudio);
			vxt_set_audio_silence(e, sdl_audio.silence);
		}
		atexit(quit_sdl);

		vxt_video_t video = {.userdata = 0, .open = open_window, .close = close_window, .buffer = video_buffer};
		vxt_set_video(e, &video);
	#endif

	clear_screen();
	atexit(clear_screen);

	if (!fd_arg && !hd_arg)
		replace_floppy();

	while (vxt_step(e))
	{
		#ifndef NO_SDL
			SDL_PumpEvents();
		#endif
	}
}
