// VirtualXT - Portable IBM PC/XT emulator written in C.
// Copyright (c) 2013-2014 Adrian Cable (adrian.cable@gmail.com)
// Copyright (c) 2019 Andreas T Jonsson (mail@andreasjonsson.se)
//
// This work is licensed under the MIT License. See included LICENSE file.

#include "libvxt.h"

#include <time.h>
#include <sys/timeb.h>
#include <memory.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#else
#include<conio.h>
#endif

#ifndef NO_GRAPHICS
#include <string.h>
#include <SDL2/SDL.h>
#endif

// Emulator system constants
#define IO_PORT_COUNT 0x10000
#define RAM_SIZE 0x10FFF0
#define REGS_BASE 0xF0000
#define VIDEO_RAM_SIZE 0x10000

// Graphics/timer/keyboard update delays (explained later)
#ifndef GRAPHICS_UPDATE_DELAY
#define GRAPHICS_UPDATE_DELAY 360000
#endif
#define KEYBOARD_TIMER_UPDATE_DELAY 20000

// 16-bit register decodes
#define REG_AX 0
#define REG_CX 1
#define REG_DX 2
#define REG_BX 3
#define REG_SP 4
#define REG_BP 5
#define REG_SI 6
#define REG_DI 7

#define REG_ES 8
#define REG_CS 9
#define REG_SS 10
#define REG_DS 11

#define REG_ZERO 12
#define REG_SCRATCH 13

// 8-bit register decodes
#define REG_AL 0
#define REG_AH 1
#define REG_CL 2
#define REG_CH 3
#define REG_DL 4
#define REG_DH 5
#define REG_BL 6
#define REG_BH 7

// FLAGS register decodes
#define FLAG_CF 40
#define FLAG_PF 41
#define FLAG_AF 42
#define FLAG_ZF 43
#define FLAG_SF 44
#define FLAG_TF 45
#define FLAG_IF 46
#define FLAG_DF 47
#define FLAG_OF 48

// Lookup tables in the BIOS binary
#define TABLE_XLAT_OPCODE 8
#define TABLE_XLAT_SUBFUNCTION 9
#define TABLE_STD_FLAGS 10
#define TABLE_PARITY_FLAG 11
#define TABLE_BASE_INST_SIZE 12
#define TABLE_I_W_SIZE 13
#define TABLE_I_MOD_SIZE 14
#define TABLE_COND_JUMP_DECODE_A 15
#define TABLE_COND_JUMP_DECODE_B 16
#define TABLE_COND_JUMP_DECODE_C 17
#define TABLE_COND_JUMP_DECODE_D 18
#define TABLE_FLAGS_BITFIELDS 19

// Bitfields for TABLE_STD_FLAGS values
#define FLAGS_UPDATE_SZP 1
#define FLAGS_UPDATE_AO_ARITH 2
#define FLAGS_UPDATE_OC_LOGIC 4

typedef unsigned char byte;
typedef unsigned short word;

struct vxt_emulator {
	byte mem[RAM_SIZE], io_ports[IO_PORT_COUNT], bios_table_lookup[20][256];
	byte *opcode_stream, *regs8, *vid_mem_base;
	byte i_rm, i_w, i_reg, i_mod, i_mod_size, i_d, i_reg4bit, raw_opcode_id, xlat_opcode_id, extra, rep_mode, seg_override_en, rep_override_en, trap_flag, int8_asap, scratch_uchar, io_hi_lo, spkr_en;
	word *regs16, reg_ip, seg_override, file_index, wave_counter;
	unsigned int op_source, op_dest, rm_addr, op_to_addr, op_from_addr, i_data0, i_data1, i_data2, scratch_uint, scratch2_uint, inst_counter, set_flags_type, GRAPHICS_X, GRAPHICS_Y, pixel_colors[16], vmem_ctr;
	int op_result, disk[3], scratch_int;
	
	vxt_alloc alloc;
	time_t clock_buf;
	struct timeb ms_clock;
};

#ifndef NO_GRAPHICS
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 1, 0, 128};
SDL_Window *sdl_window;
SDL_Surface *sdl_screen;
SDL_Event sdl_event;
unsigned short vid_addr_lookup[VIDEO_RAM_SIZE], cga_colors[4] = {0 /* Black */, 0x1F1F /* Cyan */, 0xE3E3 /* Magenta */, 0xFFFF /* White */};
#endif

// Helper macros

// Decode mod, r_m and reg fields in instruction
#define DECODE_RM_REG e->scratch2_uint = 4 * !e->i_mod, \
					  e->op_to_addr = e->rm_addr = e->i_mod < 3 ? SEGREG(e->seg_override_en ? e->seg_override : e->bios_table_lookup[e->scratch2_uint + 3][e->i_rm], e->bios_table_lookup[e->scratch2_uint][e->i_rm], e->regs16[e->bios_table_lookup[e->scratch2_uint + 1][e->i_rm]] + e->bios_table_lookup[e->scratch2_uint + 2][e->i_rm] * e->i_data1+) : GET_REG_ADDR(e->i_rm), \
					  e->op_from_addr = GET_REG_ADDR(e->i_reg), \
					  e->i_d && (e->scratch_uint = e->op_from_addr, e->op_from_addr = e->rm_addr, e->op_to_addr = e->scratch_uint)

// Return memory-mapped register location (offset into mem array) for register #reg_id
#define GET_REG_ADDR(reg_id) (REGS_BASE + (e->i_w ? 2 * reg_id : 2 * reg_id + reg_id / 4 & 7))

// Returns number of top bit in operand (i.e. 8 for 8-bit operands, 16 for 16-bit operands)
#define TOP_BIT 8*(e->i_w + 1)

// Opcode execution unit helpers
#define OPCODE ;break; case
#define OPCODE_CHAIN ; case

// [I]MUL/[I]DIV/DAA/DAS/ADC/SBB helpers
#define MUL_MACRO(op_data_type,out_regs) (set_opcode(e, 0x10), \
										  out_regs[e->i_w + 1] = (e->op_result = CAST(op_data_type)e->mem[e->rm_addr] * (op_data_type)*out_regs) >> 16, \
										  e->regs16[REG_AX] = e->op_result, \
										  set_OF(e, set_CF(e, e->op_result - (op_data_type)e->op_result)))
#define DIV_MACRO(out_data_type,in_data_type,out_regs) (e->scratch_int = CAST(out_data_type)e->mem[e->rm_addr]) && !(e->scratch2_uint = (in_data_type)(e->scratch_uint = (out_regs[e->i_w+1] << 16) + e->regs16[REG_AX]) / e->scratch_int, e->scratch2_uint - (out_data_type)e->scratch2_uint) ? out_regs[e->i_w+1] = e->scratch_uint - e->scratch_int * (*out_regs = e->scratch2_uint) : pc_interrupt(e, 0)
#define DAA_DAS(op1,op2,mask,min) set_AF(e, (((e->scratch2_uint = e->regs8[REG_AL]) & 0x0F) > 9) || e->regs8[FLAG_AF]) && (e->op_result = e->regs8[REG_AL] op1 6, set_CF(e, e->regs8[FLAG_CF] || (e->regs8[REG_AL] op2 e->scratch2_uint))), \
								  set_CF(e, (((mask & 1 ? e->scratch2_uint : e->regs8[REG_AL]) & mask) > min) || e->regs8[FLAG_CF]) && (e->op_result = e->regs8[REG_AL] op1 0x60)
#define ADC_SBB_MACRO(a) OP(a##= e->regs8[FLAG_CF] +), \
						 set_CF(e, e->regs8[FLAG_CF] && (e->op_result == e->op_dest) || (a e->op_result < a(int)e->op_dest)), \
						 set_AF_OF_arith(e)

// Execute arithmetic/logic operations in emulator memory/registers
#define R_M_OP(dest,op,src) (e->i_w ? e->op_dest = CAST(unsigned short)dest, e->op_result = CAST(unsigned short)dest op (e->op_source = CAST(unsigned short)src) \
								 : (e->op_dest = dest, e->op_result = dest op (e->op_source = CAST(unsigned char)src)))
#define MEM_OP(dest,op,src) R_M_OP(e->mem[dest],op,e->mem[src])
#define OP(op) MEM_OP(e->op_to_addr,op,e->op_from_addr)

// Increment or decrement a register #reg_id (usually SI or DI), depending on direction flag and operand size (given by i_w)
#define INDEX_INC(reg_id) (e->regs16[reg_id] -= (2 * e->regs8[FLAG_DF] - 1)*(e->i_w + 1))

// Helpers for stack operations
#define R_M_PUSH(a) (e->i_w = 1, R_M_OP(e->mem[SEGREG(REG_SS, REG_SP, --)], =, a))
#define R_M_POP(a) (e->i_w = 1, e->regs16[REG_SP] += 2, R_M_OP(a, =, e->mem[SEGREG(REG_SS, REG_SP, -2+)]))

// Convert segment:offset to linear address in emulator memory space
#define SEGREG(reg_seg,reg_ofs,op) 16 * e->regs16[reg_seg] + (unsigned short)(op e->regs16[reg_ofs])

// Returns sign bit of an 8-bit or 16-bit operand
#define SIGN_OF(a) (1 & (e->i_w ? CAST(short)a : a) >> (TOP_BIT - 1))

// Reinterpretation cast
#define CAST(a) *(a*)&

// Keyboard driver for console. This may need changing for UNIX/non-UNIX platforms
#ifdef _WIN32
#define KEYBOARD_DRIVER kbhit() && (e->mem[0x4A6] = getch(), pc_interrupt(e, 7))
#else
#define KEYBOARD_DRIVER read(0, mem + 0x4A6, 1) && (e->int8_asap = (mem[0x4A6] == 0x1B), pc_interrupt(e, 7))
#endif

// Keyboard driver for SDL
#ifdef NO_GRAPHICS
#define SDL_KEYBOARD_DRIVER KEYBOARD_DRIVER
#else
//#define SDL_KEYBOARD_DRIVER sdl_screen ? SDL_PollEvent(&sdl_event) && (sdl_event.type == SDL_KEYDOWN || sdl_event.type == SDL_KEYUP) && (scratch_uint = sdl_event.key.keysym.unicode, scratch2_uint = sdl_event.key.keysym.mod, CAST(short)mem[0x4A6] = 0x400 + 0x800*!!(scratch2_uint & KMOD_ALT) + 0x1000*!!(scratch2_uint & KMOD_SHIFT) + 0x2000*!!(scratch2_uint & KMOD_CTRL) + 0x4000*(sdl_event.type == SDL_KEYUP) + ((!scratch_uint || scratch_uint > 0x7F) ? sdl_event.key.keysym.sym : scratch_uint), pc_interrupt(7)) : (KEYBOARD_DRIVER)

// This needs to be fixed. /aj
#define SDL_KEYBOARD_DRIVER sdl_screen ? SDL_PollEvent(&sdl_event) && (sdl_event.type == SDL_KEYDOWN || sdl_event.type == SDL_KEYUP) && (e->mem[0x4A6] = (unsigned char)tolower(*SDL_GetKeyName(sdl_event.key.keysym.sym)), pc_interrupt(e, 7)) : (KEYBOARD_DRIVER) 

#endif

// Helper functions

// Set carry flag
static char set_CF(vxt_emulator_t *e, int new_CF)
{
	return e->regs8[FLAG_CF] = !!new_CF;
}

// Set auxiliary flag
static char set_AF(vxt_emulator_t *e, int new_AF)
{
	return e->regs8[FLAG_AF] = !!new_AF;
}

// Set overflow flag
static char set_OF(vxt_emulator_t *e, int new_OF)
{
	return e->regs8[FLAG_OF] = !!new_OF;
}

// Set auxiliary and overflow flag after arithmetic operations
static char set_AF_OF_arith(vxt_emulator_t *e)
{
	set_AF(e, (e->op_source ^= e->op_dest ^ e->op_result) & 0x10);
	if (e->op_result == e->op_dest)
		return set_OF(e, 0);
	else
		return set_OF(e, 1 & (e->regs8[FLAG_CF] ^ e->op_source >> (TOP_BIT - 1)));
}

// Assemble and return emulated CPU FLAGS register in scratch_uint
static void make_flags(vxt_emulator_t *e)
{
	e->scratch_uint = 0xF002; // 8086 has reserved and unused flags set to 1
	for (int i = 9; i--;)
		e->scratch_uint += e->regs8[FLAG_CF + i] << e->bios_table_lookup[TABLE_FLAGS_BITFIELDS][i];
}

// Set emulated CPU FLAGS register from regs8[FLAG_xx] values
static void set_flags(vxt_emulator_t *e, int new_flags)
{
	for (int i = 9; i--;)
		e->regs8[FLAG_CF + i] = !!(1 << e->bios_table_lookup[TABLE_FLAGS_BITFIELDS][i] & new_flags);
}

// Convert raw opcode to translated opcode index. This condenses a large number of different encodings of similar
// instructions into a much smaller number of distinct functions, which we then execute
static void set_opcode(vxt_emulator_t *e, unsigned char opcode)
{
	e->xlat_opcode_id = e->bios_table_lookup[TABLE_XLAT_OPCODE][e->raw_opcode_id = opcode];
	e->extra = e->bios_table_lookup[TABLE_XLAT_SUBFUNCTION][opcode];
	e->i_mod_size = e->bios_table_lookup[TABLE_I_MOD_SIZE][opcode];
	e->set_flags_type = e->bios_table_lookup[TABLE_STD_FLAGS][opcode];
}

// Execute INT #interrupt_num on the emulated machine
static char pc_interrupt(vxt_emulator_t *e, unsigned char interrupt_num)
{
	set_opcode(e, 0xCD); // Decode like INT

	make_flags(e);
	R_M_PUSH(e->scratch_uint);
	R_M_PUSH(e->regs16[REG_CS]);
	R_M_PUSH(e->reg_ip);
	MEM_OP(REGS_BASE + 2 * REG_CS, =, 4 * interrupt_num + 2);
	R_M_OP(e->reg_ip, =, e->mem[4 * interrupt_num]);

	return e->regs8[FLAG_TF] = e->regs8[FLAG_IF] = 0;
}

// AAA and AAS instructions - which_operation is +1 for AAA, and -1 for AAS
static int AAA_AAS(vxt_emulator_t *e, char which_operation)
{
	return (e->regs16[REG_AX] += 262 * which_operation*set_AF(e, set_CF(e, ((e->regs8[REG_AL] & 0x0F) > 9) || e->regs8[FLAG_AF])), e->regs8[REG_AL] &= 0x0F);
}

#ifndef NO_GRAPHICS
void audio_callback(void *data, unsigned char *stream, int len)
{
	vxt_emulator_t *e = (vxt_emulator_t*)data;
	for (int i = 0; i < len; i++)
		stream[i] = (e->spkr_en == 3) && CAST(unsigned short)e->mem[0x4AA] ? -((54 * e->wave_counter++ / CAST(unsigned short)e->mem[0x4AA]) & 1) : sdl_audio.silence;

	e->spkr_en = e->io_ports[0x61] & 3;
}
#endif

static void *default_alloc(void *p, size_t sz)
{
	if (!p) return malloc(sz);
	if (p && sz) return realloc(p, sz);
	if (p && !sz) free(p);
}

vxt_emulator_t *vxt_init(vxt_alloc alloc)
{
	alloc = alloc ? alloc : default_alloc;
	vxt_emulator_t *e = (vxt_emulator_t*)alloc(0, sizeof(vxt_emulator_t));
	e->alloc = alloc;
	return e;
}

void vxt_close(vxt_emulator_t *e)
{
	e->alloc(e, 0);
}

// Emulator entry point
int main_vxt(vxt_emulator_t *e, int argc, char **argv)
{
#ifndef NO_GRAPHICS
	// Initialise SDL
	SDL_Init(SDL_INIT_AUDIO);
	sdl_audio.callback = audio_callback;
	sdl_audio.userdata = (void*)e;
#ifdef _WIN32
	sdl_audio.samples = 512;
#endif
	SDL_OpenAudio(&sdl_audio, 0);
#endif

	// regs16 and reg8 point to F000:0, the start of memory-mapped registers. CS is initialised to F000
	e->regs16 = (unsigned short *)(e->regs8 = e->mem + REGS_BASE);
	e->regs16[REG_CS] = 0xF000;

	// Trap flag off
	e->regs8[FLAG_TF] = 0;

	// Set DL equal to the boot device: 0 for the FD, or 0x80 for the HD. Normally, boot from the FD.
	// But, if the HD image file is prefixed with @, then boot from the HD
	e->regs8[REG_DL] = ((argc > 3) && (*argv[3] == '@')) ? argv[3]++, 0x80 : 0;

	// Open BIOS (file id disk[2]), floppy disk image (disk[1]), and hard disk image (disk[0]) if specified
	for (e->file_index = 3; e->file_index;)
		e->disk[--e->file_index] = *++argv ? open(*argv, 32898) : 0;

	// Set CX:AX equal to the hard disk image size, if present
	CAST(unsigned)e->regs16[REG_AX] = *e->disk ? lseek(*e->disk, 0, 2) >> 9 : 0;

	// Load BIOS image into F000:0100, and set IP to 0100
	read(e->disk[2], e->regs8 + (e->reg_ip = 0x100), 0xFF00);

	// Load instruction decoding helper table
	for (int i = 0; i < 20; i++)
		for (int j = 0; j < 256; j++)
			e->bios_table_lookup[i][j] = e->regs8[e->regs16[0x81 + i] + j];

	// Instruction execution loop. Terminates if CS:IP = 0:0
	for (; e->opcode_stream = e->mem + 16 * e->regs16[REG_CS] + e->reg_ip, e->opcode_stream != e->mem;)
	{
		// Set up variables to prepare for decoding an opcode
		set_opcode(e, *e->opcode_stream);

		// Extract i_w and i_d fields from instruction
		e->i_w = (e->i_reg4bit = e->raw_opcode_id & 7) & 1;
		e->i_d = e->i_reg4bit / 2 & 1;

		// Extract instruction data fields
		e->i_data0 = CAST(short)e->opcode_stream[1];
		e->i_data1 = CAST(short)e->opcode_stream[2];
		e->i_data2 = CAST(short)e->opcode_stream[3];

		// seg_override_en and rep_override_en contain number of instructions to hold segment override and REP prefix respectively
		if (e->seg_override_en)
			e->seg_override_en--;
		if (e->rep_override_en)
			e->rep_override_en--;

		// i_mod_size > 0 indicates that opcode uses i_mod/i_rm/i_reg, so decode them
		if (e->i_mod_size)
		{
			e->i_mod = (e->i_data0 & 0xFF) >> 6;
			e->i_rm = e->i_data0 & 7;
			e->i_reg = e->i_data0 / 8 & 7;

			if ((!e->i_mod && e->i_rm == 6) || (e->i_mod == 2))
				e->i_data2 = CAST(short)e->opcode_stream[4];
			else if (e->i_mod != 1)
				e->i_data2 = e->i_data1;
			else // If i_mod is 1, operand is (usually) 8 bits rather than 16 bits
				e->i_data1 = (char)e->i_data1;

			DECODE_RM_REG;
		}

		// Instruction execution unit
		switch (e->xlat_opcode_id)
		{
			OPCODE_CHAIN 0: // Conditional jump (JAE, JNAE, etc.)
				// i_w is the invert flag, e.g. i_w == 1 means JNAE, whereas i_w == 0 means JAE 
				e->scratch_uchar = e->raw_opcode_id / 2 & 7;
				e->reg_ip += (char)e->i_data0 * (e->i_w ^ (e->regs8[e->bios_table_lookup[TABLE_COND_JUMP_DECODE_A][e->scratch_uchar]] || e->regs8[e->bios_table_lookup[TABLE_COND_JUMP_DECODE_B][e->scratch_uchar]] || e->regs8[e->bios_table_lookup[TABLE_COND_JUMP_DECODE_C][e->scratch_uchar]] ^ e->regs8[e->bios_table_lookup[TABLE_COND_JUMP_DECODE_D][e->scratch_uchar]]))
			OPCODE 1: // MOV reg, imm
				e->i_w = !!(e->raw_opcode_id & 8);
				R_M_OP(e->mem[GET_REG_ADDR(e->i_reg4bit)], =, e->i_data0)
			OPCODE 3: // PUSH regs16
				R_M_PUSH(e->regs16[e->i_reg4bit])
			OPCODE 4: // POP regs16
				R_M_POP(e->regs16[e->i_reg4bit])
			OPCODE 2: // INC|DEC regs16
				e->i_w = 1;
				e->i_d = 0;
				e->i_reg = e->i_reg4bit;
				DECODE_RM_REG;
				e->i_reg = e->extra
			OPCODE_CHAIN 5: // INC|DEC|JMP|CALL|PUSH
				if (e->i_reg < 2) // INC|DEC
					MEM_OP(e->op_from_addr, += 1 - 2 * e->i_reg +, REGS_BASE + 2 * REG_ZERO),
					e->op_source = 1,
					set_AF_OF_arith(e),
					set_OF(e, e->op_dest + 1 - e->i_reg == 1 << (TOP_BIT - 1)),
					(e->xlat_opcode_id == 5) && (set_opcode(e, 0x10), 0); // Decode like ADC
				else if (e->i_reg != 6) // JMP|CALL
					e->i_reg - 3 || R_M_PUSH(e->regs16[REG_CS]), // CALL (far)
					e->i_reg & 2 && R_M_PUSH(e->reg_ip + 2 + e->i_mod*(e->i_mod != 3) + 2*(!e->i_mod && e->i_rm == 6)), // CALL (near or far)
					e->i_reg & 1 && (e->regs16[REG_CS] = CAST(short)e->mem[e->op_from_addr + 2]), // JMP|CALL (far)
					R_M_OP(e->reg_ip, =, e->mem[e->op_from_addr]),
					set_opcode(e, 0x9A); // Decode like CALL
				else // PUSH
					R_M_PUSH(e->mem[e->rm_addr])
			OPCODE 6: // TEST r/m, imm16 / NOT|NEG|MUL|IMUL|DIV|IDIV reg
				e->op_to_addr = e->op_from_addr;

				switch (e->i_reg)
				{
					OPCODE_CHAIN 0: // TEST
						set_opcode(e, 0x20); // Decode like AND
						e->reg_ip += e->i_w + 1;
						R_M_OP(e->mem[e->op_to_addr], &, e->i_data2)
					OPCODE 2: // NOT
						OP(=~)
					OPCODE 3: // NEG
						OP(=-);
						e->op_dest = 0;
						set_opcode(e, 0x28); // Decode like SUB
						set_CF(e, e->op_result > e->op_dest)
					OPCODE 4: // MUL
						e->i_w ? MUL_MACRO(unsigned short, e->regs16) : MUL_MACRO(unsigned char, e->regs8)
					OPCODE 5: // IMUL
						e->i_w ? MUL_MACRO(short, e->regs16) : MUL_MACRO(char, e->regs8)
					OPCODE 6: // DIV
						e->i_w ? DIV_MACRO(unsigned short, unsigned, e->regs16) : DIV_MACRO(unsigned char, unsigned short, e->regs8)
					OPCODE 7: // IDIV
						e->i_w ? DIV_MACRO(short, int, e->regs16) : DIV_MACRO(char, short, e->regs8);
				}
			OPCODE 7: // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP AL/AX, immed
				e->rm_addr = REGS_BASE;
				e->i_data2 = e->i_data0;
				e->i_mod = 3;
				e->i_reg = e->extra;
				e->reg_ip--;
			OPCODE_CHAIN 8: // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP reg, immed
				e->op_to_addr = e->rm_addr;
				e->regs16[REG_SCRATCH] = (e->i_d |= !e->i_w) ? (char)e->i_data2 : e->i_data2;
				e->op_from_addr = REGS_BASE + 2 * REG_SCRATCH;
				e->reg_ip += !e->i_d + 1;
				set_opcode(e, 0x08 * (e->extra = e->i_reg));
			OPCODE_CHAIN 9: // ADD|OR|ADC|SBB|AND|SUB|XOR|CMP|MOV reg, r/m
				switch (e->extra)
				{
					OPCODE_CHAIN 0: // ADD
						OP(+=),
						set_CF(e, e->op_result < e->op_dest)
					OPCODE 1: // OR
						OP(|=)
					OPCODE 2: // ADC
						ADC_SBB_MACRO(+)
					OPCODE 3: // SBB
						ADC_SBB_MACRO(-)
					OPCODE 4: // AND
						OP(&=)
					OPCODE 5: // SUB
						OP(-=),
						set_CF(e, e->op_result > e->op_dest)
					OPCODE 6: // XOR
						OP(^=)
					OPCODE 7: // CMP
						OP(-),
						set_CF(e, e->op_result > e->op_dest)
					OPCODE 8: // MOV
						OP(=);
				}
			OPCODE 10: // MOV sreg, r/m | POP r/m | LEA reg, r/m
				if (!e->i_w) // MOV
					e->i_w = 1,
					e->i_reg += 8,
					DECODE_RM_REG,
					OP(=);
				else if (!e->i_d) // LEA
					e->seg_override_en = 1,
					e->seg_override = REG_ZERO,
					DECODE_RM_REG,
					R_M_OP(e->mem[e->op_from_addr], =, e->rm_addr);
				else // POP
					R_M_POP(e->mem[e->rm_addr])
			OPCODE 11: // MOV AL/AX, [loc]
				e->i_mod = e->i_reg = 0;
				e->i_rm = 6;
				e->i_data1 = e->i_data0;
				DECODE_RM_REG;
				MEM_OP(e->op_from_addr, =, e->op_to_addr)
			OPCODE 12: // ROL|ROR|RCL|RCR|SHL|SHR|???|SAR reg/mem, 1/CL/imm (80186)
				e->scratch2_uint = SIGN_OF(e->mem[e->rm_addr]),
				e->scratch_uint = e->extra ? // xxx reg/mem, imm
					++e->reg_ip,
					(char)e->i_data1
				: // xxx reg/mem, CL
					e->i_d
						? 31 & e->regs8[REG_CL]
				: // xxx reg/mem, 1
					1;
				if (e->scratch_uint)
				{
					if (e->i_reg < 4) // Rotate operations
						e->scratch_uint %= e->i_reg / 2 + TOP_BIT,
						R_M_OP(e->scratch2_uint, =, e->mem[e->rm_addr]);
					if (e->i_reg & 1) // Rotate/shift right operations
						R_M_OP(e->mem[e->rm_addr], >>=, e->scratch_uint);
					else // Rotate/shift left operations
						R_M_OP(e->mem[e->rm_addr], <<=, e->scratch_uint);
					if (e->i_reg > 3) // Shift operations
						set_opcode(e, 0x10); // Decode like ADC
					if (e->i_reg > 4) // SHR or SAR
						set_CF(e, e->op_dest >> (e->scratch_uint - 1) & 1);
				}

				switch (e->i_reg)
				{
					OPCODE_CHAIN 0: // ROL
						R_M_OP(e->mem[e->rm_addr], += , e->scratch2_uint >> (TOP_BIT - e->scratch_uint));
						set_OF(e, SIGN_OF(e->op_result) ^ set_CF(e, e->op_result & 1))
					OPCODE 1: // ROR
						e->scratch2_uint &= (1 << e->scratch_uint) - 1,
						R_M_OP(e->mem[e->rm_addr], += , e->scratch2_uint << (TOP_BIT - e->scratch_uint));
						set_OF(e, SIGN_OF(e->op_result * 2) ^ set_CF(e, SIGN_OF(e->op_result)))
					OPCODE 2: // RCL
						R_M_OP(e->mem[e->rm_addr], += (e->regs8[FLAG_CF] << (e->scratch_uint - 1)) + , e->scratch2_uint >> (1 + TOP_BIT - e->scratch_uint));
						set_OF(e, SIGN_OF(e->op_result) ^ set_CF(e, e->scratch2_uint & 1 << (TOP_BIT - e->scratch_uint)))
					OPCODE 3: // RCR
						R_M_OP(e->mem[e->rm_addr], += (e->regs8[FLAG_CF] << (TOP_BIT - e->scratch_uint)) + , e->scratch2_uint << (1 + TOP_BIT - e->scratch_uint));
						set_CF(e, e->scratch2_uint & 1 << (e->scratch_uint - 1));
						set_OF(e, SIGN_OF(e->op_result) ^ SIGN_OF(e->op_result * 2))
					OPCODE 4: // SHL
						set_OF(e, SIGN_OF(e->op_result) ^ set_CF(e, SIGN_OF(e->op_dest << (e->scratch_uint - 1))))
					OPCODE 5: // SHR
						set_OF(e, SIGN_OF(e->op_dest))
					OPCODE 7: // SAR
						e->scratch_uint < TOP_BIT || set_CF(e, e->scratch2_uint);
						set_OF(e, 0);
						R_M_OP(e->mem[e->rm_addr], +=, e->scratch2_uint *= ~(((1 << TOP_BIT) - 1) >> e->scratch_uint));
				}
			OPCODE 13: // LOOPxx|JCZX
				e->scratch_uint = !!--e->regs16[REG_CX];

				switch(e->i_reg4bit)
				{
					OPCODE_CHAIN 0: // LOOPNZ
						e->scratch_uint &= !e->regs8[FLAG_ZF]
					OPCODE 1: // LOOPZ
						e->scratch_uint &= e->regs8[FLAG_ZF]
					OPCODE 3: // JCXXZ
						e->scratch_uint = !++e->regs16[REG_CX];
				}
				e->reg_ip += e->scratch_uint*(char)e->i_data0
			OPCODE 14: // JMP | CALL short/near
				e->reg_ip += 3 - e->i_d;
				if (!e->i_w)
				{
					if (e->i_d) // JMP far
						e->reg_ip = 0,
						e->regs16[REG_CS] = e->i_data2;
					else // CALL
						R_M_PUSH(e->reg_ip);
				}
				e->reg_ip += e->i_d && e->i_w ? (char)e->i_data0 : e->i_data0
			OPCODE 15: // TEST reg, r/m
				MEM_OP(e->op_from_addr, &, e->op_to_addr)
			OPCODE 16: // XCHG AX, regs16
				e->i_w = 1;
				e->op_to_addr = REGS_BASE;
				e->op_from_addr = GET_REG_ADDR(e->i_reg4bit);
			OPCODE_CHAIN 24: // NOP|XCHG reg, r/m
				if (e->op_to_addr != e->op_from_addr)
					OP(^=),
					MEM_OP(e->op_from_addr, ^=, e->op_to_addr),
					OP(^=)
			OPCODE 17: // MOVSx (extra=0)|STOSx (extra=1)|LODSx (extra=2)
				e->scratch2_uint = e->seg_override_en ? e->seg_override : REG_DS;

				for (e->scratch_uint = e->rep_override_en ? e->regs16[REG_CX] : 1; e->scratch_uint; e->scratch_uint--)
				{
					MEM_OP(e->extra < 2 ? SEGREG(REG_ES, REG_DI,) : REGS_BASE, =, e->extra & 1 ? REGS_BASE : SEGREG(e->scratch2_uint, REG_SI,)),
					e->extra & 1 || INDEX_INC(REG_SI),
					e->extra & 2 || INDEX_INC(REG_DI);
				}

				if (e->rep_override_en)
					e->regs16[REG_CX] = 0
			OPCODE 18: // CMPSx (extra=0)|SCASx (extra=1)
				e->scratch2_uint = e->seg_override_en ? e->seg_override : REG_DS;

				if ((e->scratch_uint = e->rep_override_en ? e->regs16[REG_CX] : 1))
				{
					for (; e->scratch_uint; e->rep_override_en || e->scratch_uint--)
					{
						MEM_OP(e->extra ? REGS_BASE : SEGREG(e->scratch2_uint, REG_SI,), -, SEGREG(REG_ES, REG_DI,)),
						e->extra || INDEX_INC(REG_SI),
						INDEX_INC(REG_DI), e->rep_override_en && !(--e->regs16[REG_CX] && (!e->op_result == e->rep_mode)) && (e->scratch_uint = 0);
					}

					e->set_flags_type = FLAGS_UPDATE_SZP | FLAGS_UPDATE_AO_ARITH; // Funge to set SZP/AO flags
					set_CF(e, e->op_result > e->op_dest);
				}
			OPCODE 19: // RET|RETF|IRET
				e->i_d = e->i_w;
				R_M_POP(e->reg_ip);
				if (e->extra) // IRET|RETF|RETF imm16
					R_M_POP(e->regs16[REG_CS]);
				if (e->extra & 2) // IRET
					set_flags(e, R_M_POP(e->scratch_uint));
				else if (!e->i_d) // RET|RETF imm16
					e->regs16[REG_SP] += e->i_data0
			OPCODE 20: // MOV r/m, immed
				R_M_OP(e->mem[e->op_from_addr], =, e->i_data2)
			OPCODE 21: // IN AL/AX, DX/imm8
				e->io_ports[0x20] = 0; // PIC EOI
				e->io_ports[0x42] = --e->io_ports[0x40]; // PIT channel 0/2 read placeholder
				e->io_ports[0x3DA] ^= 9; // CGA refresh
				e->scratch_uint = e->extra ? e->regs16[REG_DX] : (unsigned char)e->i_data0;
				e->scratch_uint == 0x60 && (e->io_ports[0x64] = 0); // Scancode read flag
				e->scratch_uint == 0x3D5 && (e->io_ports[0x3D4] >> 1 == 7) && (e->io_ports[0x3D5] = ((e->mem[0x49E]*80 + e->mem[0x49D] + CAST(short)e->mem[0x4AD]) & (e->io_ports[0x3D4] & 1 ? 0xFF : 0xFF00)) >> (e->io_ports[0x3D4] & 1 ? 0 : 8)); // CRT cursor position
				R_M_OP(e->regs8[REG_AL], =, e->io_ports[e->scratch_uint]);
			OPCODE 22: // OUT DX/imm8, AL/AX
				e->scratch_uint = e->extra ? e->regs16[REG_DX] : (unsigned char)e->i_data0;
				R_M_OP(e->io_ports[e->scratch_uint], =, e->regs8[REG_AL]);
				e->scratch_uint == 0x61 && (e->io_hi_lo = 0, e->spkr_en |= e->regs8[REG_AL] & 3); // Speaker control
				(e->scratch_uint == 0x40 || e->scratch_uint == 0x42) && (e->io_ports[0x43] & 6) && (e->mem[0x469 + e->scratch_uint - (e->io_hi_lo ^= 1)] = e->regs8[REG_AL]); // PIT rate programming
#ifndef NO_GRAPHICS
				e->scratch_uint == 0x43 && (e->io_hi_lo = 0, e->regs8[REG_AL] >> 6 == 2) && (SDL_PauseAudio((e->regs8[REG_AL] & 0xF7) != 0xB6), 0); // Speaker enable
#endif
				e->scratch_uint == 0x3D5 && (e->io_ports[0x3D4] >> 1 == 6) && (e->mem[0x4AD + !(e->io_ports[0x3D4] & 1)] = e->regs8[REG_AL]); // CRT video RAM start offset
				e->scratch_uint == 0x3D5 && (e->io_ports[0x3D4] >> 1 == 7) && (e->scratch2_uint = ((e->mem[0x49E]*80 + e->mem[0x49D] + CAST(short)e->mem[0x4AD]) & (e->io_ports[0x3D4] & 1 ? 0xFF00 : 0xFF)) + (e->regs8[REG_AL] << (e->io_ports[0x3D4] & 1 ? 0 : 8)) - CAST(short)e->mem[0x4AD], e->mem[0x49D] = e->scratch2_uint % 80, e->mem[0x49E] = e->scratch2_uint / 80); // CRT cursor position
				e->scratch_uint == 0x3B5 && e->io_ports[0x3B4] == 1 && (e->GRAPHICS_X = e->regs8[REG_AL] * 16); // Hercules resolution reprogramming. Defaults are set in the BIOS
				e->scratch_uint == 0x3B5 && e->io_ports[0x3B4] == 6 && (e->GRAPHICS_Y = e->regs8[REG_AL] * 4);
			OPCODE 23: // REPxx
				e->rep_override_en = 2;
				e->rep_mode = e->i_w;
				e->seg_override_en && e->seg_override_en++
			OPCODE 25: // PUSH reg
				R_M_PUSH(e->regs16[e->extra])
			OPCODE 26: // POP reg
				R_M_POP(e->regs16[e->extra])
			OPCODE 27: // xS: segment overrides
				e->seg_override_en = 2;
				e->seg_override = e->extra;
				e->rep_override_en && e->rep_override_en++
			OPCODE 28: // DAA/DAS
				e->i_w = 0;
				e->extra ? DAA_DAS(-=, >=, 0xFF, 0x99) : DAA_DAS(+=, <, 0xF0, 0x90) // extra = 0 for DAA, 1 for DAS
			OPCODE 29: // AAA/AAS
				e->op_result = AAA_AAS(e, e->extra - 1)
			OPCODE 30: // CBW
				e->regs8[REG_AH] = -SIGN_OF(e->regs8[REG_AL])
			OPCODE 31: // CWD
				e->regs16[REG_DX] = -SIGN_OF(e->regs16[REG_AX])
			OPCODE 32: // CALL FAR imm16:imm16
				R_M_PUSH(e->regs16[REG_CS]);
				R_M_PUSH(e->reg_ip + 5);
				e->regs16[REG_CS] = e->i_data2;
				e->reg_ip = e->i_data0
			OPCODE 33: // PUSHF
				make_flags(e);
				R_M_PUSH(e->scratch_uint)
			OPCODE 34: // POPF
				set_flags(e, R_M_POP(e->scratch_uint))
			OPCODE 35: // SAHF
				make_flags(e);
				set_flags(e, (e->scratch_uint & 0xFF00) + e->regs8[REG_AH])
			OPCODE 36: // LAHF
				make_flags(e),
				e->regs8[REG_AH] = e->scratch_uint
			OPCODE 37: // LES|LDS reg, r/m
				e->i_w = e->i_d = 1;
				DECODE_RM_REG;
				OP(=);
				MEM_OP(REGS_BASE + e->extra, =, e->rm_addr + 2)
			OPCODE 38: // INT 3
				++e->reg_ip;
				pc_interrupt(e, 3)
			OPCODE 39: // INT imm8
				e->reg_ip += 2;
				pc_interrupt(e, e->i_data0)
			OPCODE 40: // INTO
				++e->reg_ip;
				e->regs8[FLAG_OF] && pc_interrupt(e, 4)
			OPCODE 41: // AAM
				if (e->i_data0 &= 0xFF)
					e->regs8[REG_AH] = e->regs8[REG_AL] / e->i_data0,
					e->op_result = e->regs8[REG_AL] %= e->i_data0;
				else // Divide by zero
					pc_interrupt(e, 0)
			OPCODE 42: // AAD
				e->i_w = 0;
				e->regs16[REG_AX] = e->op_result = 0xFF & e->regs8[REG_AL] + e->i_data0 * e->regs8[REG_AH]
			OPCODE 43: // SALC
				e->regs8[REG_AL] = -e->regs8[FLAG_CF]
			OPCODE 44: // XLAT
				e->regs8[REG_AL] = e->mem[SEGREG(e->seg_override_en ? e->seg_override : REG_DS, REG_BX, e->regs8[REG_AL] +)]
			OPCODE 45: // CMC
				e->regs8[FLAG_CF] ^= 1
			OPCODE 46: // CLC|STC|CLI|STI|CLD|STD
				e->regs8[e->extra / 2] = e->extra & 1
			OPCODE 47: // TEST AL/AX, immed
				R_M_OP(e->regs8[REG_AL], &, e->i_data0)
			OPCODE 48: // Emulator-specific 0F xx opcodes
				switch ((char)e->i_data0)
				{
					OPCODE_CHAIN 0: // PUTCHAR_AL
						write(1, e->regs8, 1)
					OPCODE 1: // GET_RTC
						time(&e->clock_buf);
						ftime(&e->ms_clock);
						memcpy(e->mem + SEGREG(REG_ES, REG_BX,), localtime(&e->clock_buf), sizeof(struct tm));
						CAST(short)e->mem[SEGREG(REG_ES, REG_BX, 36+)] = e->ms_clock.millitm;
					OPCODE 2: // DISK_READ
					OPCODE_CHAIN 3: // DISK_WRITE
						e->regs8[REG_AL] = ~lseek(e->disk[e->regs8[REG_DL]], CAST(unsigned)e->regs16[REG_BP] << 9, 0)
							? ((char)e->i_data0 == 3 ? (int(*)())write : (int(*)())read)(e->disk[e->regs8[REG_DL]], e->mem + SEGREG(REG_ES, REG_BX,), e->regs16[REG_AX])
							: 0;
				}
		}

		// Increment instruction pointer by computed instruction length. Tables in the BIOS binary
		// help us here.
		e->reg_ip += (e->i_mod*(e->i_mod != 3) + 2*(!e->i_mod && e->i_rm == 6))*e->i_mod_size + e->bios_table_lookup[TABLE_BASE_INST_SIZE][e->raw_opcode_id] + e->bios_table_lookup[TABLE_I_W_SIZE][e->raw_opcode_id]*(e->i_w + 1);

		// If instruction needs to update SF, ZF and PF, set them as appropriate
		if (e->set_flags_type & FLAGS_UPDATE_SZP)
		{
			e->regs8[FLAG_SF] = SIGN_OF(e->op_result);
			e->regs8[FLAG_ZF] = !e->op_result;
			e->regs8[FLAG_PF] = e->bios_table_lookup[TABLE_PARITY_FLAG][(unsigned char)e->op_result];

			// If instruction is an arithmetic or logic operation, also set AF/OF/CF as appropriate.
			if (e->set_flags_type & FLAGS_UPDATE_AO_ARITH)
				set_AF_OF_arith(e);
			if (e->set_flags_type & FLAGS_UPDATE_OC_LOGIC)
				set_CF(e, 0), set_OF(e, 0);
		}

		// Poll timer/keyboard every KEYBOARD_TIMER_UPDATE_DELAY instructions
		if (!(++e->inst_counter % KEYBOARD_TIMER_UPDATE_DELAY))
			e->int8_asap = 1;

#ifndef NO_GRAPHICS
		// Update the video graphics display every GRAPHICS_UPDATE_DELAY instructions
		if (!(e->inst_counter % GRAPHICS_UPDATE_DELAY))
		{
			// Video card in graphics mode?
			if (e->io_ports[0x3B8] & 2)
			{
				byte cga_mode = e->mem[0x4AC];

				// If we don't already have an SDL window open, set it up and compute color and video memory translation tables
				if (!sdl_screen)
				{
					if (cga_mode) // CGA?
					{
						for (int i = 0; i < 16; i++)
							e->pixel_colors[i] = cga_colors[(i & 12) >> 2] + (cga_colors[i & 3] << 16); // CGA -> RGB332	
					}
					else
					{
						for (int i = 0; i < 16; i++)
							e->pixel_colors[i] = 0xFF*(((i & 1) << 24) + ((i & 2) << 15) + ((i & 4) << 6) + ((i & 8) >> 3)); // Hercules -> RGB332
					}

					for (int i = 0; i < e->GRAPHICS_X * e->GRAPHICS_Y / 4; i++)
						vid_addr_lookup[i] = i / e->GRAPHICS_X * (e->GRAPHICS_X / 8) + (i / 2) % (e->GRAPHICS_X / 8) + 0x2000*(e->mem[0x4AC] ? (2 * i / e->GRAPHICS_X) % 2 : (4 * i / e->GRAPHICS_X) % 4);

					SDL_Init(SDL_INIT_VIDEO);
					sdl_window = SDL_CreateWindow(cga_mode ? "VirtualXT (CGA)" : "VirtualXT (Hercules)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, e->GRAPHICS_X, e->GRAPHICS_Y, 0);
					sdl_screen = SDL_CreateRGBSurface(0, e->GRAPHICS_X, e->GRAPHICS_Y, 8, 0xE0, 0x1C, 0x3, 0x0);

					//SDL_EnableUNICODE(1);
					//SDL_EnableKeyRepeat(500, 30);
				}

				// Refresh SDL display from emulated graphics card video RAM
				e->vid_mem_base = e->mem + 0xB0000 + 0x8000*(cga_mode ? 1 : e->io_ports[0x3B8] >> 7); // B800:0 for CGA/Hercules bank 2, B000:0 for Hercules bank 1
				for (int i = 0; i < e->GRAPHICS_X * e->GRAPHICS_Y / 4; i++)
					((unsigned *)sdl_screen->pixels)[i] = e->pixel_colors[15 & (e->vid_mem_base[vid_addr_lookup[i]] >> 4*!(i & 1))];

				SDL_BlitSurface(sdl_screen, NULL, SDL_GetWindowSurface(sdl_window), NULL);
				SDL_UpdateWindowSurface(sdl_window);
			}
			else if (sdl_screen) // Application has gone back to text mode, so close the SDL window
			{
				SDL_FreeSurface(sdl_screen);
				SDL_DestroyWindow(sdl_window);
				SDL_QuitSubSystem(SDL_INIT_VIDEO);
				
				sdl_screen = 0;
				sdl_window = 0;
			}
			SDL_PumpEvents();
		}
#endif

		// Application has set trap flag, so fire INT 1
		if (e->trap_flag)
			pc_interrupt(e, 1);

		e->trap_flag = e->regs8[FLAG_TF];

		// If a timer tick is pending, interrupts are enabled, and no overrides/REP are active,
		// then process the tick and check for new keystrokes
		if (e->int8_asap && !e->seg_override_en && !e->rep_override_en && e->regs8[FLAG_IF] && !e->regs8[FLAG_TF])
			pc_interrupt(e, 0xA), e->int8_asap = 0, SDL_KEYBOARD_DRIVER;
	}

#ifndef NO_GRAPHICS
	SDL_Quit();
#endif
	return 0;
}
