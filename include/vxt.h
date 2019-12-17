// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2013-2014 Adrian Cable (adrian.cable@gmail.com)
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#ifndef _VXT_H_
#define _VXT_H_

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VXT_INTERNAL_MEMORY 0

// Modem control
#define VXT_SERIAL_MODEM_DATA_TERMINAL_READY            0x1
#define VXT_SERIAL_MODEM_REQUEST_TO_SEND                0x2
#define VXT_SERIAL_MODEM_AUX_OUTPUT_1                   0x4
#define VXT_SERIAL_MODEM_AUX_OUTPUT_2                   0x8
#define VXT_SERIAL_MODEM_LOOPBACK_MODE                  0x10
#define VXT_SERIAL_MODEM_AUTOFLOW_CONTROL               0x20

// Modem status
#define VXT_SERIAL_MODEM_DELTA_CLEAR_TO_SEND            0x1
#define VXT_SERIAL_MODEM_DELTA_DATA_SET_READY           0x2
#define VXT_SERIAL_MODEM_TRAILING_EDGE_RING_INDICATOR   0x4
#define VXT_SERIAL_MODEM_DELTA_DATA_CARRIER_DETECTED    0x8
#define VXT_SERIAL_MODEM_CLEAR_TO_SEND                  0x10
#define VXT_SERIAL_MODEM_DATA_SET_READY                 0x20
#define VXT_SERIAL_MODEM_RING_INDICATOR                 0x40
#define VXT_SERIAL_MODEM_RECEIVED_LINE_SIGNAL_DETECT    0x80

// Line status
#define VXT_SERIAL_LINE_DATA_READY                      0x1
#define VXT_SERIAL_LINE_OVERRUN_ERROR                   0x2
#define VXT_SERIAL_LINE_PARITY_ERROR                    0x4
#define VXT_SERIAL_LINE_FRAMING_ERROR                   0x8
#define VXT_SERIAL_LINE_BREAK_DETECT                    0x10
#define VXT_SERIAL_LINE_TRANS_HOLDING_REG_EMPTY         0x20
#define VXT_SERIAL_LINE_TRANS_SHIFT_REG_EMPTY           0x40
#define VXT_SERIAL_LINE_TIME_OUT_ERROR                  0x80

// Serial port base addresses
#define VXT_SERIAL_COM1_BASE 0x3F8
#define VXT_SERIAL_COM2_BASE 0x2F8
#define VXT_SERIAL_COM3_BASE 0x3E8
#define VXT_SERIAL_COM4_BASE 0x2E8

// Serial port IO offsets
#define VXT_SERIAL_DATA 0
#define VXT_SERIAL_DLAB_LOW 0
#define VXT_SERIAL_INTERRUPT_ENABLE 1
#define VXT_SERIAL_DLAB_HIGH 1
#define VXT_SERIAL_FIFO_CONTROL 2
#define VXT_SERIAL_LINE_CONTROL 3
#define VXT_SERIAL_MODEM_CONTROL 4
#define VXT_SERIAL_LINE_STATUS 5
#define VXT_SERIAL_MODEM_STATUS 6
#define VXT_SERIAL_SCRATCH 7

#define VXT_MASK_KEY_UP 0x80

typedef unsigned char byte;
typedef unsigned short word;

typedef enum {
    VXT_KEY_INVALID, // No key
    VXT_KEY_ESCAPE,
    VXT_KEY_1_EXCLAIM,
    VXT_KEY_2_AT,
    VXT_KEY_3_HASH,
    VXT_KEY_4_DOLLAR,
    VXT_KEY_5_PERCENT,
    VXT_KEY_6_CARET,
    VXT_KEY_7_AMPERSAND,
    VXT_KEY_8_ASTERISK,
    VXT_KEY_9_LEFTPAREN,
    VXT_KEY_0_RIGHTPAREN,
    VXT_KEY_MINUS_UNDERSCORE,
    VXT_KEY_EQUAL_PLUS,
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
    VXT_KEY_KP_INSERT_0,
    VXT_KEY_KP_DELETE_PERIOD
} vxt_scancode_t;

typedef struct vxt_emulator vxt_emulator_t;
typedef void (*vxt_pause_audio_t)(int);

typedef enum {
    VXT_TEXT,
    VXT_CGA,
    VXT_HERCULES
} vxt_mode_t;

typedef struct {
    vxt_scancode_t scancode;
    char ascii;
} vxt_key_t;

typedef struct {
    void *userdata;

    struct tm *(*localtime)(void*);
    unsigned short (*millitm)(void*);
} vxt_clock_t;

typedef struct {
    void *userdata;

    vxt_key_t (*getkey)(void*);
    void (*initialize)(void*,vxt_mode_t,int,int);
    byte *(*backbuffer)(void*); // Returns a RGB332 buffer with X*Y size
    void (*textmode)(byte*,byte*,byte,byte,byte);
} vxt_video_t;

typedef struct {
    void *userdata;

    int (*filter)(void*,word,int);
    byte (*in)(void*,word);
    void (*out)(void*,word,byte);
} vxt_port_map_t;

typedef struct {
    byte modem;
    byte line;
} vxt_serial_status_t;

typedef struct {
    void *userdata;

    void (*init)(void*,int);
    vxt_serial_status_t (*status)(void*);
    void (*send)(void*,byte);
    byte (*receive)(void*);
} vxt_serial_t;

typedef struct {
    void *userdata;
    int boot;

    size_t (*read)(void*,void*,size_t);
    size_t (*write)(void*,const void*,size_t); 
    size_t (*seek)(void*,size_t,int);
} vxt_drive_t;

extern vxt_emulator_t *vxt_open(vxt_video_t *video, vxt_clock_t *clock, void *mem);
extern size_t vxt_memory_required();
extern const char *vxt_version();
extern void vxt_load_bios(vxt_emulator_t *e, const void *data, size_t sz);
extern void vxt_replace_floppy(vxt_emulator_t *e, vxt_drive_t *fd);
extern void vxt_set_harddrive(vxt_emulator_t *e, vxt_drive_t *hd);
extern void vxt_set_port_map(vxt_emulator_t *e, vxt_port_map_t *map);
extern void vxt_set_serial(vxt_emulator_t *e, int port, vxt_serial_t *com);
extern void vxt_set_audio_control(vxt_emulator_t *e, vxt_pause_audio_t ac, byte silence);
extern int vxt_blink(vxt_emulator_t *e);
extern int vxt_step(vxt_emulator_t *e);
extern void vxt_close(vxt_emulator_t *e);

// Expects single channel, 44100Hz, unsigned bytes
extern void vxt_audio_callback(vxt_emulator_t *e, byte *stream, int len);

#ifdef __cplusplus
}
#endif
#endif