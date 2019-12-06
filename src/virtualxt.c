// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "vxt.h"
#include "kb.h"

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>
#include <assert.h>

#include <SDL2/SDL.h>

#ifdef _WIN32
	#include <windows.h>
	#include <io.h>

	// This is an issue with the MinGW. /aj
	#ifdef main
		#undef main
	#endif
#endif

#define VERSION_STRING "0.0.1"

vxt_emulator_t *e = 0;
vxt_drive_t fd = {0};

const int text_color[] = {
	0x000000,
	0x0000AA,
	0x00AA00,
	0x00AAAA,
	0xAA0000,
	0xAA00AA,
	0xAA5500,
	0xAAAAAA,
	0x555555,
	0x5555FF,
	0x55FF55,
	0x55FFFF,
	0xFF5555,
	0xFF55FF,
	0xFFFF55,
	0xFFFFFF
};

SDL_Surface *sdl_surface = 0;
SDL_Texture *sdl_texture = 0;
SDL_Renderer *sdl_renderer = 0;
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 1, 0, 128};

void replace_floppy()
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

static void close_window()
{
	SDL_FreeSurface(sdl_surface); sdl_surface = 0;
	SDL_DestroyTexture(sdl_texture); sdl_texture = 0;
	SDL_DestroyRenderer(sdl_renderer); sdl_renderer = 0;
	SDL_DestroyWindow(sdl_window); sdl_window = 0;
}

static void open_window(void *ud, vxt_mode_t m, int x, int y)
{
	if (sdl_window) {
		SDL_FreeSurface(sdl_surface); sdl_surface = 0;
		SDL_DestroyTexture(sdl_texture); sdl_texture = 0;
	} else {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
		SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");

		SDL_Init(SDL_INIT_VIDEO);
		SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_RESIZABLE|SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MOUSE_FOCUS, &sdl_window, &sdl_renderer);
	}

	int h = (int)((float)x / (4.f / 3.f));
	SDL_RenderSetLogicalSize(sdl_renderer, x, h);

	if (m == VXT_TEXT) {
		SDL_SetWindowTitle(sdl_window, "VirtualXT");
		sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, x, y);	
		sdl_surface = SDL_CreateRGBSurface(0, x, y, 32, 0, 0, 0, 0);
	} else {
		SDL_SetWindowTitle(sdl_window, m == VXT_CGA ? "VirtualXT (CGA)" : "VirtualXT (Hercules)");
		sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, x, y);	
		sdl_surface = SDL_CreateRGBSurface(0, x, y, 8, 0xE0, 0x1C, 0x3, 0x0);
	}
}

static unsigned char *video_buffer(void *ud)
{
	SDL_UpdateTexture(sdl_texture, 0, sdl_surface->pixels, sdl_surface->pitch);
	SDL_RenderCopy(sdl_renderer, sdl_texture, 0, 0);
	SDL_RenderPresent(sdl_renderer);
	return sdl_surface->pixels;
}

static size_t io_read(void *ud, void* buf, size_t count) { return (size_t)read((int)(intptr_t)ud, buf, count); }
static size_t io_write(void *ud, const void *buf, size_t count) { return (size_t)write((int)(intptr_t)ud, buf, count); }
static size_t io_seek(void *ud, size_t offset, int whence) { return (size_t)lseek((int)(intptr_t)ud, offset, whence); }

static struct tm *get_localtime(void *ud) { time((time_t*)ud); return localtime((time_t*)ud); }
static unsigned short get_millitm(void *ud) { struct timeb c; ftime(&c); return c.millitm; }

static void quit_sdl() { if (sdl_window) close_window(); SDL_Quit(); }
static void close_emulator() { if (e) vxt_close(e); }

static void blit_char(SDL_Surface *dst, unsigned char *font, unsigned char ch, unsigned char attrib, int x, int y) {
	assert(dst->format->BytesPerPixel == 4);

	unsigned *pixels = (unsigned*)dst->pixels;
	unsigned bgcolor = text_color[(attrib&0x70)>>4];
	unsigned fgcolor = ((attrib&0x80) && vxt_blink(e)) ? bgcolor : text_color[attrib&0xF];
	
	for (int i = 0; i < 8; i++) {
		unsigned char glyph_line = font[ch*8+i];

		for (int j = 0; j < 8; j++) {
			unsigned char mask = 0x80 >> j;
			unsigned fg = (glyph_line & mask ? fgcolor : bgcolor);
			pixels[dst->w * (y + i) + x + j] = fg | 0xFF000000;
		}
	}
}

static void textmode(unsigned char *mem, unsigned char *font, unsigned char cursor, unsigned char cx, unsigned char cy) {
	const int nchar = 80*25;
	for (int i = 0; i < nchar * 2; i+=2) {
		unsigned char ch = mem[i];
		int index = i / 2;
		blit_char(sdl_surface, font, ch, mem[i+1], (index % 80) * 8, (index / 80) * 8);
	}

	if (cursor && vxt_blink(e))
		blit_char(sdl_surface, font, '_', (mem[160*cy+cx*2+1] & 0x70) | 0xF, cx * 8, cy * 8);

	SDL_UpdateTexture(sdl_texture, 0, sdl_surface->pixels, sdl_surface->pitch);
	SDL_RenderCopy(sdl_renderer, sdl_texture, 0, 0);
	SDL_RenderPresent(sdl_renderer);
}

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

	time_t clock_buf;
	vxt_clock_t clock = {.userdata = &clock_buf, .localtime = get_localtime, .millitm = get_millitm};
	vxt_video_t video = {.userdata = 0, .getkey = sdl_getkey, .initialize = open_window, .backbuffer = video_buffer, .textmode = textmode};
	e = vxt_open(&video, &clock, VXT_INTERNAL_MEMORY);
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
		char bios_buff[0xFFFF];
		FILE *fp = fopen(bios_arg, "rb");
		if (!fp) { printf("Can't open BIOS image: %s\n", bios_arg); return -1; }
		vxt_load_bios(e, bios_buff, fread(bios_buff, 1, sizeof(bios_buff), fp));
		fclose(fp);
	}

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

	if (!fd_arg && !hd_arg)
		replace_floppy();

	while (vxt_step(e))
	{
		#ifndef NO_SDL
			SDL_PumpEvents();
		#endif
	}
}
