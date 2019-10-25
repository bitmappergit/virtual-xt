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

typedef struct vxt_emulator vxt_emulator_t;
typedef void (*vxt_pause_audio_t)(int);

typedef enum {
    VXT_CGA,
    VXT_HERCULES
} vxt_mode_t;

typedef struct {
    void *userdata;

    int (*kbhit)(void*);
    int (*getch)(void*);
    void (*putchar)(void*,int);
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