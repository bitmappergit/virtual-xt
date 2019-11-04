// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2013-2014 Adrian Cable (adrian.cable@gmail.com)
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#ifndef _LIBVXT_H_
#define _LIBVXT_H_

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VXT_INTERNAL_MEMORY 0
#ifndef VXT_GRAPHICS_UPDATE_DELAY
    #define VXT_GRAPHICS_UPDATE_DELAY 360000
#endif

#define VXT_MASK_KEY_UP 0x80

typedef enum {
    VXT_KEY_INVALID, // No key
    VXT_KEY_ESCAPE,
    VXT_KEY_1,
    VXT_KEY_2,
    VXT_KEY_3,
    VXT_KEY_4,
    VXT_KEY_5,
    VXT_KEY_6,
    VXT_KEY_7,
    VXT_KEY_8,
    VXT_KEY_9,
    VXT_KEY_0,
    VXT_KEY_MINUS_OR_UNDERSCORE,
    VXT_KEY_EQUAL_OR_PLUS,
    VXT_KEY_BACKSPACE,
    VXT_KEY_TAB,
    VXT_KEY_Q,
    VXT_KEY_W,
    VXT_KEY_E,
    VXT_KEY_R,
    VXT_KEY_T,
    VXT_KEY_Y,
    VXT_KEY_U,
    VXT_KEY_I,
    VXT_KEY_O,
    VXT_KEY_P,
    VXT_KEY_LBRACKET_LBRACE,
    VXT_KEY_RBRACKET_RBRACE,
    VXT_KEY_ENTER,
    VXT_KEY_CONTROL,
    VXT_KEY_A,
    VXT_KEY_S,
    VXT_KEY_D,
    VXT_KEY_F,
    VXT_KEY_G,
    VXT_KEY_H,
    VXT_KEY_J,
    VXT_KEY_K,
    VXT_KEY_L,
    VXT_KEY_SEMICOLON_COLON,
    VXT_KEY_QUOTE_DQUOTE,
    VXT_KEY_BACKQUOTE_TILDE,
    VXT_KEY_LSHIFT,
    VXT_KEY_BACKSLASH_VBAR,
    VXT_KEY_Z,
    VXT_KEY_X,
    VXT_KEY_C,
    VXT_KEY_V,
    VXT_KEY_B,
    VXT_KEY_N,
    VXT_KEY_M,
    VXT_KEY_COMMA_LESS,
    VXT_KEY_PERIOD_GREATER,
    VXT_KEY_SLASH_QUESTION,
    VXT_KEY_RSHIFT,
    VXT_KEY_PRINT,
    VXT_KEY_ALT,
    VXT_KEY_SPACE,
    VXT_KEY_CAPSLOCK,
    VXT_KEY_F1,
    VXT_KEY_F2,
    VXT_KEY_F3,
    VXT_KEY_F4,
    VXT_KEY_F5,
    VXT_KEY_F6,
    VXT_KEY_F7,
    VXT_KEY_F8,
    VXT_KEY_F9,
    VXT_KEY_F10,
    VXT_KEY_NUMLOCK,
    VXT_KEY_SCROLLOCK,
    VXT_KEY_KP_HOME_7,
    VXT_KEY_KP_UP_8,
    VXT_KEY_KP_PAGEUP_9,
    VXT_KEY_KP_MINUS,
    VXT_KEY_KP_LEFT_4,
    VXT_KEY_KP_5,
    VXT_KEY_KP_RIGHT_6,
    VXT_KEY_KP_PLUS, 
    VXT_KEY_KP_END_1,
    VXT_KEY_KP_DOWN_2,
    VXT_KEY_KP_PAGEDOWN_3,
    VXT_KEY_KP_INSERT,
    VXT_KEY_KP_DELETE,

    // Shifted

    VXT_KEY_EXCLAIM = VXT_KEY_1,
    VXT_KEY_AT = VXT_KEY_2,
    VXT_KEY_HASH = VXT_KEY_3,
    VXT_KEY_DOLLAR = VXT_KEY_4,
    VXT_KEY_PERCENT = VXT_KEY_5

/*
    VXT_KEY_6,
    VXT_KEY_7,
    VXT_KEY_8,
    VXT_KEY_9,
    VXT_KEY_0,
    VXT_KEY_MINUS_OR_UNDERSCORE,
    VXT_KEY_EQUAL_OR_PLUS,
    VXT_KEY_BACKSPACE,
    VXT_KEY_TAB,
    VXT_KEY_Q,
    VXT_KEY_W,
    VXT_KEY_E,
    VXT_KEY_R,
    VXT_KEY_T,
    VXT_KEY_Y,
    VXT_KEY_U,
    VXT_KEY_I,
    VXT_KEY_O,
    VXT_KEY_P,
    VXT_KEY_LBRACKET_LBRACE,
    VXT_KEY_RBRACKET_RBRACE,
    VXT_KEY_ENTER,
    VXT_KEY_CONTROL,
    VXT_KEY_A,
    VXT_KEY_S,
    VXT_KEY_D,
    VXT_KEY_F,
    VXT_KEY_G,
    VXT_KEY_H,
    VXT_KEY_J,
    VXT_KEY_K,
    VXT_KEY_L,
    VXT_KEY_SEMICOLON_COLON,
    VXT_KEY_QUOTE_DQUOTE,
    VXT_KEY_BACKQUOTE_TILDE,
    VXT_KEY_LSHIFT,
    VXT_KEY_BACKSLASH_VBAR,
    VXT_KEY_Z,
    VXT_KEY_X,
    VXT_KEY_C,
    VXT_KEY_V,
    VXT_KEY_B,
    VXT_KEY_N,
    VXT_KEY_M,
    VXT_KEY_COMMA_LESS,
    VXT_KEY_PERIOD_GREATER,
    VXT_KEY_SLASH_QUESTION,
    VXT_KEY_RSHIFT,
    VXT_KEY_PRINT,
    VXT_KEY_ALT,
    VXT_KEY_SPACE,
    VXT_KEY_CAPSLOCK,
    VXT_KEY_F1,
    VXT_KEY_F2,
    VXT_KEY_F3,
    VXT_KEY_F4,
    VXT_KEY_F5,
    VXT_KEY_F6,
    VXT_KEY_F7,
    VXT_KEY_F8,
    VXT_KEY_F9,
    VXT_KEY_F10,
    VXT_KEY_NUMLOCK,
    VXT_KEY_SCROLLOCK,
    VXT_KEY_KP_HOME_7,
    VXT_KEY_KP_UP_8,
    VXT_KEY_KP_PAGEUP_9,
    VXT_KEY_KP_MINUS,
    VXT_KEY_KP_LEFT_4,
    VXT_KEY_KP_5,
    VXT_KEY_KP_RIGHT_6,
    VXT_KEY_KP_PLUS, 
    VXT_KEY_KP_END_1,
    VXT_KEY_KP_DOWN_2,
    VXT_KEY_KP_PAGEDOWN_3,
    VXT_KEY_KP_INSERT,
    VXT_KEY_KP_DELETE
*/
} vxt_scancode_t;

typedef struct vxt_emulator vxt_emulator_t;
typedef void (*vxt_pause_audio_t)(int);

typedef enum {
    VXT_CGA,
    VXT_HERCULES
} vxt_mode_t;

typedef struct {
    vxt_scancode_t scancode;
    char ascii;
} vxt_key_t;

typedef struct {
    void *userdata;

    vxt_key_t (*getkey)(void*);
    void (*putchar)(void*,unsigned char);
} vxt_terminal_t;

typedef struct {
    void *userdata;

    struct tm *(*localtime)(void*);
    unsigned short (*millitm)(void*);
} vxt_clock_t;

typedef struct {
    void *userdata;

    void *(*open)(void*,vxt_mode_t,int,int);
    void (*close)(void*,void*);
    unsigned char *(*buffer)(void*,void*); // Returns a RGB332 buffer with X*Y size
} vxt_video_t;

typedef struct {
    void *userdata;

    int (*filter)(void*,unsigned short, int);
    unsigned char (*in)(void*,unsigned short);
    void (*out)(void*,unsigned short, unsigned char);
} vxt_port_map_t;

typedef struct {
    void *userdata;
    int boot;

    size_t (*read)(void*,void*,size_t);
    size_t (*write)(void*,const void*,size_t); 
    size_t (*seek)(void*,size_t,int);
} vxt_drive_t;

extern vxt_emulator_t *vxt_open(vxt_terminal_t *term, vxt_clock_t *clock, void *mem);
extern size_t vxt_memory_required();
extern void vxt_load_bios(vxt_emulator_t *e, const void *data, size_t sz);
extern void vxt_replace_floppy(vxt_emulator_t *e, vxt_drive_t *fd);
extern void vxt_set_harddrive(vxt_emulator_t *e, vxt_drive_t *hd);
extern void vxt_set_video(vxt_emulator_t *e, vxt_video_t *video);
extern void vxt_set_port_map(vxt_emulator_t *e, vxt_port_map_t *map);
extern void vxt_set_audio_control(vxt_emulator_t *e, vxt_pause_audio_t ac);
extern void vxt_set_audio_silence(vxt_emulator_t *e, unsigned char s);
extern int vxt_step(vxt_emulator_t *e);
extern void vxt_close(vxt_emulator_t *e);

// Expects single channel, 44100Hz, unsigned bytes
extern void vxt_audio_callback(vxt_emulator_t *e, unsigned char *stream, int len);

#ifdef __cplusplus
}
#endif
#endif