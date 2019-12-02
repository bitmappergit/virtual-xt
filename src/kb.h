// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#ifndef _KB_H_
#define _KB_H_

#include <vxt.h>
#include <ctype.h>

const vxt_scancode_t ascii2scan[96] = {
    VXT_KEY_SPACE,
    VXT_KEY_EXCLAIM,
    VXT_KEY_QUOTE_DQUOTE,
    VXT_KEY_HASH,
    VXT_KEY_DOLLAR,
    VXT_KEY_PERCENT,
    VXT_KEY_7,
    VXT_KEY_QUOTE_DQUOTE,
    VXT_KEY_9,
    VXT_KEY_0,
    VXT_KEY_8,
    VXT_KEY_EQUAL_OR_PLUS,
    VXT_KEY_COMMA_LESS,
    VXT_KEY_MINUS_OR_UNDERSCORE,
    VXT_KEY_PERIOD_GREATER,
    VXT_KEY_SLASH_QUESTION,
    VXT_KEY_0,
    VXT_KEY_1,
    VXT_KEY_2,
    VXT_KEY_3,
    VXT_KEY_4,
    VXT_KEY_5,
    VXT_KEY_6,
    VXT_KEY_7,
    VXT_KEY_8,
    VXT_KEY_9,
    VXT_KEY_SEMICOLON_COLON,
    VXT_KEY_SEMICOLON_COLON,
    VXT_KEY_COMMA_LESS,
    VXT_KEY_EQUAL_OR_PLUS,
    VXT_KEY_PERIOD_GREATER,
    VXT_KEY_SLASH_QUESTION,
    VXT_KEY_2,
    VXT_KEY_A,
    VXT_KEY_B,
    VXT_KEY_C,
    VXT_KEY_D,
    VXT_KEY_E,
    VXT_KEY_F,
    VXT_KEY_G,
    VXT_KEY_H,
    VXT_KEY_I,
    VXT_KEY_J,
    VXT_KEY_K,
    VXT_KEY_L,
    VXT_KEY_M,
    VXT_KEY_N,
    VXT_KEY_O,
    VXT_KEY_P,
    VXT_KEY_Q,
    VXT_KEY_R,
    VXT_KEY_S,
    VXT_KEY_T,
    VXT_KEY_U,
    VXT_KEY_V,
    VXT_KEY_W,
    VXT_KEY_X,
    VXT_KEY_Y,
    VXT_KEY_Z,
    VXT_KEY_LBRACKET_LBRACE,
    VXT_KEY_BACKSLASH_VBAR,
    VXT_KEY_RBRACKET_RBRACE,
    VXT_KEY_6,
    VXT_KEY_MINUS_OR_UNDERSCORE,
    VXT_KEY_BACKQUOTE_TILDE,
    VXT_KEY_A,
    VXT_KEY_B,
    VXT_KEY_C,
    VXT_KEY_D,
    VXT_KEY_E,
    VXT_KEY_F,
    VXT_KEY_G,
    VXT_KEY_H,
    VXT_KEY_I,
    VXT_KEY_J,
    VXT_KEY_K,
    VXT_KEY_L,
    VXT_KEY_M,
    VXT_KEY_N,
    VXT_KEY_O,
    VXT_KEY_P,
    VXT_KEY_Q,
    VXT_KEY_R,
    VXT_KEY_S,
    VXT_KEY_T,
    VXT_KEY_U,
    VXT_KEY_V,
    VXT_KEY_W,
    VXT_KEY_X,
    VXT_KEY_Y,
    VXT_KEY_Z,
    VXT_KEY_LBRACKET_LBRACE,
    VXT_KEY_BACKSLASH_VBAR,
    VXT_KEY_RBRACKET_RBRACE,
    VXT_KEY_BACKQUOTE_TILDE,
    VXT_KEY_KP_DELETE
};

#ifdef _WIN32

#include <windows.h>
#include <WinUser.h>

static vxt_key_t term_getkey(void *ud)
{
    INPUT_RECORD event;
    DWORD count = 0;
    HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);

    while (1)
    {
        vxt_key_t key = {.scancode = VXT_KEY_INVALID, .ascii = 0};
        GetNumberOfConsoleInputEvents(hstdin, &count);
        if (!count) return key;

        ReadConsoleInput(hstdin, &event, 1, &count);
        if (count > 0 && event.EventType == KEY_EVENT)
        {
            KEY_EVENT_RECORD *ke = &event.Event.KeyEvent;
            key.scancode = ke->bKeyDown ? 0 : VXT_MASK_KEY_UP;

            switch (ke->wVirtualKeyCode)
            {
                case 'A': if (ke->dwControlKeyState&RIGHT_ALT_PRESSED) {
                    replace_floppy();
                    return key;
                }
                break;
                case VK_ESCAPE: key.scancode |= VXT_KEY_ESCAPE; key.ascii = 0x1B; return key;
                case VK_BACK: key.scancode |= VXT_KEY_BACKSPACE; key.ascii = 0x8; return key;
                case VK_TAB: key.scancode |= VXT_KEY_TAB; key.ascii = '\t'; return key;
                case VK_RETURN: key.scancode |= VXT_KEY_ENTER; key.ascii = '\r'; return key;
                case VK_CONTROL: key.scancode |= VXT_KEY_CONTROL; return key;
                case VK_MENU: key.scancode |= VXT_KEY_ALT; return key;
                case VK_CAPITAL: key.scancode |= VXT_KEY_CAPSLOCK; return key;
                case VK_NUMLOCK: key.scancode |= VXT_KEY_NUMLOCK; return key;
                case VK_SCROLL: key.scancode |= VXT_KEY_SCROLLOCK; return key;
                case VK_HOME: case VK_NUMPAD7: key.scancode |= VXT_KEY_KP_HOME_7; return key;
                case VK_UP: case VK_NUMPAD8: key.scancode |= VXT_KEY_KP_UP_8; return key;
                case VK_PRIOR: case VK_NUMPAD9: key.scancode |= VXT_KEY_KP_PAGEUP_9; return key;
                case VK_LEFT: case VK_NUMPAD4: key.scancode |= VXT_KEY_KP_LEFT_4; return key;
                case VK_NUMPAD5: key.scancode |= VXT_KEY_KP_5; return key;
                case VK_RIGHT: case VK_NUMPAD6: key.scancode |= VXT_KEY_KP_RIGHT_6; return key;
                case VK_END: case VK_NUMPAD1: key.scancode |= VXT_KEY_KP_END_1; return key;
                case VK_DOWN: case VK_NUMPAD2: key.scancode |= VXT_KEY_KP_DOWN_2; return key;
                case VK_NEXT: case VK_NUMPAD3: key.scancode |= VXT_KEY_KP_PAGEDOWN_3; return key;
                case VK_INSERT: case VK_NUMPAD0: key.scancode |= VXT_KEY_KP_INSERT; return key;
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
                case VK_SHIFT: key.scancode |= (GetAsyncKeyState(VK_LSHIFT) & 0x8000) ? VXT_KEY_LSHIFT : VXT_KEY_RSHIFT; return key;
                case VK_SNAPSHOT: key.scancode |= VXT_KEY_PRINT; return key;
            }

            char ch = ke->uChar.AsciiChar;
            if (ch >= 0x20 && ch <= 0x7F) {
                key.scancode |= ascii2scan[ch - 0x20];
                key.ascii = ch;
                return key;
            }
        }
    }
}

#endif

#ifndef NO_SDL

#include <SDL2/SDL.h>

static SDL_Window *sdl_window = 0;

static vxt_key_t sdl_getkey(void *ud)
{
    term_getkey(ud); // Drain terminal
	vxt_key_t key = {.scancode = VXT_KEY_INVALID, .ascii = 0};

	if (sdl_window)
	{
		SDL_Event ev;
		while (SDL_PollEvent(&ev))
		{
			if (ev.type != SDL_KEYDOWN && ev.type != SDL_KEYUP) continue;
            key.scancode = ev.type == SDL_KEYDOWN ? 0 : VXT_MASK_KEY_UP;

            switch (ev.key.keysym.sym)
            {
                case SDLK_ESCAPE: key.scancode |= VXT_KEY_ESCAPE; return key;
                case SDLK_RETURN: key.scancode |= VXT_KEY_ENTER; return key;
                case SDLK_BACKSPACE: key.scancode |= VXT_KEY_BACKSPACE; return key;
                case SDLK_TAB: key.scancode |= VXT_KEY_TAB; return key;
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
                case SDLK_LSHIFT: key.scancode |= VXT_KEY_LSHIFT; return key;
                case SDLK_RSHIFT: key.scancode |= VXT_KEY_RSHIFT; return key;
                case SDLK_PRINTSCREEN: key.scancode |= VXT_KEY_PRINT; return key;
                default: if (ev.key.keysym.sym >= 0x20 && ev.key.keysym.sym <= 0x7F) {
                    if (ev.key.keysym.mod&KMOD_RALT)
                    {
                        if (ev.key.keysym.sym)
                        {
                            replace_floppy();
                            return key;
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

#endif

#endif