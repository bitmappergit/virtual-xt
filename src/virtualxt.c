// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "vxt.h"
#include "kb.h"
#include "version.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>
#include <fcntl.h>

#ifdef _WIN32
	#include <windows.h>
	#include <io.h>

	#ifdef __MINGW32__
		#include <SDL2/SDL.h>

		// This is an issue with the MinGW. /aj
		#ifdef main
			#undef main
		#endif
	#else
		#include <SDL.h>
	#endif
#else
	#include <unistd.h>
	#include <SDL2/SDL.h>
#endif

// Missing on some systems.
#ifndef O_BINARY
	#define O_BINARY 0
#endif
#ifndef O_NOINHERIT
	#define O_NOINHERIT 0
#endif

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

char title_buffer[64] = {0};
SDL_Window *sdl_window = 0;
SDL_Surface *sdl_surface = 0;
SDL_Texture *sdl_texture = 0;
SDL_Renderer *sdl_renderer = 0;
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 1, 0, 128};

static void replace_floppy()
{
	int f = -1;
	char buf[512] = {0};

	#ifdef _WIN32
		OPENFILENAME ofn;        
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize     = sizeof(ofn);
		ofn.lpstrFilter     = "Floppy Image Files\0*.img\0\0";    
		ofn.lpstrFile       = buf;
		ofn.nMaxFile        = sizeof(buf);
		ofn.lpstrTitle      = "Select Floppy Image";
		ofn.Flags           = OFN_NONETWORKBUTTON|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;

		if (!GetOpenFileName(&ofn)) return;
	#else
		// Check if zenity is installed.
		if (system("which zenity > /dev/null 2>&1")) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing dependency", "Please install 'zenity', available at https://wiki.gnome.org/Projects/Zenity", NULL);
			exit(-1);
		}

		FILE *fp = popen("zenity --file-selection --title=\"Select Floppy Image\" --file-filter=*.img", "r");
		if (fgets(buf, sizeof(buf), fp)) buf[strlen(buf)-1] = 0;
		pclose(fp);
	#endif

	if (*buf) {
		if ((f = open(buf, O_RDWR|O_BINARY|O_NOINHERIT)) == -1) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing dependency", "Please install 'zenity', available at https://wiki.gnome.org/Projects/Zenity", NULL);
			return;
		}
	} else {
		return;
	}

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
	SDL_SetWindowTitle(sdl_window, "VirtualXT");

	if (m == VXT_TEXT) {
		sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, x, y);	
		sdl_surface = SDL_CreateRGBSurface(0, x, y, 32, 0, 0, 0, 0);
	} else {
		sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, x, y);	
		sdl_surface = SDL_CreateRGBSurface(0, x, y, 8, 0xE0, 0x1C, 0x3, 0x0);
	}
}

static byte *video_buffer(void *ud)
{
	SDL_RenderClear(sdl_renderer);
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

static void blit_char(SDL_Surface *dst, byte *font, byte ch, byte attrib, int x, int y) {
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

static void textmode(unsigned char *mem, byte *font, byte cursor, byte cx, byte cy) {
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

static vxt_key_t sdl_getkey(void *ud)
{
	vxt_key_t key = {.scancode = VXT_KEY_INVALID, .ascii = 0};
	if (sdl_window)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
            if (ev.type == SDL_QUIT) exit(0);
			if (ev.type != SDL_KEYDOWN && ev.type != SDL_KEYUP) continue;
            key.scancode = ev.type == SDL_KEYDOWN ? 0 : VXT_MASK_KEY_UP;

            switch (ev.key.keysym.sym)
            {
                case SDLK_ESCAPE: key.ascii = 0x1B; key.scancode |= VXT_KEY_ESCAPE; return key;
                case SDLK_RETURN: key.ascii = '\r'; key.scancode |= VXT_KEY_ENTER; return key;
                case SDLK_BACKSPACE: key.ascii = '\b'; key.scancode |= VXT_KEY_BACKSPACE; return key;
                case SDLK_TAB: key.ascii = '\t'; key.scancode |= VXT_KEY_TAB; return key;
                case SDLK_UP: case SDLK_KP_8: key.scancode |= VXT_KEY_KP_UP_8; return key;
                case SDLK_DOWN: case SDLK_KP_2: key.scancode |= VXT_KEY_KP_DOWN_2; return key;
                case SDLK_LEFT: case SDLK_KP_4: key.scancode |= VXT_KEY_KP_LEFT_4; return key;
                case SDLK_RIGHT: case SDLK_KP_6: key.scancode |= VXT_KEY_KP_RIGHT_6; return key;
                case SDLK_LCTRL: case SDLK_RCTRL: key.scancode |= VXT_KEY_CONTROL; return key;
                case SDLK_LALT: key.scancode |= VXT_KEY_ALT; return key;
                case SDLK_NUMLOCKCLEAR: key.scancode |= VXT_KEY_NUMLOCK; return key;
                case SDLK_SCROLLLOCK: key.scancode |= VXT_KEY_SCROLLOCK; return key;
                case SDLK_HOME: case SDLK_KP_7: key.scancode |= VXT_KEY_KP_HOME_7; return key;
                case SDLK_PAGEUP: case SDLK_KP_9: key.scancode |= VXT_KEY_KP_PAGEUP_9; return key;
                case SDLK_KP_5: key.scancode |= VXT_KEY_KP_5; return key;
                case SDLK_END: case SDLK_KP_1: key.scancode |= VXT_KEY_KP_END_1; return key;
                case SDLK_PAGEDOWN: case SDLK_KP_3: key.scancode |= VXT_KEY_KP_PAGEDOWN_3; return key;
                case SDLK_INSERT: case SDLK_KP_0: key.scancode |= VXT_KEY_KP_INSERT; return key;
				case SDLK_LSHIFT: key.scancode |= VXT_KEY_LSHIFT; return key;
                case SDLK_RSHIFT: key.scancode |= VXT_KEY_RSHIFT; return key;
                case SDLK_PRINTSCREEN: key.scancode |= VXT_KEY_PRINT; return key;
                case SDLK_F1: key.scancode |= VXT_KEY_F1; return key;
                case SDLK_F2: key.scancode |= VXT_KEY_F2; return key;
                case SDLK_F3: key.scancode |= VXT_KEY_F3; return key;
                case SDLK_F4: key.scancode |= VXT_KEY_F4; return key;
                case SDLK_F5: key.scancode |= VXT_KEY_F5; return key;
                case SDLK_F6: key.scancode |= VXT_KEY_F6; return key;
                case SDLK_F7: key.scancode |= VXT_KEY_F7; return key;
                case SDLK_F8: key.scancode |= VXT_KEY_F8; return key;
                case SDLK_F9: key.scancode |= VXT_KEY_F9; return key;
                case SDLK_F10: key.scancode |= VXT_KEY_F10; return key;
                default: if (ev.key.keysym.sym >= 0x20 && ev.key.keysym.sym <= 0x7F) {
                    SDL_Keymod mod = ev.key.keysym.mod;
                    if (mod & KMOD_RALT && ev.type == SDL_KEYDOWN)
                    {
                        switch (ev.key.keysym.sym) {
							case 'q':
								exit(0);
							case 'a':
								replace_floppy();
								continue;
							case 'f':
								SDL_SetWindowFullscreen(sdl_window, SDL_GetWindowFlags(sdl_window) & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
								continue;
						}
                    }

                    char ch = (char)ev.key.keysym.sym;
                    key.scancode |= ascii2scan[ch - 0x20];
                    key.ascii = ch;
                    return key;
                }
            }
		}
	}
	return key;
}

static void open_manual()
{
	#if defined(_WIN32)
		system("cmd /c manual\\index.html");
	#elif defined(__APPLE__) && defined(__MACH__)
		system("open manual\\index.html");
	#else
		system("xdg-open manual\\index.html");
	#endif
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
	#if defined(_WIN32) && defined(NDEBUG)
		ShowWindow(GetConsoleWindow(), SW_HIDE);
	#endif

	int hdboot_arg = 0, noaudio_arg = 0;
	double mips_arg = 0.0;
	const char *fd_arg = 0, *hd_arg = 0, *bios_arg = 0;

	while (--argc && ++argv) {
		if (!strcmp(*argv, "-h")) { print_help(); return 0; }
		if (!strcmp(*argv, "-m")) { open_manual(); return 0; }
		if (!strcmp(*argv, "-v")) { printf(VERSION_STRING "\n"); return 0; }
		if (!strcmp(*argv, "-a")) { fd_arg = argc-- ? *(++argv) : fd_arg; continue; }
		if (!strcmp(*argv, "-c")) { hd_arg = argc-- ? *(++argv) : hd_arg; continue; }
		if (!strcmp(*argv, "--mips")) { mips_arg = argc-- ? atof(*(++argv)) : mips_arg; continue; }
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
		int f = open(fd_arg, O_RDWR|O_BINARY|O_NOINHERIT);
		if (f == -1) { printf("Can't open FD image: %s\n", fd_arg); return -1; }
		fd.userdata = (void*)(intptr_t)f,
		vxt_replace_floppy(e, &fd);
	}

	if (hd_arg)
	{
		int f = open(hd_arg, O_RDWR|O_BINARY|O_NOINHERIT);
		if (f == -1) { printf("Can't open HD image: %s\n", hd_arg); return -1; }
		hd.userdata = (void*)(intptr_t)f,
		hd.boot = hdboot_arg;
		vxt_set_harddrive(e, &hd);
	}

	if (bios_arg)
	{
		static char bios_buff[0xFFFF];
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

		vxt_set_audio_control(e, (vxt_pause_audio_t)SDL_PauseAudio, sdl_audio.silence);
	}

	SDL_Init(SDL_INIT_TIMER);
	atexit(quit_sdl);

	if (!fd_arg && !hd_arg)
		replace_floppy();

	const double it = 1000.0 / (mips_arg * 1000.0);
	const Uint64 freq = SDL_GetPerformanceFrequency();
	Uint64 last = SDL_GetPerformanceCounter();

	for (int num_inst = 0;; num_inst++) {
		Uint64 start = SDL_GetPerformanceCounter();
		if ((start - last) / freq >= 1) {
			sprintf(title_buffer, "VirtualXT @ %.2f MIPS", (double)num_inst / 1000000.0);
			SDL_SetWindowTitle(sdl_window, title_buffer);
			last = start;
			num_inst = 0;
		}

		if (!vxt_step(e))
			break;

		while (mips_arg) {
			double t = (double)((SDL_GetPerformanceCounter() - start) * 1000000) / freq;
			if (t >= it)
				break;
		}
	}
}
