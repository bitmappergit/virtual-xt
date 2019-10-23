// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2013-2014 Adrian Cable (adrian.cable@gmail.com)
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#ifndef _LIBVXT_H_
#define _LIBVXT_H_

#include <stddef.h>

typedef struct vxt_emulator vxt_emulator_t;
typedef void* (*vxt_alloc)(void*,size_t);

extern vxt_emulator_t *vxt_init(vxt_alloc);
extern void vxt_close(vxt_emulator_t*);

extern int main_vxt(vxt_emulator_t *e, int argc, char *argv[]);

#endif