// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019-2020 Andreas T Jonsson (mail@andreasjonsson.se)
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
#include <sys/stat.h>
#include <fcntl.h>

#if defined(_WIN32) && !defined(__MINGW32__)
	#include <SDL.h>
#else
	#include <SDL2/SDL.h>
#endif

#if defined(_WIN32)
	#include <windows.h>
	#include <io.h>

	// This is an issue with the MinGW. /aj
	#if defined(__MINGW32__) && defined(main)
		#undef main
	#endif
#elif defined(__APPLE__) && defined(__MACH__)
	#include <nfd.h>
#else
	#include <unistd.h>
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
vxt_key_t auto_release = {0};
int command_key = 0;

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

const char *scale_filter = "0";
const char *video_driver = "opengl";

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

	#if defined(_WIN32)
		OPENFILENAME ofn;        
		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize     = sizeof(ofn);
		ofn.lpstrFilter     = "Floppy Image Files\0*.img\0\0";    
		ofn.lpstrFile       = buf;
		ofn.nMaxFile        = sizeof(buf);
		ofn.lpstrTitle      = "Select Floppy Image";
		ofn.Flags           = OFN_NONETWORKBUTTON|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;

		if (!GetOpenFileName(&ofn)) return;
	#elif defined(__APPLE__) && defined(__MACH__)
		nfdchar_t *path = NULL;
		if (NFD_OpenDialog("*.img", NULL, &path) != NFD_OKAY) return;
		strncpy(buf, path, sizeof(buf));
		free(path);
	#elif !defined(__EMSCRIPTEN__)
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

static int file_exist(const char *filename)
{
	struct stat buffer;
	return stat(filename, &buffer) == 0;
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
		SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scale_filter);
		if (video_driver) SDL_SetHint(SDL_HINT_RENDER_DRIVER, video_driver);

		SDL_Init(SDL_INIT_VIDEO);
		if (SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_RESIZABLE|SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MOUSE_FOCUS, &sdl_window, &sdl_renderer)) {
			printf("%s\n", SDL_GetError());
			exit(-1);
		}
	}

	int h = (int)((float)x / (4.f / 3.f));
	SDL_RenderSetLogicalSize(sdl_renderer, x, h);
	SDL_SetWindowTitle(sdl_window, "VirtualXT");
	SDL_StartTextInput();

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

static void blit_char(SDL_Surface *dst, byte *font, byte ch, byte attrib, int x, int y)
{
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

static void textmode(unsigned char *mem, byte *font, byte cursor, byte cx, byte cy)
{
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

byte joystick_buttons(void *ud)
{
	SDL_GameController *controller = (SDL_GameController*)ud;
	return (byte)((SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) << 1) | SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A));
}

void joystick_axis(void *ud, word *x, word *y)
{
	SDL_GameController *controller = (SDL_GameController*)ud;
	*x = (word)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
	*y = (word)SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
}

static void open_manual()
{
	char buffer[512] = {0};

	#if defined(_WIN32)
		strcpy(buffer, "cmd /c ");
		if (file_exist("manual\\index.html"))
			system(strcat(buffer, "manual\\index.html"));
		else if (file_exist("doc\\manual\\index.html"))
			system(strcat(buffer, "doc\\manual\\index.html"));
		return;
	#elif defined(__APPLE__) && defined(__MACH__)
		strcpy(buffer, "open ");
	#else
		strcpy(buffer, "xdg-open ");
	#endif

	if (file_exist("manual/index.html"))
		system(strcat(buffer, "manual/index.html"));
	else if (file_exist("doc/manual/index.html"))
		system(strcat(buffer, "doc/manual/index.html"));
	else if (file_exist("../Resources/manual/index.html"))
		system(strcat(buffer, "../Resources/manual/index.html"));
	else if (file_exist("../share/virtualxt/manual/index.html"))
		system(strcat(buffer, "../share/virtualxt/manual/index.html"));
	else if (file_exist("/usr/local/share/virtualxt/manual/index.html"))
		system(strcat(buffer, "/usr/local/share/virtualxt/manual/index.html"));
	else
		printf("Could not find the manual!\n");
}

static vxt_key_t sdl_getkey(void *ud)
{
	vxt_key_t key = {.scancode = VXT_KEY_INVALID, .ascii = 0};
	if (auto_release.scancode != VXT_KEY_INVALID)
	{
		key = auto_release;
		auto_release.scancode = VXT_KEY_INVALID; auto_release.ascii = 0;
		return key;
	}
	
	if (sdl_window)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			key.ascii = 0;
			key.scancode = VXT_KEY_INVALID;

            if (ev.type == SDL_QUIT)
				exit(0);

			if (ev.type == SDL_TEXTINPUT)
			{
				char ch = *ev.text.text;
				key.scancode = ascii2scan[ch - 0x20];
				key.ascii = *ev.text.text;

				auto_release = key;
				auto_release.scancode |= VXT_MASK_KEY_UP;

				// Reset any buffers.
				// Not sure we should do this. /aj
				SDL_StopTextInput();
				SDL_StartTextInput();
				return key;
			}

			if (ev.type == SDL_KEYDOWN) key.scancode = VXT_KEY_INVALID;
			else if (ev.type == SDL_KEYUP) key.scancode = VXT_MASK_KEY_UP;
			else continue;

			SDL_Keycode sym = ev.key.keysym.sym;
			if ((ev.key.keysym.mod & KMOD_NUM) == 0) switch (sym)
            {
                case SDLK_KP_0: key.scancode |= VXT_KEY_KP_INSERT_0; return key;
				case SDLK_KP_1: key.scancode |= VXT_KEY_KP_END_1; return key;
				case SDLK_KP_2: key.scancode |= VXT_KEY_KP_DOWN_2; return key;
				case SDLK_KP_3: key.scancode |= VXT_KEY_KP_PAGEDOWN_3; return key;
				case SDLK_KP_4: key.scancode |= VXT_KEY_KP_LEFT_4; return key;
				case SDLK_KP_6: key.scancode |= VXT_KEY_KP_RIGHT_6; return key;
				case SDLK_KP_7: key.scancode |= VXT_KEY_KP_HOME_7; return key;
				case SDLK_KP_8: key.scancode |= VXT_KEY_KP_UP_8; return key;
                case SDLK_KP_9: key.scancode |= VXT_KEY_KP_PAGEUP_9; return key;
			}

            switch (sym)
            {
                case SDLK_ESCAPE: key.ascii = 0x1B; key.scancode |= VXT_KEY_ESCAPE; return key;
                case SDLK_RETURN: key.ascii = '\r'; key.scancode |= VXT_KEY_ENTER; return key;
                case SDLK_BACKSPACE: key.ascii = '\b'; key.scancode |= VXT_KEY_BACKSPACE; return key;
                case SDLK_TAB: key.ascii = '\t'; key.scancode |= VXT_KEY_TAB; return key;
                case SDLK_LCTRL: case SDLK_RCTRL: key.scancode |= VXT_KEY_CONTROL; return key;
                case SDLK_LALT: key.scancode |= VXT_KEY_ALT; return key;
                case SDLK_NUMLOCKCLEAR: key.scancode |= VXT_KEY_NUMLOCK; return key;
                case SDLK_SCROLLLOCK: key.scancode |= VXT_KEY_SCROLLOCK; return key;
				case SDLK_LSHIFT: key.scancode |= VXT_KEY_LSHIFT; return key;
                case SDLK_RSHIFT: key.scancode |= VXT_KEY_RSHIFT; return key;
                case SDLK_PRINTSCREEN: key.scancode |= VXT_KEY_PRINT; return key;

				case SDLK_DELETE: key.scancode |= VXT_KEY_KP_DELETE_PERIOD; return key;
                case SDLK_INSERT: key.scancode |= VXT_KEY_KP_INSERT_0; return key;
				case SDLK_END: key.scancode |= VXT_KEY_KP_END_1; return key;
				case SDLK_DOWN: key.scancode |= VXT_KEY_KP_DOWN_2; return key;
				case SDLK_PAGEDOWN: key.scancode |= VXT_KEY_KP_PAGEDOWN_3; return key;
				case SDLK_LEFT: key.scancode |= VXT_KEY_KP_LEFT_4; return key;
				case SDLK_RIGHT: key.scancode |= VXT_KEY_KP_RIGHT_6; return key;
				case SDLK_HOME: key.scancode |= VXT_KEY_KP_HOME_7; return key;
				case SDLK_UP: key.scancode |= VXT_KEY_KP_UP_8; return key;
                case SDLK_PAGEUP: key.scancode |= VXT_KEY_KP_PAGEUP_9; return key;

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

				case SDLK_F11:
				case SDLK_F12:
				{
					if (ev.type == SDL_KEYDOWN) {
						if (!ev.key.repeat) {
							command_key = 1;
							SDL_StopTextInput();
						}
					} else {
						command_key = 0;
						SDL_StartTextInput();
					}
					continue;
				}

				default:
					if (command_key && ev.type == SDL_KEYDOWN && !ev.key.repeat) switch (sym)
					{
						case 'q': exit(0);
						case 'a': replace_floppy(); continue;
						case 'f': SDL_SetWindowFullscreen(sdl_window, SDL_GetWindowFlags(sdl_window) & (SDL_WINDOW_FULLSCREEN|SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); continue;
						case 'm': open_manual(); continue;
					}
			}
		}
	}
	return key;
}

static void print_help()
{
	printf("VirtualXT - IBM PC/XT Emulator\n");
	printf("By Andreas T Jonsson\n\n");
	printf("Version: " VERSION_STRING "\n\n");
	printf("See manual for option. (-m)\n");
}

int main(int argc, char *argv[])
{
	#if defined(_WIN32) && defined(NDEBUG)
		ShowWindow(GetConsoleWindow(), SW_HIDE);
	#endif

	int hdboot_arg = 0, noaudio_arg = 0, joystick_arg = 0;
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
		if (!strcmp(*argv, "--joystick")) { joystick_arg = 1; continue; }
		if (!strcmp(*argv, "--bios")) { bios_arg = argc-- ? *(++argv) : bios_arg; continue; }
		if (!strcmp(*argv, "--filter")) { scale_filter = argc-- ? *(++argv) : scale_filter; continue; }
		if (!strcmp(*argv, "--driver")) { video_driver = argc-- ? *(++argv) : video_driver; continue; }
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

	#ifdef __EMSCRIPTEN__
		fd_arg = "boot.img";
	#endif

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

	vxt_joystick_t joystick = {.userdata = 0, .buttons = joystick_buttons, .axis = joystick_axis};
	if (joystick_arg)
	{
		SDL_Init(SDL_INIT_JOYSTICK);

		SDL_Joystick *sdl_joystick = 0;
		for (int i = 0; i < SDL_NumJoysticks(); i++) {
			if ((sdl_joystick = SDL_JoystickOpen(i))) {
				printf("Joystick found: %s\n", SDL_JoystickName(sdl_joystick));
				break;
			}
		}

		if (sdl_joystick && SDL_JoystickGetAttached(sdl_joystick)) {
			printf("Joystick initialized!\n");
			joystick.userdata = sdl_joystick;
			vxt_set_joystick(e, &joystick);
			SDL_JoystickEventState(SDL_ENABLE);
		}
	}

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
			return 0;

		while (mips_arg) {
			double t = (double)((SDL_GetPerformanceCounter() - start) * 1000000) / freq;
			if (t >= it)
				break;
		}
	}
}
