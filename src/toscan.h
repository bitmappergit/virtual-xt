// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#ifndef _WINVKEYS_H_
#define _WINVKEYS_H_

#include <vxt.h>

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

#ifndef NO_SDL

#include <SDL2/SDL.h>

static vxt_scancode_t sdl_to_scancode(SDL_Keycode sym)
{
    switch (sym)
    {
        case SDLK_ESCAPE: return VXT_KEY_ESCAPE;
        case SDLK_RETURN: return VXT_KEY_ENTER;
        default: return VXT_KEY_INVALID;
    }
}

static char sdl_to_ascii(SDL_Keycode sym)
{
    if (sym > 127)
        return 0;
    return (char)sym;
}

#endif

#endif