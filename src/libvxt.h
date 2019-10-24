// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2013-2014 Adrian Cable (adrian.cable@gmail.com)
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#ifndef _LIBVXT_H_
#define _LIBVXT_H_

#include <stddef.h>

typedef struct vxt_emulator vxt_emulator_t;
typedef void* (*vxt_alloc_t)(void*,size_t);

typedef struct {
    void *userdata;

    int (*kbhit)(void*);
    int (*getch)(void*);
    void (*putchar)(void*,int);
} vxt_terminal_t;

typedef struct {
    void *userdata;
    int boot;

    size_t (*read)(void*,void*,size_t);
    size_t (*write)(void*,const void*,size_t); 
    size_t (*seek)(void*,size_t,int);
} vxt_drive_t;

extern vxt_emulator_t *vxt_init(vxt_terminal_t *term, vxt_drive_t *fd, vxt_drive_t *hd, vxt_alloc_t alloc);
extern void vxt_load_bios(vxt_emulator_t *e, const void *data, size_t sz);
extern void vxt_close(vxt_emulator_t *e);
extern int vxt_step(vxt_emulator_t *e);

#endif