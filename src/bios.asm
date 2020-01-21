; BIOS source for VirtualXT IBM PC/XT emulator.
; Copyright (c) 2013-2014 Adrian Cable (adrian.cable@gmail.com)
; Copyright (c) 2019-2020 Andreas T Jonsson (mail@andreasjonsson.se)
;
; This work is licensed under the MIT License. See included LICENSE file.
;
; Compiles with NASM.

	cpu	8086

; Here we define macros for some custom instructions that help the emulator talk with the outside
; world. They are described in detail in the hint.html file, which forms part of the emulator
; distribution.

%macro	extended_emuctl 0
	db	0x0f, 0x00
%endmacro

%macro	extended_debug 0
	db	0x0f, 0x01
%endmacro

%macro	extended_get_rtc 0
	db	0x0f, 0x02
%endmacro

%macro	extended_read_disk 0
	db	0x0f, 0x03
%endmacro

%macro	extended_write_disk 0
	db	0x0f, 0x04
%endmacro

%macro	extended_serial_com 0
	db	0x0f, 0x05
%endmacro

org	100h				; BIOS loads at offset 0x0100

main:

	jmp	bios_entry

	dw cga_font 	; CGA font lookup for emulator.

; These values (BIOS ID string, BIOS date and so forth) go at the very top of memory
biosstr	db	'VirtualXT BIOS Revision 1', 0, 0, 0, 0, 0
mem_top	db	0xea, 0, 0x01, 0, 0xf0, '14/01/20', 0, 0xfe, 0

bios_entry:

	; Set up initial stack to F000:F000

	mov	sp, 0xf000
	mov	ss, sp

	push	cs
	pop	es

	push	ax

	; The emulator requires a few control registers in memory to always be zero for correct
	; instruction decoding (in particular, register look-up operations). These are the
	; emulator's zero segment (ZS) and always-zero flag (XF). Because the emulated memory
	; space is uninitialised, we need to be sure these values are zero before doing anything
	; else. The instructions we need to use to set them must not rely on look-up operations.
	; So e.g. MOV to memory is out but string operations are fine.

	cld

	xor	ax, ax
	mov	di, 24
	stosw			; Set ZS = 0
	mov	di, 49
	stosb			; Set XF = 0

	; Now we can do whatever we want! DL starts off being the boot disk.

	mov	[cs:boot_device], dl

	; Set up Hercules graphics support. We start with the adapter in text mode

	push	dx

	mov	dx, 0x3b8
	mov	al, 0
	out	dx, al		; Set Hercules support to text mode

	mov	dx, 0x3b4
	mov	al, 1		; Hercules CRTC "horizontal displayed" register select
	out	dx, al
	mov	dx, 0x3b5
	mov	al, 0x2d	; 0x2D = 45 (* 16) = 720 pixels wide (GRAPHICS_X)
	out	dx, al
	mov	dx, 0x3b4
	mov	al, 6		; Hercules CRTC "vertical displayed" register select
	out	dx, al
	mov	dx, 0x3b5
	mov	al, 0x57	; 0x57 = 87 (* 4) = 348 pixels high (GRAPHICS_Y)
	out	dx, al

	pop	dx

	pop	ax

	; Check cold boot/warm boot. We initialise disk parameters on cold boot only

	cmp	byte [cs:boot_state], 0	; Cold boot?
	jne	boot

	mov	byte [cs:boot_state], 1	; Set flag so next boot will be warm boot

	; First, set up the disk subsystem. Only do this on the very first startup, when
	; the emulator sets up the CX/AX registers with disk information.

	; Compute the cylinder/head/sector count for the HD disk image, if present.
	; Total number of sectors is in CX:AX, or 0 if there is no HD image. First,
	; we put it in DX:CX.

	mov	dx, cx
	mov	cx, ax

	mov	[cs:hd_secs_hi], dx
	mov	[cs:hd_secs_lo], cx

	cmp	cx, 0
	je	maybe_no_hd

	mov	word [cs:num_disks], 2
	jmp	calc_hd

maybe_no_hd:

	cmp	dx, 0
	je	no_hd

	mov	word [cs:num_disks], 2
	jmp	calc_hd

no_hd:

	mov	word [cs:num_disks], 1

calc_hd:

	mov	ax, cx
	mov	word [cs:hd_max_track], 1
	mov	word [cs:hd_max_head], 1

	cmp	dx, 0		; More than 63 total sectors? If so, we have more than 1 track.
	ja	sect_overflow
	cmp	ax, 63
	ja	sect_overflow

	mov	[cs:hd_max_sector], ax
	jmp	calc_heads

sect_overflow:

	mov	cx, 63		; Calculate number of tracks
	div	cx
	mov	[cs:hd_max_track], ax
	mov	word [cs:hd_max_sector], 63

calc_heads:

	mov	dx, 0		; More than 1024 tracks? If so, we have more than 1 head.
	mov	ax, [cs:hd_max_track]
	cmp	ax, 1024
	ja	track_overflow
	
	jmp	calc_end

track_overflow:

	mov	cx, 1024
	div	cx
	mov	[cs:hd_max_head], ax
	mov	word [cs:hd_max_track], 1024

calc_end:

	; Convert number of tracks into maximum track (0-based) and then store in INT 41
	; HD parameter table

	mov	ax, [cs:hd_max_head]
	mov	[cs:int41_max_heads], al
	mov	ax, [cs:hd_max_track]
	mov	[cs:int41_max_cyls], ax
	mov	ax, [cs:hd_max_sector]
	mov	[cs:int41_max_sect], al

	dec	word [cs:hd_max_track]
	dec	word [cs:hd_max_head]
	
; Main BIOS entry point. Zero the flags, and set up registers.

boot:	mov	ax, 0
	push	ax
	popf

	push	cs
	push	cs
	pop	ds
	pop	ss
	mov	sp, 0xf000
	
; Set up the IVT. First we zero out the table

	cld

	xor	ax, ax
	mov	es, ax
	xor	di, di
	mov	cx, 512
	rep	stosw

; Then we load in the pointers to our interrupt handlers

	mov	di, 0
	mov	si, int_table
	mov	cx, [itbl_size]
	rep	movsb

; Set pointer to INT 41 table for hard disk

	mov	cx, int41
	mov	word [es:4*0x41], cx
	mov	cx, 0xf000
	mov	word [es:4*0x41 + 2], cx

; Set up last 16 bytes of memory, including boot jump, BIOS date, machine ID byte

	mov	ax, 0xffff
	mov	es, ax
	mov	di, 0
	mov	si, mem_top
	mov	cx, 16
	rep	movsb

; Set up the BIOS data area

	mov	ax, 0x40
	mov	es, ax
	mov	di, 0
	mov	si, bios_data
	mov	cx, 0x100
	rep	movsb

; Clear video memory

	mov	ax, 0xb800
	mov	es, ax
	mov	di, 0
	mov	cx, 80*25
	mov	ax, 0x0700
	rep	stosw

; Set up some I/O ports, between 0 and FFF. Most of them we set to 0xFF, to indicate no device present

	mov	dx, 0x61
	mov	al, 0
	out	dx, al		; Make sure the speaker is off

	mov	dx, 0x60
	out	dx, al		; No scancode

	mov	dx, 0x64
	out	dx, al		; No key waiting

	mov	dx, 0
	mov	al, 0xFF

next_out:

	inc	dx

	cmp	dx, 0x40	; We deal with the PIT channel 0 later
	je	next_out
	cmp	dx, 0x42	; We deal with the PIT channel 2 later
	je	next_out
	cmp	dx, 0x3B8	; We deal with the Hercules port later, too
	je	next_out
	cmp	dx, 0x60	; Keyboard scancode
	je	next_out
	cmp	dx, 0x61	; Sound output
	je	next_out
	cmp	dx, 0x64	; Keyboard status
	je	next_out
	cmp	dx, 0x201	; Joystick input
	je	next_out

	out	dx, al

	cmp	dx, 0xFFF
	jl	next_out

	mov	al, 0

	mov	dx, 0x3DA	; CGA refresh port
	out	dx, al

	mov	dx, 0x3BA	; Hercules detection port
	out	dx, al

	mov	dx, 0x3B8	; Hercules video mode port
	out	dx, al

	mov	dx, 0x3BC	; LPT1
	out	dx, al

	mov	dx, 0x62	; PPI - needed for memory parity checks
	out	dx, al

; Get initial RTC value

	push	cs
	pop	es
	mov	bx, timetable
	extended_get_rtc
	mov	ax, [es:tm_msec]
	mov	[cs:last_int8_msec], ax

; Read boot sector from FDD, and load it into 0:7C00

	mov	ax, 0
	mov	es, ax

	mov	ax, 0x0201
	mov	dh, 0
	mov	dl, [cs:boot_device]
	mov	cx, 1
	mov	bx, 0x7c00
	int	13h

; Jump to boot sector

	jmp	0:0x7c00

; ************************* INT 7h handler - keyboard driver (VirtualXT internal)

int1d:	; Whenever the user presses a key, INT 7 is called by the emulator.
	; Scancode is at 0040:this_keystroke and ASCII at 0040:this_keystroke_ascii

	push	ds
	push	es
	push	ax
	push	bx
	push	cx
	push	bp

	push	cs
	pop	ds

	mov	bx, 0x40	; Set segment to BIOS data area segment (0x40)
	mov	es, bx

	; Retrieve the keystroke

	mov	al, [es:this_keystroke-bios_data]
	mov	ch, al 				; Save scancode to CH

	and al, 0x7f 			; Remove key up bit
	mov bl, al 				; Store masked scancode in BL

	mov al, ch
	and	al, 0x80 			; Key up mask
	xor al, 0x80 			; Flip to key down
	mov cl, al 				; Store state mask in CL

	cmp	bl, 0x2A 			; LShift?
	je process_shift_key
	cmp	bl, 0x36 			; RShift?
	je process_shift_key
	cmp	bl, 0x38 			; Alt?
	je process_alt_key
	cmp	bl, 0x1d 			; Ctrl?
	je process_ctrl_key
	cmp	bl, 0x3a 			; CapsLock?
	je process_capslock_key
	cmp	bl, 0x45 			; NumLock?
	je process_numlock_key
	cmp	bl, 0x46 			; ScrollLock?
	je process_scrlock_key
	cmp	bl, 0x52 			; Insert?
	je process_insert_key

	jmp send_key_press

  process_shift_key:

	mov al, cl
	cpu	186
	shr	al, 7
	cpu	8086
	mov ah, al

	mov al, [es:keyflags1-bios_data]
	and al, 0xFE

	jmp set_key_flag

  process_alt_key:

  	mov al, cl
	cpu	186
	shr	al, 4
	cpu	8086
	mov ah, al

	mov al, [es:keyflags1-bios_data]
	and al, 0xF7

	jmp set_key_flag

  process_ctrl_key:

    mov al, cl
	cpu	186
	shr	al, 5
	cpu	8086
	mov ah, al

	mov al, [es:keyflags1-bios_data]
	and al, 0xFB

	jmp set_key_flag

  process_insert_key:

	mov cl, [es:keyflags1-bios_data]

	test cl, 0x20
	jne send_key_press 		; Jump if NumLock is on.

	cmp cl, 0
	jnz send_key_press 		; Toggle on key up.

	mov al, cl
	and al, 0x80
	xor al, 0x80
	mov ah, al				; Save toggle state in ah.

	mov al, cl
	and al, 0x7F 			; Save the other states in al.

	jmp set_key_flag

  process_capslock_key:

	cmp cl, 0
	jnz send_key_press 		; Toggle on key up.

	mov cl, [es:keyflags1-bios_data]

	mov al, cl
	and al, 0x40
	xor al, 0x40
	mov ah, al				; Save toggle state in ah.

	mov al, cl
	and al, 0xBF 			; Save the other states in al.

	jmp set_key_flag

  process_numlock_key:

	cmp cl, 0
	jnz send_key_press 		; Toggle on key up.

	mov cl, [es:keyflags1-bios_data]

	mov al, cl
	and al, 0x20
	xor al, 0x20
	mov ah, al				; Save toggle state in ah.

	mov al, cl
	and al, 0xDF 			; Save the other states in al.

	jmp set_key_flag

  process_scrlock_key:

	cmp cl, 0
	jnz send_key_press 		; Toggle on key up.

	mov cl, [es:keyflags1-bios_data]

	mov al, cl
	and al, 0x10
	xor al, 0x10
	mov ah, al				; Save toggle state in ah.

	mov al, cl
	and al, 0xEF 			; Save the other states in al.

	jmp set_key_flag

  set_key_flag:

	or al, ah	
	mov [es:keyflags1-bios_data], al

  send_key_press:

  	mov	al, ch 			; Restore scancode

	call io_key_available

	pop	bp
	pop	cx
	pop	bx
	pop	ax
	pop	es
	pop	ds
	iret

; ************************* INT 9h handler - keyboard (PC BIOS standard)

int9:
	push	es
	push	ax
	push	bx
	push	bp

	in	al, 0x60

	cmp	al, 0x80 ; Key up?
	jae	no_add_buf
	cmp	al, 0x2A ; LShift?
	je	no_add_buf
	cmp	al, 0x36 ; RShift?
	je	no_add_buf
	cmp	al, 0x38 ; Alt?
	je	no_add_buf
	cmp	al, 0x1d ; Ctrl?
	je	no_add_buf
	cmp	bl, 0x3a ; CapsLock?
	je no_add_buf
	cmp	bl, 0x45 ; NumLock?
	je no_add_buf
	cmp	bl, 0x46 ; ScrollLock?
	je no_add_buf

	mov	bx, 0x40
	mov	es, bx

	; Tail of the BIOS keyboard buffer goes in BP. This is where we add new keystrokes

	mov	ah, [es:this_keystroke_ascii-bios_data]

	mov	bp, [es:kbbuf_tail-bios_data]
	mov	byte [es:bp], ah 					; ASCII code
	mov	byte [es:bp+1], al 					; Scan code

	; ESC keystroke is in the buffer now
	add	word [es:kbbuf_tail-bios_data], 2
	call	kb_adjust_buf ; Wrap the tail around the head if the buffer gets too large

  no_add_buf:

	mov	al, 1
	out	0x64, al

	pop	bp
	pop	bx
	pop	ax
	pop	es

	iret

; ************************* INT Ah handler - timer (VirtualXT internal)

inta:
	; VirtualXT called interrupt 0xA frequently, at a rate dependent on the speed of your computer.
	; This interrupt handler scales down the call rate and calls INT 8 at 18.2 times per second,
	; as per a real PC.

	; See if there is an ESC waiting from a previous INT 7h. If so, put it in the keyboard buffer
	; (because by now - 1/18.2 secs on - we know it can't be part of an escape key sequence).
	; Also handle CGA refresh register. Also release any keys that are still marked as down.

	push	ax
	push	bx
	push	dx
	push	bp
	push	es

	push	cx
	push	di
	push	ds
	push	si

	; Increment 32-bit BIOS timer tick counter, once every 18.2 ms

	push	cs
	pop	es
	mov	bx, timetable
	extended_get_rtc
	
	mov	ax, [cs:tm_msec]
	sub	ax, [cs:last_int8_msec]

  make_ctr_positive:

	cmp	ax, 0
	jge	no_add_1000

	add	ax, 1000
	jmp	make_ctr_positive

  no_add_1000:

	mov	bx, 0x40
	mov	es, bx

	mov	dx, 0
	mov	bx, 1193
	mul	bx

	mov	bx, [es:timer0_freq-bios_data]

	cmp	bx, 0 ; 0 actually means FFFF
	jne	no_adjust_10000

	mov	bx, 0xffff

  no_adjust_10000:

	div	bx ; AX now contains number of timer ticks since last int 8 (DX is remainder)

	cmp	ax, 0
	je	i8_end

	add	word [es:0x6C], ax
	adc	word [es:0x6E], 0

inta_call_int8:

	push	ax	; Workaround for CPM-86 - INT 1C destroys AX!!
	int	8
	pop	ax

	dec	ax
	cmp	ax, 0
	jne	inta_call_int8

	mov	ax, [cs:tm_msec]
	mov	[cs:last_int8_msec], ax

i8_end:	

	; A Hercules graphics adapter flips bit 7 of I/O port 3BA on refresh
	mov	dx, 0x3BA
	in 	al, dx
	xor	al, 0x80
	out	dx, al

	pop	si
	pop	ds
	pop	di
	pop	cx
	
	pop	es
	pop	bp
	pop	dx
	pop	bx
	pop	ax

	iret

; ************************* INT 8h handler - timer

int8:

	int	0x1c
	iret

; ************************* INT 10h handler - video services

int10:

	cmp	ah, 0x00 ; Set video mode
	je	int10_set_vm
	cmp	ah, 0x01 ; Set cursor shape
	je	int10_set_cshape
	cmp	ah, 0x02 ; Set cursor position
	je	int10_set_cursor
	cmp	ah, 0x03 ; Get cursur position
	je	int10_get_cursor
	cmp	ah, 0x06 ; Scroll up window
	je	int10_scrollup
	cmp	ah, 0x07 ; Scroll down window
	je	int10_scrolldown
	cmp	ah, 0x08 ; Get character at cursor
	je	int10_charatcur
	cmp	ah, 0x09 ; Write char and attribute
	je	int10_write_char_attrib
	cmp	ah, 0x0e ; Write character at cursor position
	je	int10_write_char
	cmp	ah, 0x0f ; Get video mode
	je	int10_get_vm
	; cmp	ah, 0x1a ; Feature check
	; je	int10_features

	iret

  int10_set_vm:

	push	dx
	push	cx
	push	bx
	push	es

	cmp	al, 4 ; CGA mode 4
	je	int10_switch_to_cga_gfx
	cmp	al, 5
	je	int10_switch_to_cga_gfx
	cmp	al, 6
	je	int10_switch_to_cga_gfx

	push	ax

	mov	dx, 0x3b8
	mov	al, 0
	out	dx, al

	mov	dx, 0x3b4
	mov	al, 1		; Hercules CRTC "horizontal displayed" register select
	out	dx, al
	mov	dx, 0x3b5
	mov	al, 0x2d	; 0x2D = 45 (* 16) = 720 pixels wide (GRAPHICS_X)
	out	dx, al
	mov	dx, 0x3b4
	mov	al, 6		; Hercules CRTC "vertical displayed" register select
	out	dx, al
	mov	dx, 0x3b5
	mov	al, 0x57	; 0x57 = 87 (* 4) = 348 pixels high (GRAPHICS_Y)
	out	dx, al

	mov	dx, 0x40
	mov	es, dx

	mov	byte [es:0xac], 0 ; Tell emulator we are in Hercules mode

	pop	ax

	cmp	al, 7		; If an app tries to set Hercules text mode 7, actually set mode 3 (we do not support mode 7's video memory buffer at B000:0)
	je	int10_set_vm_3
	cmp	al, 2		; Same for text mode 2 (mono)
	je	int10_set_vm_3

	jmp	int10_set_vm_continue

  int10_switch_to_cga_gfx:

	; Switch to CGA-like graphics mode (with Hercules CRTC set for 640 x 400)
	
	mov	dx, 0x40
	mov	es, dx

	mov	[es:0x49], al	; Current video mode
	mov	byte [es:0xac], 1 ; Tell emulator we are in CGA mode

	mov	dx, 0x3b4
	mov	al, 1		; Hercules CRTC "horizontal displayed" register select
	out	dx, al
	mov	dx, 0x3b5
	mov	al, 0x28	; 0x28 = 40 (* 16) = 640 pixels wide (GRAPHICS_X)
	out	dx, al
	mov	dx, 0x3b4
	mov	al, 6		; Hercules CRTC "vertical displayed" register select
	out	dx, al
	mov	dx, 0x3b5
	mov	al, 0x64	; 0x64 = 100 (* 4) = 400 pixels high (GRAPHICS_Y)
	out	dx, al

	mov	dx, 0x3b8
	mov	al, 0x8a
	out	dx, al

	mov	bh, 7	
	call clear_screen

	mov	ax, 0x30
	jmp	svmn_exit

  int10_set_vm_3:

	mov	al, 3

  int10_set_vm_continue:

	mov	bx, 0x40
	mov	es, bx

	mov	[es:vidmode-bios_data], al

	mov	bh, 7		; Black background, white foreground
	call clear_screen

	cmp	byte [es:vidmode-bios_data], 6
	je	set6
	mov	al, 0x30
	jmp	svmn

  set6:

	mov	al, 0x3f

  svmn:

	; Take Hercules adapter out of graphics mode when resetting video mode via int 10
	push	ax
	mov	dx, 0x3B8
	mov	al, 0
	out	dx, al
	pop	ax

  svmn_exit:

	pop	es
	pop	bx
	pop	cx
	pop	dx
	iret

  int10_set_cshape:

	push	ds
	push	ax
	push	cx

	mov	ax, 0x40
	mov	ds, ax

	mov	byte [cursor_visible-bios_data], 1	; Show cursor

	and	ch, 01100000b
	cmp	ch, 00100000b
	jne	cur_done

	mov	byte [cursor_visible-bios_data], 0	; Hide cursor

    cur_done:

	pop	cx
	pop	ax
	pop	ds
	iret

  int10_set_cursor:

	push	ds
	push	ax

	mov	ax, 0x40
	mov	ds, ax

	mov	[curpos_y-bios_data], dh
	mov	[crt_curpos_y-bios_data], dh
	mov	[curpos_x-bios_data], dl
	mov	[crt_curpos_x-bios_data], dl

	pop	ax
	pop	ds
	iret

  int10_get_cursor:

	push	es

	mov	cx, 0x40
	mov	es, cx

	mov	cx, 0x0607
	mov	dl, [es:curpos_x-bios_data]
	mov	dh, [es:curpos_y-bios_data]

	pop	es

	iret

  int10_scrollup:

	cmp	al, 0 ; Clear window
	jne	cls_partial

	cmp	cx, 0 ; Start of screen
	jne	cls_partial

	cmp	dl, 0x4f ; Clearing columns 0-79
	jb	cls_partial

	cmp	dh, 0x18 ; Clearing rows 0-24 (or more)
	jb	cls_partial

	call	clear_screen
	iret

	cls_partial:

	push	bx
	push	ax

	push	ds
	push	es
	push	cx
	push	dx
	push	si
	push	di

	push	bx

	mov	bx, 0xb800
	mov	es, bx
	mov	ds, bx

	pop	bx
	mov	bl, al

    cls_vmem_scroll_up_next_line:

	cmp	bl, 0
	je	cls_vmem_scroll_up_done

    cls_vmem_scroll_up_one:

	push	bx
	push	dx

	mov	ax, 0
	mov	al, ch		; Start row number is now in AX
	mov	bx, 80
	mul	bx
	add	al, cl
	adc	ah, 0		; Character number is now in AX
	mov	bx, 2
	mul	bx		; Memory location is now in AX

	pop	dx
	pop	bx

	mov	di, ax
	mov	si, ax
	add	si, 2*80	; In a moment we will copy CX words from DS:SI to ES:DI

	mov	ax, 0
	add	al, dl
	adc	ah, 0
	inc	ax
	sub	al, cl
	sbb	ah, 0		; AX now contains the number of characters from the row to copy

	cmp	ch, dh
	jae	cls_vmem_scroll_up_one_done

	; Copy next row

	push	cx
	mov	cx, ax		; CX is now the length (in words) of the row to copy
	cld
	rep	movsw		; Scroll the line up
	pop	cx

	inc	ch		; Move onto the next row
	jmp	cls_vmem_scroll_up_one

    cls_vmem_scroll_up_one_done:

	push	cx
	mov	cx, ax		; CX is now the length (in words) of the row to copy
	mov	ah, bh		; Attribute for new line
	mov	al, 0		; Write 0 to video memory for new characters
	cld
	rep	stosw
	pop	cx

	dec	bl		; Scroll whole text block another line
	jmp	cls_vmem_scroll_up_next_line	

    cls_vmem_scroll_up_done:

	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	es
	pop	ds

	pop	ax
	pop	bx

	iret
	
  int10_scrolldown:

  	cmp	al, 0 ; Clear window
	jne	cls_partial_down

	cmp	cx, 0 ; Start of screen
	jne	cls_partial_down

	cmp	dl, 0x4f ; Clearing columns 0-79
	jne	cls_partial_down

	cmp	dh, 0x18 ; Clearing rows 0-24 (or more)
	jl	cls_partial_down

	call	clear_screen
	iret

	cls_partial_down:

	push	ax
	push	bx

	push	ds
	push	es
	push	cx
	push	dx
	push	si
	push	di

	push	bx

	mov	bx, 0xb800
	mov	es, bx
	mov	ds, bx

	pop	bx
	mov	bl, al

    cls_vmem_scroll_down_next_line:

	cmp	bl, 0
	je	cls_vmem_scroll_down_done

    cls_vmem_scroll_down_one:

	push	bx
	push	dx

	mov	ax, 0
	mov	al, dh		; End row number is now in AX
	mov	bx, 80
	mul	bx
	add	al, cl
	adc	ah, 0		; Character number is now in AX
	mov	bx, 2
	mul	bx		; Memory location (start of final row) is now in AX

	pop	dx
	pop	bx

	mov	di, ax
	mov	si, ax
	sub	si, 2*80	; In a moment we will copy CX words from DS:SI to ES:DI

	mov	ax, 0
	add	al, dl
	adc	ah, 0
	inc	ax
	sub	al, cl
	sbb	ah, 0		; AX now contains the number of characters from the row to copy

	cmp	ch, dh
	jae	cls_vmem_scroll_down_one_done

	push	cx
	mov	cx, ax		; CX is now the length (in words) of the row to copy
	rep	movsw		; Scroll the line down
	pop	cx

	dec	dh		; Move onto the next row
	jmp	cls_vmem_scroll_down_one

    cls_vmem_scroll_down_one_done:

	push	cx
	mov	cx, ax		; CX is now the length (in words) of the row to copy
	mov	ah, bh		; Attribute for new line
	mov	al, 0		; Write 0 to video memory for new characters
	rep	stosw
	pop	cx

	dec	bl		; Scroll whole text block another line
	jmp	cls_vmem_scroll_down_next_line	

    cls_vmem_scroll_down_done:

	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	es
	pop	ds

	pop	bx
	pop	ax
	iret

  int10_charatcur:

	; This returns the character at the cursor. It is completely dysfunctional,
	; and only works at all if the character has previously been written following
	; an int 10/ah = 2 call to set the cursor position. Added just to support
	; GWBASIC.

	push	ds
	push	es
	push	bx
	push	dx

	mov	bx, 0x40
	mov	es, bx

	mov	bx, 0xb800
	mov	ds, bx

	mov	bx, 160
	mov	ax, 0
	mov	al, [es:curpos_y-bios_data]
	mul	bx

	mov	bx, 0
	mov	bl, [es:curpos_x-bios_data]
	add	ax, bx
	add	ax, bx
	mov	bx, ax

	mov	ah, 7
	mov	al, [bx]

	pop	dx
	pop	bx
	pop	es
	pop	ds

	iret

  i10_unsup:

	iret

  int10_write_char:

	push	ds
	push	es
	push	cx
	push	dx
	push	ax
	push	bp
	push	bx

	push	ax

	mov	bx, 0x40
	mov	es, bx

	mov	cl, al
	mov	ch, 7

	mov	bx, 0xb800
	mov	ds, bx

	cmp	al, 0x20
	jl	int10_write_char_skip

	cmp byte [es:vidmode-bios_data], 4
	je 	int10_write_char_cga320
	cmp byte [es:vidmode-bios_data], 5
	je 	int10_write_char_cga320

	mov	bx, 160
	mov	ax, 0
	mov	al, [es:curpos_y-bios_data]
	mul	bx

	mov	bx, 0
	mov	bl, [es:curpos_x-bios_data]
	shl	bx, 1
	add	bx, ax

	mov	[bx], cx

	jmp int10_write_char_skip

	int10_write_char_cga320:

	call put_cga320_char

	int10_write_char_skip:
	
	pop	ax
	push ax

	jmp	int10_write_char_skip_lines

  int10_write_char_attrib:

	push	ds
	push	es
	push	cx
	push	dx
	push	ax
	push	bp
	push	bx
	
	push	ax

	mov	dl, al
	mov	dh, bl

	mov	bx, 0x40
	mov	es, bx

	mov	bx, 0xb800
	mov	ds, bx

	mov	bx, 160
	mov	ax, 0
	mov	al, [es:curpos_y-bios_data]
	mul	bx

	mov	bx, 0
	mov	bl, [es:curpos_x-bios_data]
	shl	bx, 1
	add	bx, ax

	int10_write_next_char_attrib:

	mov	[bx], dx
	add bx, 2

	dec	cx
	cmp	cx, 0

	jne	int10_write_next_char_attrib

    int10_write_char_skip_lines:

	pop	ax

	push es
	pop	ds

	cmp	al, 0x08
	jne	int10_write_char_attrib_inc_x

	dec	byte [curpos_x-bios_data]
	dec	byte [crt_curpos_x-bios_data]
	cmp	byte [curpos_x-bios_data], 0
	jg	int10_write_char_attrib_done

	mov	byte [curpos_x-bios_data], 0
	mov	byte [crt_curpos_x-bios_data], 0
	jmp	int10_write_char_attrib_done

    int10_write_char_attrib_inc_x:

	cmp	al, 0x0A	; New line?
	je	int10_write_char_attrib_newline

	cmp	al, 0x0D	; Carriage return?
	jne	int10_write_char_attrib_not_cr

	mov	byte [curpos_x-bios_data], 0
	mov	byte [crt_curpos_x-bios_data], 0
	jmp	int10_write_char_attrib_done

    int10_write_char_attrib_not_cr:

	inc	byte [curpos_x-bios_data]
	inc	byte [crt_curpos_x-bios_data]
	cmp	byte [curpos_x-bios_data], 80
	jge	int10_write_char_attrib_newline
	jmp	int10_write_char_attrib_done

    int10_write_char_attrib_newline:

	mov	byte [curpos_x-bios_data], 0
	mov	byte [crt_curpos_x-bios_data], 0
	inc	byte [curpos_y-bios_data]
	inc	byte [crt_curpos_y-bios_data]

	cmp	byte [curpos_y-bios_data], 25
	jb	int10_write_char_attrib_done
	mov	byte [curpos_y-bios_data], 24
	mov	byte [crt_curpos_y-bios_data], 24

	mov	bh, 7
	mov	al, 1
	mov	cx, 0
	mov	dx, 0x184f

	pushf
	push	cs
	call	int10_scrollup

    int10_write_char_attrib_done:

	pop	bx
	pop	bp
	pop	ax
	pop	dx
	pop	cx
	pop	es
	pop	ds

	iret

  ; Larg parts of this function is from Julian Olds "plus" modifications to 8086tiny.

  put_cga320_char:
	; Character is in AL
	; Colour is in AH
	push	ax
	push	bx
	push	cx
	push	ds	
	push	es
	push	di
	push	bp

	; Get the colour mask into BH
	cmp	ah, 1
	jne	put_cga320_char_c2
	mov	bh, 0x55
	jmp	put_cga320_char_cdone
	put_cga320_char_c2:
	cmp	ah, 2
	jne	put_cga320_char_c3
	mov	bh, 0xAA
	jmp	put_cga320_char_cdone
	put_cga320_char_c3:
	mov	bh, 0xFF

	put_cga320_char_cdone:
	; Get glyph character top offset into bp and character segment into cx
	test	al, 0x80
	jne	put_cga320_char_high

	; Characters 0 .. 127 are always in ROM
	mov	ah, 0
	shl	ax, 1
	shl	ax, 1
	shl	ax, 1
	add	ax, cga_font
	mov	bp, ax

	mov	cx, cs

	jmp	put_cga320_char_vidoffset

	put_cga320_char_high:
	; Characters 128 .. 255 get their address from interrupt vector 1F
	and	al, 0x7F
	mov	ah, 0
	shl	ax, 1
	shl	ax, 1
	shl	ax, 1
	mov	bp, ax

	mov	ax, 0
	mov	ds, ax
	mov	ax, [ds:0x7c]
	add	bp, ax 

	mov	cx, [ds:0x7e]

	put_cga320_char_vidoffset:
	mov	ax, 0x40
	mov	ds, ax

	; Get the address offset in video ram for the top of the character into DI
	mov	al, 80 ; bytes per row
	mul	byte [ds:curpos_y-bios_data]
	shl	ax, 1
	shl	ax, 1
	add	al, [ds:curpos_x-bios_data]
	adc	ah, 0
	add	al, [ds:curpos_x-bios_data]
	adc	ah, 0
	mov	di, ax

	; get segment for character data into ds
	mov	ds, cx

	; get video RAM address for even lines into es
	mov	ax, 0xb800
	mov	es, ax

	push	di

	mov	bl, byte [ds:bp]
	; Translate character glyph into CGA 320 format
	call	put_cga320_char_double
	stosw
	add	di, 78

	mov	bl, byte [ds:bp+2]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw
	add	di, 78

	mov	bl, byte [ds:bp+4]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw
	add	di, 78

	mov	bl, byte [ds:bp+6]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw

	; get video RAM address for odd lines into es
	mov ax, 0xba00
	mov es, ax

	pop	di

	mov	bl, byte [ds:bp+1]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw
	add	di, 78

	mov	bl, byte [ds:bp+3]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw
	add	di, 78

	mov	bl, byte [ds:bp+5]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw
	add	di, 78

	mov	bl, byte [ds:bp+7]
	; Translate character glyph into CGA 320 format
	call put_cga320_char_double
	stosw	

	put_cga320_char_done:

	pop	bp
	pop	di
	pop	es
	pop	ds
	pop	cx
	pop	bx
	pop	ax
	ret

  put_cga320_char_double:
	; BL = character bit pattern
	; BH = colour mask
	; AX is set to double width character bit pattern

	mov	ax, 0
	test	bl, 0x80
	je	put_chachar_bit6
	or	al, 0xc0
	put_chachar_bit6:
	test	bl, 0x40
	je	put_chachar_bit5
	or	al, 0x30
	put_chachar_bit5:
	test	bl, 0x20
	je	put_chachar_bit4
	or	al, 0x0c
	put_chachar_bit4:
	test	bl, 0x10
	je	put_chachar_bit3
	or	al, 0x03
	put_chachar_bit3:
	test	bl, 0x08
	je	put_chachar_bit2
	or	ah, 0xc0
	put_chachar_bit2:
	test	bl, 0x04
	je	put_chachar_bit1
	or	ah, 0x30
	put_chachar_bit1:
	test	bl, 0x02
	je	put_chachar_bit0
	or	ah, 0x0c
	put_chachar_bit0:
	test	bl, 0x01
	je	put_chachar_done
	or	ah, 0x03
	put_chachar_done:
	and	al, bh
	and	ah, bh
	ret

  int10_get_vm:

	push	es

	mov	ax, 0x40
	mov	es, ax

	mov	ah, 80 ; Number of columns
	mov	al, [es:vidmode-bios_data]
	mov	bh, 0

	pop	es

	iret

  int10_features:

	; Signify we have CGA display

	; mov	al, 0x1a
	; mov	bx, 0x0202
	; iret

; ************************* INT 11h - get equipment list

int11:	
	mov	ax, [cs:equip]
	iret

; ************************* INT 12h - return memory size

int12:	
	mov	ax, 0x280 ; 640K conventional memory
	iret

; ************************* INT 13h handler - disk services

int13:
	cmp	ah, 0x00 ; Reset disk
	je	int13_reset_disk
	cmp	ah, 0x01 ; Get last status
	je	int13_last_status

	cmp	dl, 0x80 ; Hard disk being queried?
	jne	i13_diskok

	; Now, need to check an HD is installed
	cmp	word [cs:num_disks], 2
	jge	i13_diskok

	; No HD, so return an error
	mov	ah, 15 ; Report no such drive
	jmp	reach_stack_stc

  i13_diskok:

	cmp	ah, 0x02 ; Read disk
	je	int13_read_disk
	cmp	ah, 0x03 ; Write disk
	je	int13_write_disk
	cmp	ah, 0x04 ; Verify disk
	je	int13_verify
	cmp	ah, 0x05 ; Format track - does nothing here
	je	int13_format
	cmp	ah, 0x08 ; Get drive parameters (hard disk)
	je	int13_getparams
	cmp	ah, 0x0c ; Seek (hard disk)
	je	int13_seek
	cmp	ah, 0x10 ; Check if drive ready (hard disk)
	je	int13_hdready
	cmp	ah, 0x15 ; Get disk type
	je	int13_getdisktype
	cmp	ah, 0x16 ; Detect disk change
	je	int13_diskchange

	mov	ah, 1 ; Invalid function
	jmp	reach_stack_stc

	iret

  int13_reset_disk:

	jmp	reach_stack_clc

  int13_last_status:

	mov	ah, [cs:disk_laststatus]
	je	ls_no_error

	stc
	iret

    ls_no_error:

	clc
	iret

  int13_read_disk:

	push	dx

	cmp	dl, 0 ; Floppy 0
	je	i_flop_rd
	cmp	dl, 0x80 ; HD
	je	i_hd_rd

	pop	dx
	mov	ah, 1
	jmp	reach_stack_stc

    i_flop_rd:

	push	si
	push	bp

	cmp	cl, [cs:int1e_spt]
	ja	rd_error

	pop	bp
	pop	si

	mov	dl, 1		; Floppy disk file handle is stored at j[1] in emulator
	jmp	i_rd

    i_hd_rd:

	mov	dl, 0		; Hard disk file handle is stored at j[0] in emulator

    i_rd: 

	push	si
	push	bp

	; Convert head/cylinder/sector number to byte offset in disk image

	call	chs_to_abs

	; Now, SI:BP contains the absolute sector offset of the block. We then multiply by 512 to get the offset into the disk image

	mov	ah, 0
	cpu	186
	shl	ax, 9
	extended_read_disk
	shr	ax, 9
	cpu	8086
	mov	ah, 0x02	; Put read code back

	cmp	al, 0
	je	rd_error

	; Read was successful. Now, check if we have read the boot sector. If so, we want to update
	; our internal table of sectors/track to match the disk format

	cmp	dx, 1		; FDD?
	jne	rd_noerror
	cmp	cx, 1		; First sector?
	jne	rd_noerror

	push	ax

	mov	al, [es:bx+24]	; Number of SPT in floppy disk BPB

	; cmp	al, 0		; If disk is unformatted, do not update the table
	; jne	rd_update_spt
	cmp	al, 9		; 9 SPT, i.e. 720K disk, so update the table
	je	rd_update_spt
	cmp	al, 18
	je	rd_update_spt	; 18 SPT, i.e. 1.44MB disk, so update the table

	pop	ax

	jmp	rd_noerror

    rd_update_spt:

	mov	[cs:int1e_spt], al
	pop	ax

    rd_noerror:

	clc
	mov	ah, 0 ; No error
	jmp	rd_finish

    rd_error:

	stc
	mov	ah, 4 ; Sector not found

    rd_finish:

	pop	bp
	pop	si
	pop	dx

	mov	[cs:disk_laststatus], ah
	jmp	reach_stack_carry

  int13_write_disk:

	push	dx

	cmp	dl, 0 ; Floppy 0
	je	i_flop_wr
	cmp	dl, 0x80 ; HD
	je	i_hd_wr

	pop	dx
	mov	ah, 1
	jmp	reach_stack_stc

    i_flop_wr:

	mov	dl, 1		; Floppy disk file handle is stored at j[1] in emulator
	jmp	i_wr

    i_hd_wr:

	mov	dl, 0		; Hard disk file handle is stored at j[0] in emulator

    i_wr:

	push	si
	push	bp
	push	cx
	push	di

	; Convert head/cylinder/sector number to byte offset in disk image

	call	chs_to_abs

	; Signal an error if we are trying to write beyond the end of the disk
	
	cmp	dl, 0 ; Hard disk?
	jne	wr_fine ; No - no need for disk sector valid check - NOTE: original submission was JNAE which caused write problems on floppy disk

	; First, we add the number of sectors we are trying to write from the absolute
	; sector number returned by chs_to_abs. We need to have at least this many
	; sectors on the disk, otherwise return a sector not found error.

	mov	cx, bp
	mov	di, si

	mov	ah, 0
	add	cx, ax
	adc	di, 0

	cmp	di, [cs:hd_secs_hi]
	ja	wr_error
	jb	wr_fine
	cmp	cx, [cs:hd_secs_lo]
	ja	wr_error

wr_fine:

	mov	ah, 0
	cpu	186
	shl	ax, 9
	extended_write_disk
	shr	ax, 9
	cpu	8086
	mov	ah, 0x03	; Put write code back

	cmp	al, 0
	je	wr_error

	clc
	mov	ah, 0 ; No error
	jmp	wr_finish

    wr_error:

	stc
	mov	ah, 4 ; Sector not found

    wr_finish:

	pop	di
	pop	cx
	pop	bp
	pop	si
	pop	dx

	mov	[cs:disk_laststatus], ah
	jmp	reach_stack_carry

  int13_verify:

	mov	ah, 0
	jmp	reach_stack_clc

  int13_getparams:

	cmp 	dl, 0
	je	i_gp_fl
	cmp	dl, 0x80
	je	i_gp_hd

	mov	ah, 0x01
	mov	[cs:disk_laststatus], ah
	jmp	reach_stack_stc

    i_gp_fl:

	push	cs
	pop	es
	mov	di, int1e	; ES:DI now points to floppy parameters table (INT 1E)

	mov	ax, 0
	mov	bx, 4
	mov	ch, 0x4f
	mov	cl, [cs:int1e_spt]
	mov	dx, 0x0101

	mov	byte [cs:disk_laststatus], 0
	jmp	reach_stack_clc

    i_gp_hd:

	mov	ax, 0
	mov	bx, 0
	mov	dl, 1
	mov	dh, [cs:hd_max_head]
	mov	cx, [cs:hd_max_track]
	ror	ch, 1
	ror	ch, 1
	add	ch, [cs:hd_max_sector]
	xchg	ch, cl

	mov	byte [cs:disk_laststatus], 0
	jmp	reach_stack_clc

  int13_seek:

	mov	ah, 0
	jmp	reach_stack_clc

  int13_hdready:

	cmp	byte [cs:num_disks], 2	; HD present?
	jne	int13_hdready_nohd
	cmp	dl, 0x80		; Checking first HD?
	jne	int13_hdready_nohd

	mov	ah, 0
	jmp	reach_stack_clc

    int13_hdready_nohd:

	jmp	reach_stack_stc

  int13_format:

	mov	ah, 0
	jmp	reach_stack_clc

  int13_getdisktype:

	cmp	dl, 0 ; Floppy
	je	gdt_flop
	cmp	dl, 0x80 ; HD
	je	gdt_hd

	mov	ah, 15 ; Report no such drive
	mov	[cs:disk_laststatus], ah
	jmp	reach_stack_stc

    gdt_flop:

	mov	ah, 1
	jmp	reach_stack_clc

    gdt_hd:

	mov	ah, 3
	mov	cx, [cs:hd_secs_hi]
	mov	dx, [cs:hd_secs_lo]
	jmp	reach_stack_clc

  int13_diskchange:

	mov	ah, 0 ; Disk not changed
	jmp	reach_stack_clc

; ************************* INT 14h - serial port functions

int14:

	extended_serial_com
	jmp	reach_stack_stc

; ************************* INT 15h - get system configuration

int15:	; Here we do not support any of the functions, and just return
	; a function not supported code - like the original IBM PC/XT does.

	; cmp	ah, 0xc0
	; je	int15_sysconfig
	; cmp	ah, 0x41
	; je	int15_waitevent
	; cmp	ah, 0x4f
	; je	int15_intercept
	; cmp	ah, 0x88
	; je	int15_getextmem
	cmp	ah, 0x84
	je int15_joystick

	; Otherwise, function not supported

	jmp	int15_not_supported

  int15_joystick:

	; Check if joysticks ar present

	push ax

	mov al, 1 			; Joystick service
	mov ah, 0 			; Check number of sticks
	extended_emuctl
	cmp al, 0

	pop ax

	je int15_not_supported

	; Check service requested

	cmp	dx, 0
	je joystick_buttons
	cmp	dx, 1
	je joystick_axis

	jmp	int15_not_supported

	joystick_buttons:

	push bx
	push cx

	mov al, 1 			; Joystick service
	mov ah, 1 			; Joystick 1
	extended_emuctl
cpu 186
	shl al, 4 			; Button states
cpu 8086
	
	pop cx
	pop bx

	jmp	joystick_done

	joystick_axis:

	mov al, 1 			; Joystick service
	mov ah, 1 			; Joystick 1
	extended_emuctl
	mov ax, bx
	mov bx, cx
	mov dx, 0

	joystick_done:

	jmp reach_stack_clc

;  int15_sysconfig: ; Return address of system configuration table in ROM
;
;	mov	bx, 0xf000
;	mov	es, bx
;	mov	bx, rom_config
;	mov	ah, 0
;
;	jmp	reach_stack_clc
;
;  int15_waitevent: ; Events not supported
;
;	mov	ah, 0x86
;
;	jmp	reach_stack_stc
;
;  int15_intercept: ; Keyboard intercept
;
;	jmp	reach_stack_stc
;
;  int15_getextmem: ; Extended memory not supported
;
;	mov	ah,0x86
;
;	jmp	reach_stack_stc

  int15_not_supported:

	mov	ah, 0x86
	jmp	reach_stack_stc

; ************************* INT 16h handler - keyboard

int16:
	cmp	ah, 0x00 ; Get keystroke (remove from buffer)
	je	kb_getkey
	cmp	ah, 0x01 ; Check for keystroke (do not remove from buffer)
	je	kb_checkkey
	cmp	ah, 0x02 ; Check shift flags
	je	kb_shiftflags
	cmp	ah, 0x12 ; Check shift flags
	je	kb_extshiftflags

	iret

  kb_getkey:
	
	push	es
	push	bx
	push	cx
	push	dx

	mov	bx, 0x40
	mov	es, bx

    kb_gkblock:

	cli

	mov	cx, [es:kbbuf_tail-bios_data]
	mov	bx, [es:kbbuf_head-bios_data]
	mov	dx, [es:bx]

	sti

	; Wait until there is a key in the buffer
	cmp	cx, bx
	je	kb_gkblock

	add	word [es:kbbuf_head-bios_data], 2
	call	kb_adjust_buf

	mov	ah, dh
	mov	al, dl

	pop	dx
	pop	cx
	pop	bx
	pop	es	

	iret

  kb_checkkey:

	push	es
	push	bx
	push	cx
	push	dx

	mov	bx, 0x40
	mov	es, bx

	mov	cx, [es:kbbuf_tail-bios_data]
	mov	bx, [es:kbbuf_head-bios_data]
	mov	dx, [es:bx]

	sti

	; Check if there is a key in the buffer. ZF is set if there is none.
	cmp	cx, bx

	mov	ah, dh
	mov	al, dl

	pop	dx
	pop	cx
	pop	bx
	pop	es

	retf	2	; NEED TO FIX THIS!!

    kb_shiftflags:

	push	es
	push	bx

	mov	bx, 0x40
	mov	es, bx

	mov	al, [es:keyflags1-bios_data]

	pop	bx
	pop	es

	iret

    kb_extshiftflags:

	push	es
	push	bx

	mov	bx, 0x40
	mov	es, bx

	mov	al, [es:keyflags1-bios_data]
	mov	ah, al

	pop	bx
	pop	es

	iret

; ************************* INT 17h handler - printer

int17:
	cmp	ah, 0x01
	je	int17_initprint ; Initialise printer

	jmp	reach_stack_stc

  int17_initprint:

	mov	ah, 1 ; No printer
	jmp	reach_stack_stc

; ************************* INT 19h = reboot

int19:
	jmp	boot

; ************************* INT 1Ah - clock

int1a:
	cmp	ah, 0
	je	int1a_getsystime ; Get ticks since midnight (used for RTC time)
	cmp	ah, 2
	je	int1a_gettime ; Get RTC time (not actually used by DOS)
	cmp	ah, 4
	je	int1a_getdate ; Get RTC date
	cmp	ah, 0x0f
	je	int1a_init    ; Initialise RTC

	iret

  int1a_getsystime:

	push	ax
	push	bx
	push	ds
	push	es

	push	cs
	push	cs
	pop	ds
	pop	es

	mov	bx, timetable

	extended_get_rtc

	mov	ax, 182  ; Clock ticks in 10 seconds
	mul	word [tm_msec]
	mov	bx, 10000
	div	bx ; AX now contains clock ticks in milliseconds counter
	mov	[tm_msec], ax

	mov	ax, 182  ; Clock ticks in 10 seconds
	mul	word [tm_sec]
	mov	bx, 10
	mov	dx, 0
	div	bx ; AX now contains clock ticks in seconds counter
	mov	[tm_sec], ax

	mov	ax, 1092 ; Clock ticks in a minute
	mul	word [tm_min] ; AX now contains clock ticks in minutes counter
	mov	[tm_min], ax
	
	mov	ax, 65520 ; Clock ticks in an hour
	mul	word [tm_hour] ; DX:AX now contains clock ticks in hours counter

	add	ax, [tm_msec] ; Add milliseconds in to AX
	adc	dx, 0 ; Carry into DX if necessary
	add	ax, [tm_sec] ; Add seconds in to AX
	adc	dx, 0 ; Carry into DX if necessary
	add	ax, [tm_min] ; Add minutes in to AX
	adc	dx, 0 ; Carry into DX if necessary

	push	dx
	push	ax
	pop	dx
	pop	cx

	pop	es
	pop	ds
	pop	bx
	pop	ax

	mov	al, 0
	iret

  int1a_gettime:

	; Return the system time in BCD format. DOS doesn't use this, but we need to return
	; something or the system thinks there is no RTC.

	push	ds
	push	es
	push	ax
	push	bx

	push	cs
	push	cs
	pop	ds
	pop	es

	mov	bx, timetable

	extended_get_rtc

	mov	ax, 0
	mov	cx, [tm_hour]
	call	hex_to_bcd
	mov	bh, al		; Hour in BCD is in BH

	mov	ax, 0
	mov	cx, [tm_min]
	call	hex_to_bcd
	mov	bl, al		; Minute in BCD is in BL

	mov	ax, 0
	mov	cx, [tm_sec]
	call	hex_to_bcd
	mov	dh, al		; Second in BCD is in DH

	mov	dl, 0		; Daylight saving flag = 0 always

	mov	cx, bx		; Hour:minute now in CH:CL

	pop	bx
	pop	ax
	pop	es
	pop	ds

	jmp	reach_stack_clc

  int1a_getdate:

	; Return the system date in BCD format.

	push	ds
	push	es
	push	bx
	push	ax

	push	cs
	push	cs
	pop	ds
	pop	es

	mov	bx, timetable

	extended_get_rtc

	mov	ax, 0x1900
	mov	cx, [tm_year]
	call	hex_to_bcd
	mov	cx, ax
	push	cx

	mov	ax, 1
	mov	cx, [tm_mon]
	call	hex_to_bcd
	mov	dh, al

	mov	ax, 0
	mov	cx, [tm_mday]
	call	hex_to_bcd
	mov	dl, al

	pop	cx
	pop	ax
	pop	bx
	pop	es
	pop	ds

	jmp	reach_stack_clc

  int1a_init:

	jmp	reach_stack_clc

; ************************* INT 1Ch - the other timer interrupt

int1c:

	iret

; ************************* INT 1Eh - diskette parameter table

int1e:

		db 0xdf ; Step rate 2ms, head unload time 240ms
		db 0x02 ; Head load time 4 ms, non-DMA mode 0
		db 0x25 ; Byte delay until motor turned off
		db 0x02 ; 512 bytes per sector
int1e_spt	db 18	; 18 sectors per track (1.44MB)
		db 0x1B ; Gap between sectors for 3.5" floppy
		db 0xFF ; Data length (ignored)
		db 0x54 ; Gap length when formatting
		db 0xF6 ; Format filler byte
		db 0x0F ; Head settle time (1 ms)
		db 0x08 ; Motor start time in 1/8 seconds

; ************************* INT 41h - hard disk parameter table

int41:

int41_max_cyls	dw 0
int41_max_heads	db 0
		dw 0
		dw 0
		db 0
		db 11000000b
		db 0
		db 0
		db 0
		dw 0
int41_max_sect	db 0
		db 0

; ************************* ROM configuration table

rom_config	dw 16		; 16 bytes following
		db 0xfe		; Model
		db 'A'		; Submodel
		db 'C'		; BIOS revision
		db 0b00100000   ; Feature 1
		db 0b00000000   ; Feature 2
		db 0b00000000   ; Feature 3
		db 0b00000000   ; Feature 4
		db 0b00000000   ; Feature 5
		db 0, 0, 0, 0, 0, 0

; Internal state variables

num_disks	dw 0	; Number of disks present
hd_secs_hi	dw 0	; Total sectors on HD (high word)
hd_secs_lo	dw 0	; Total sectors on HD (low word)
hd_max_sector	dw 0	; Max sector number on HD
hd_max_track	dw 0	; Max track number on HD
hd_max_head	dw 0	; Max head number on HD
drive_tracks_temp dw 0
drive_sectors_temp dw 0
drive_heads_temp  dw 0
drive_num_temp    dw 0
boot_state	db 0
cga_refresh_reg	db 0

; Default interrupt handlers

int0:
int1:
int2:
int3:
int4:
int5:
int6:
int7:
intb:
intc:
intd:
inte:
intf:
int18:
int1b:

iret

; ************ Function call library ************

; Hex to BCD routine. Input is AX in hex (can be 0), and adds CX in hex to it, forming a BCD output in AX.

hex_to_bcd:

	push	bx

	jcxz	h2bfin

  h2bloop:

	inc	ax

	; First process the low nibble of AL
	mov	bh, al
	and	bh, 0x0f
	cmp	bh, 0x0a
	jne	c1
	add	ax, 0x0006

	; Then the high nibble of AL
  c1:
	mov	bh, al
	and	bh, 0xf0
	cmp	bh, 0xa0
	jne	c2
	add	ax, 0x0060

	; Then the low nibble of AH
  c2:	
	mov	bh, ah
	and	bh, 0x0f
	cmp	bh, 0x0a
	jne	c3
	add	ax, 0x0600

  c3:	
	loop	h2bloop
  h2bfin:
	pop	bx
	ret

; Keyboard adjust buffer head and tail. If either head or the tail are at the end of the buffer, reset them
; back to the start, since it is a circular buffer.

kb_adjust_buf:

	push	ax
	push	bx

	; Check to see if the head is at the end of the buffer (or beyond). If so, bring it back
	; to the start

	mov	ax, [es:kbbuf_end_ptr-bios_data]
	cmp	[es:kbbuf_head-bios_data], ax
	jnge	kb_adjust_tail

	mov	bx, [es:kbbuf_start_ptr-bios_data]
	mov	[es:kbbuf_head-bios_data], bx	

  kb_adjust_tail:

	; Check to see if the tail is at the end of the buffer (or beyond). If so, bring it back
	; to the start

	mov	ax, [es:kbbuf_end_ptr-bios_data]
	cmp	[es:kbbuf_tail-bios_data], ax
	jnge	kb_adjust_done

	mov	bx, [es:kbbuf_start_ptr-bios_data]
	mov	[es:kbbuf_tail-bios_data], bx	

  kb_adjust_done:

	pop	bx
	pop	ax
	ret

; Convert CHS disk position (in CH, CL and DH) to absolute sector number in BP:SI
; Floppy disks have 512 bytes per sector, 9/18 sectors per track, 2 heads. DH is head number (1 or 0), CH bits 5..0 is
; sector number, CL7..6 + CH7..0 is 10-bit cylinder/track number. Hard disks have 512 bytes per sector, but a variable
; number of tracks and heads.

chs_to_abs:

	push	ax	
	push	bx
	push	cx
	push	dx

	mov	[cs:drive_num_temp], dl

	; First, we extract the track number from CH and CL.

	push	cx
	mov	bh, cl
	mov	cl, 6
	shr	bh, cl
	mov	bl, ch

	; Multiply track number (now in BX) by the number of heads

	cmp	byte [cs:drive_num_temp], 1 ; Floppy disk?

	push	dx

	mov	dx, 0
	xchg	ax, bx

	jne	chs_hd

	shl	ax, 1 ; Multiply by 2 (number of heads on FD)
	push	ax
	xor	ax, ax
	mov	al, [cs:int1e_spt]
	mov	[cs:drive_sectors_temp], ax ; Retrieve sectors per track from INT 1E table
	pop	ax

	jmp	chs_continue

chs_hd:

	mov	bp, [cs:hd_max_head]
	inc	bp
	mov	[cs:drive_heads_temp], bp

	mul	word [cs:drive_heads_temp] ; HD, so multiply by computed head count

	mov	bp, [cs:hd_max_sector] ; We previously calculated maximum HD track, so number of tracks is 1 more
	mov	[cs:drive_sectors_temp], bp

chs_continue:

	xchg	ax, bx

	pop	dx

	xchg	dh, dl
	mov	dh, 0
	add	bx, dx

	mov	ax, [cs:drive_sectors_temp]
	mul	bx

	; Now we extract the sector number (from 1 to 63) - for some reason they start from 1

	pop	cx
	mov	ch, 0
	and	cl, 0x3F
	dec	cl

	add	ax, cx
	adc	dx, 0
	mov	bp, ax
	mov	si, dx

	; Now, SI:BP contains offset into disk image file (FD or HD)

	pop	dx
	pop	cx
	pop	bx
	pop	ax
	ret

; Clear video memory with attribute in BH

clear_screen:

	push	ax

	push	ax
	push	es

	mov	ax, 0x40
	mov	es, ax
	mov	byte [es:curpos_x-bios_data], 0
	mov	byte [es:crt_curpos_x-bios_data], 0
	mov	byte [es:curpos_y-bios_data], 0
	mov	byte [es:crt_curpos_y-bios_data], 0

	pop	es
	pop	ax

	push es
	push di
	push cx

	cld
	mov	ax, 0xb800
	mov	es, ax
	mov	di, 0
	mov	al, 0
	mov	ah, bh
	mov	cx, 80*25
	rep	stosw

	pop	cx
	pop	di
	pop	es

	pop	ax

	ret

; Sets key available flag on I/O port 0x64, outputs key scan code in AL to I/O port 0x60, and calls INT 9

io_key_available:

	push	ax
	mov	al, 1
	out	0x64, al
	pop	ax

	out	0x60, al
	int	9
	ret

; Reaches up into the stack before the end of an interrupt handler, and sets the carry flag

reach_stack_stc:

	xchg	bp, sp
	or	word [bp+4], 1
	xchg	bp, sp
	iret

; Reaches up into the stack before the end of an interrupt handler, and clears the carry flag

reach_stack_clc:

	xchg	bp, sp
	and	word [bp+4], 0xfffe
	xchg	bp, sp
	iret

; Reaches up into the stack before the end of an interrupt handler, and returns with the current
; setting of the carry flag

reach_stack_carry:

	jc	reach_stack_stc
	jmp	reach_stack_clc

; ****************************************************************************************
; That's it for the code. Now, the data tables follow.
; ****************************************************************************************

; Standard PC-compatible BIOS data area - to copy to 40:0

bios_data:

com1addr	dw	0x3F8
com2addr	dw	0
com3addr	dw	0
com4addr	dw	0
lpt1addr	dw	0
lpt2addr	dw	0
lpt3addr	dw	0
lpt4addr	dw	0
equip		dw	0b0000001000100001 		; With COM1
;equip		dw	0b0000000000100001
		db	0
memsize		dw	0x280
		db	0
		db	0
keyflags1	db	0
keyflags2	db	0
		db	0
kbbuf_head	dw	kbbuf-bios_data
kbbuf_tail	dw	kbbuf-bios_data
kbbuf: times 32	db	'X'
drivecal	db	0
diskmotor	db	0
motorshutoff	db	0x07
disk_laststatus	db	0
times 7		db	0
vidmode		db	0x03
vid_cols	dw	80
page_size	dw	0x1000
		dw	0
curpos_x	db	0
curpos_y	db	0
times 7		dw	0
cur_v_end	db	7
cur_v_start	db	6
disp_page	db	0
crtport		dw	0x3d4
		db	10
		db	0
times 5		db	0
clk_dtimer	dd	0
clk_rollover	db	0
ctrl_break	db	0
soft_rst_flg	dw	0x1234
		db	0
num_hd		db	0
		db	0
		db	0
		dd	0
		dd	0
kbbuf_start_ptr	dw	0x001e
kbbuf_end_ptr	dw	0x003e
vid_rows	db	25         ; at 40:84
		db	0
		db	0
vidmode_opt	db	0 ; 0x70
		db	0 ; 0x89
		db	0 ; 0x51
		db	0 ; 0x0c
		db	0
		db	0
		db	0
		db	0
		db	0
		db	0
		db	0
		db	0
		db	0
		db	0
		db	0
kb_mode		db	0
kb_led		db	0
		db	0
		db	0
		db	0
		db	0
boot_device	db	0
crt_curpos_x	db	0
crt_curpos_y	db	0
		db	0
		db	0
cursor_visible	db	1
		db	0
		db	0
		db	0
		db	0
this_keystroke	db	0
this_keystroke_ascii		db	0
timer0_freq	dw	0xffff ; PIT channel 0 (55ms)
timer2_freq	dw	0      ; PIT channel 2
cga_vmode	db	0
vmem_offset	dw	0      ; Video RAM offset
ending:		times (0xff-($-com1addr)) db	0

; Interrupt vector table - to copy to 0:0

int_table	dw int0
          	dw 0xf000
          	dw int1
          	dw 0xf000
          	dw int2
          	dw 0xf000
          	dw int3
          	dw 0xf000
          	dw int4
          	dw 0xf000
          	dw int5
          	dw 0xf000
          	dw int6
          	dw 0xf000
          	dw int7
          	dw 0xf000
          	dw int8
          	dw 0xf000
          	dw int9
          	dw 0xf000
          	dw inta
          	dw 0xf000
          	dw intb
          	dw 0xf000
          	dw intc
          	dw 0xf000
          	dw intd
          	dw 0xf000
          	dw inte
          	dw 0xf000
          	dw intf
          	dw 0xf000
          	dw int10
          	dw 0xf000
          	dw int11
          	dw 0xf000
          	dw int12
          	dw 0xf000
          	dw int13
          	dw 0xf000
          	dw int14
          	dw 0xf000
          	dw int15
          	dw 0xf000
          	dw int16
          	dw 0xf000
          	dw int17
          	dw 0xf000
          	dw int18
          	dw 0xf000
          	dw int19
          	dw 0xf000
          	dw int1a
          	dw 0xf000
          	dw int1b
          	dw 0xf000
          	dw int1c
          	dw 0xf000
          	dw int1d
          	dw 0xf000
          	dw int1e

itbl_size	dw $-int_table

; CGA font data

cga_font 	db	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e
			db	0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e, 0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00
			db	0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x38, 0x7c, 0x38, 0xfe, 0xfe, 0xd6, 0x10, 0x38
			db	0x10, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x10, 0x38, 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00
			db	0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff, 0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00
			db	0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff, 0x0f, 0x03, 0x05, 0x7d, 0x84, 0x84, 0x84, 0x78
			db	0x3c, 0x42, 0x42, 0x42, 0x3c, 0x18, 0x7e, 0x18, 0x3f, 0x21, 0x3f, 0x20, 0x20, 0x60, 0xe0, 0xc0
			db	0x3f, 0x21, 0x3f, 0x21, 0x23, 0x67, 0xe6, 0xc0, 0x18, 0xdb, 0x3c, 0xe7, 0xe7, 0x3c, 0xdb, 0x18
			db	0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00, 0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00
			db	0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x00, 0x24, 0x00
			db	0x7f, 0x92, 0x92, 0x72, 0x12, 0x12, 0x12, 0x00, 0x3e, 0x63, 0x38, 0x44, 0x44, 0x38, 0xcc, 0x78
			db	0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff
			db	0x10, 0x38, 0x7c, 0x54, 0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x54, 0x7c, 0x38, 0x10, 0x00
			db	0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00
			db	0x00, 0x00, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00
			db	0x00, 0x10, 0x38, 0x7c, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x38, 0x10, 0x10, 0x00, 0x10, 0x00
			db	0x24, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x24, 0x7e, 0x24, 0x7e, 0x24, 0x24, 0x00
			db	0x18, 0x3e, 0x40, 0x3c, 0x02, 0x7c, 0x18, 0x00, 0x00, 0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00
			db	0x30, 0x48, 0x30, 0x56, 0x88, 0x88, 0x76, 0x00, 0x10, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00
			db	0x10, 0x20, 0x40, 0x40, 0x40, 0x20, 0x10, 0x00, 0x20, 0x10, 0x08, 0x08, 0x08, 0x10, 0x20, 0x00
			db	0x00, 0x44, 0x38, 0xfe, 0x38, 0x44, 0x00, 0x00, 0x00, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x20, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00
			db	0x3c, 0x42, 0x46, 0x4a, 0x52, 0x62, 0x3c, 0x00, 0x10, 0x30, 0x50, 0x10, 0x10, 0x10, 0x7c, 0x00
			db	0x3c, 0x42, 0x02, 0x0c, 0x30, 0x42, 0x7e, 0x00, 0x3c, 0x42, 0x02, 0x1c, 0x02, 0x42, 0x3c, 0x00
			db	0x08, 0x18, 0x28, 0x48, 0xfe, 0x08, 0x1c, 0x00, 0x7e, 0x40, 0x7c, 0x02, 0x02, 0x42, 0x3c, 0x00
			db	0x1c, 0x20, 0x40, 0x7c, 0x42, 0x42, 0x3c, 0x00, 0x7e, 0x42, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00
			db	0x3c, 0x42, 0x42, 0x3c, 0x42, 0x42, 0x3c, 0x00, 0x3c, 0x42, 0x42, 0x3e, 0x02, 0x04, 0x38, 0x00
			db	0x00, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x20
			db	0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00
			db	0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x3c, 0x42, 0x02, 0x04, 0x08, 0x00, 0x08, 0x00
			db	0x3c, 0x42, 0x5e, 0x52, 0x5e, 0x40, 0x3c, 0x00, 0x18, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x00
			db	0x7c, 0x22, 0x22, 0x3c, 0x22, 0x22, 0x7c, 0x00, 0x1c, 0x22, 0x40, 0x40, 0x40, 0x22, 0x1c, 0x00
			db	0x78, 0x24, 0x22, 0x22, 0x22, 0x24, 0x78, 0x00, 0x7e, 0x22, 0x28, 0x38, 0x28, 0x22, 0x7e, 0x00
			db	0x7e, 0x22, 0x28, 0x38, 0x28, 0x20, 0x70, 0x00, 0x1c, 0x22, 0x40, 0x40, 0x4e, 0x22, 0x1e, 0x00
			db	0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00
			db	0x0e, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38, 0x00, 0x62, 0x24, 0x28, 0x30, 0x28, 0x24, 0x63, 0x00
			db	0x70, 0x20, 0x20, 0x20, 0x20, 0x22, 0x7e, 0x00, 0x63, 0x55, 0x49, 0x41, 0x41, 0x41, 0x41, 0x00
			db	0x62, 0x52, 0x4a, 0x46, 0x42, 0x42, 0x42, 0x00, 0x18, 0x24, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00
			db	0x7c, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x70, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x4a, 0x3c, 0x03, 0x00
			db	0x7c, 0x22, 0x22, 0x3c, 0x28, 0x24, 0x72, 0x00, 0x3c, 0x42, 0x40, 0x3c, 0x02, 0x42, 0x3c, 0x00
			db	0x7f, 0x49, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00
			db	0x41, 0x41, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00, 0x41, 0x41, 0x41, 0x49, 0x49, 0x49, 0x36, 0x00
			db	0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41, 0x00, 0x41, 0x22, 0x14, 0x08, 0x08, 0x08, 0x1c, 0x00
			db	0x7f, 0x42, 0x04, 0x08, 0x10, 0x21, 0x7f, 0x00, 0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x78, 0x00
			db	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x78, 0x08, 0x08, 0x08, 0x08, 0x08, 0x78, 0x00
			db	0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff
			db	0x10, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x02, 0x3e, 0x42, 0x3f, 0x00
			db	0x60, 0x20, 0x20, 0x2e, 0x31, 0x31, 0x2e, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x40, 0x42, 0x3c, 0x00
			db	0x06, 0x02, 0x02, 0x3a, 0x46, 0x46, 0x3b, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00
			db	0x0c, 0x12, 0x10, 0x38, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x3d, 0x42, 0x42, 0x3e, 0x02, 0x7c
			db	0x60, 0x20, 0x2c, 0x32, 0x22, 0x22, 0x62, 0x00, 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00
			db	0x02, 0x00, 0x06, 0x02, 0x02, 0x42, 0x42, 0x3c, 0x60, 0x20, 0x24, 0x28, 0x30, 0x28, 0x26, 0x00
			db	0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00, 0x00, 0x00, 0x76, 0x49, 0x49, 0x49, 0x49, 0x00
			db	0x00, 0x00, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3c, 0x00
			db	0x00, 0x00, 0x6c, 0x32, 0x32, 0x2c, 0x20, 0x70, 0x00, 0x00, 0x36, 0x4c, 0x4c, 0x34, 0x04, 0x0e
			db	0x00, 0x00, 0x6c, 0x32, 0x22, 0x20, 0x70, 0x00, 0x00, 0x00, 0x3e, 0x40, 0x3c, 0x02, 0x7c, 0x00
			db	0x10, 0x10, 0x7c, 0x10, 0x10, 0x12, 0x0c, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x46, 0x3a, 0x00
			db	0x00, 0x00, 0x41, 0x41, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00, 0x41, 0x49, 0x49, 0x49, 0x36, 0x00
			db	0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x7c
			db	0x00, 0x00, 0x7c, 0x08, 0x10, 0x20, 0x7c, 0x00, 0x0c, 0x10, 0x10, 0x60, 0x10, 0x10, 0x0c, 0x00
			db	0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x00, 0x30, 0x08, 0x08, 0x06, 0x08, 0x08, 0x30, 0x00
			db	0x32, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x41, 0x41, 0x7f, 0x00
			db	0x3c, 0x42, 0x40, 0x42, 0x3c, 0x0c, 0x02, 0x3c, 0x00, 0x44, 0x00, 0x44, 0x44, 0x44, 0x3e, 0x00
			db	0x0c, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00, 0x3c, 0x42, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00
			db	0x42, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00, 0x30, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00
			db	0x10, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00, 0x00, 0x00, 0x3c, 0x40, 0x40, 0x3c, 0x06, 0x1c
			db	0x3c, 0x42, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00, 0x42, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00
			db	0x30, 0x00, 0x3c, 0x42, 0x7e, 0x40, 0x3c, 0x00, 0x24, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1c, 0x00
			db	0x7c, 0x82, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00, 0x30, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1c, 0x00
			db	0x42, 0x18, 0x24, 0x42, 0x7e, 0x42, 0x42, 0x00, 0x18, 0x18, 0x00, 0x3c, 0x42, 0x7e, 0x42, 0x00
			db	0x0c, 0x00, 0x7c, 0x20, 0x38, 0x20, 0x7c, 0x00, 0x00, 0x00, 0x33, 0x0c, 0x3f, 0x44, 0x3b, 0x00
			db	0x1f, 0x24, 0x44, 0x7f, 0x44, 0x44, 0x47, 0x00, 0x18, 0x24, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00
			db	0x00, 0x42, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00, 0x20, 0x10, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00
			db	0x18, 0x24, 0x00, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x20, 0x10, 0x00, 0x42, 0x42, 0x42, 0x3c, 0x00
			db	0x00, 0x42, 0x00, 0x42, 0x42, 0x3e, 0x02, 0x3c, 0x42, 0x18, 0x24, 0x42, 0x42, 0x24, 0x18, 0x00
			db	0x42, 0x00, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x08, 0x08, 0x3e, 0x40, 0x40, 0x3e, 0x08, 0x08
			db	0x18, 0x24, 0x20, 0x70, 0x20, 0x42, 0x7c, 0x00, 0x44, 0x28, 0x7c, 0x10, 0x7c, 0x10, 0x10, 0x00
			db	0xf8, 0x4c, 0x78, 0x44, 0x4f, 0x44, 0x45, 0xe6, 0x1c, 0x12, 0x10, 0x7c, 0x10, 0x10, 0x90, 0x60
			db	0x0c, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x3e, 0x00, 0x0c, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1c, 0x00
			db	0x04, 0x08, 0x00, 0x3c, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x04, 0x08, 0x42, 0x42, 0x42, 0x3c, 0x00
			db	0x32, 0x4c, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x00, 0x34, 0x4c, 0x00, 0x62, 0x52, 0x4a, 0x46, 0x00
			db	0x3c, 0x44, 0x44, 0x3e, 0x00, 0x7e, 0x00, 0x00, 0x38, 0x44, 0x44, 0x38, 0x00, 0x7c, 0x00, 0x00
			db	0x10, 0x00, 0x10, 0x20, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x40, 0x40, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x7e, 0x02, 0x02, 0x00, 0x00, 0x42, 0xc4, 0x48, 0xf6, 0x29, 0x43, 0x8c, 0x1f
			db	0x42, 0xc4, 0x4a, 0xf6, 0x2a, 0x5f, 0x82, 0x02, 0x00, 0x10, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00
			db	0x00, 0x12, 0x24, 0x48, 0x24, 0x12, 0x00, 0x00, 0x00, 0x48, 0x24, 0x12, 0x24, 0x48, 0x00, 0x00
			db	0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa
			db	0xdb, 0x77, 0xdb, 0xee, 0xdb, 0x77, 0xdb, 0xee, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10
			db	0x10, 0x10, 0x10, 0x10, 0xf0, 0x10, 0x10, 0x10, 0x10, 0x10, 0xf0, 0x10, 0xf0, 0x10, 0x10, 0x10
			db	0x14, 0x14, 0x14, 0x14, 0xf4, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x14, 0x14, 0x14
			db	0x00, 0x00, 0xf0, 0x10, 0xf0, 0x10, 0x10, 0x10, 0x14, 0x14, 0xf4, 0x04, 0xf4, 0x14, 0x14, 0x14
			db	0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x00, 0xfc, 0x04, 0xf4, 0x14, 0x14, 0x14
			db	0x14, 0x14, 0xf4, 0x04, 0xfc, 0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x14, 0xfc, 0x00, 0x00, 0x00
			db	0x10, 0x10, 0xf0, 0x10, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x10, 0x10, 0x10
			db	0x10, 0x10, 0x10, 0x10, 0x1f, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0xff, 0x00, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x00, 0xff, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10
			db	0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0xff, 0x10, 0x10, 0x10
			db	0x10, 0x10, 0x1f, 0x10, 0x1f, 0x10, 0x10, 0x10, 0x14, 0x14, 0x14, 0x14, 0x17, 0x14, 0x14, 0x14
			db	0x14, 0x14, 0x17, 0x10, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x10, 0x17, 0x14, 0x14, 0x14
			db	0x14, 0x14, 0xf7, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xf7, 0x14, 0x14, 0x14
			db	0x14, 0x14, 0x17, 0x10, 0x17, 0x14, 0x14, 0x14, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00
			db	0x14, 0x14, 0xf7, 0x00, 0xf7, 0x14, 0x14, 0x14, 0x10, 0x10, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00
			db	0x14, 0x14, 0x14, 0x14, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x10, 0x10, 0x10
			db	0x00, 0x00, 0x00, 0x00, 0xff, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x1f, 0x00, 0x00, 0x00
			db	0x10, 0x10, 0x1f, 0x10, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x10, 0x1f, 0x10, 0x10, 0x10
			db	0x00, 0x00, 0x00, 0x00, 0x1f, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xff, 0x14, 0x14, 0x14
			db	0x10, 0x10, 0xff, 0x10, 0xff, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0xf0, 0x00, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x00, 0x1f, 0x10, 0x10, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
			db	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0
			db	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00
			db	0x00, 0x00, 0x31, 0x4a, 0x44, 0x4a, 0x31, 0x00, 0x00, 0x3c, 0x42, 0x7c, 0x42, 0x7c, 0x40, 0x40
			db	0x00, 0x7e, 0x42, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x3f, 0x54, 0x14, 0x14, 0x14, 0x14, 0x00
			db	0x7e, 0x42, 0x20, 0x18, 0x20, 0x42, 0x7e, 0x00, 0x00, 0x00, 0x3e, 0x48, 0x48, 0x48, 0x30, 0x00
			db	0x00, 0x44, 0x44, 0x44, 0x7a, 0x40, 0x40, 0x80, 0x00, 0x33, 0x4c, 0x08, 0x08, 0x08, 0x08, 0x00
			db	0x7c, 0x10, 0x38, 0x44, 0x44, 0x38, 0x10, 0x7c, 0x18, 0x24, 0x42, 0x7e, 0x42, 0x24, 0x18, 0x00
			db	0x18, 0x24, 0x42, 0x42, 0x24, 0x24, 0x66, 0x00, 0x1c, 0x20, 0x18, 0x3c, 0x42, 0x42, 0x3c, 0x00
			db	0x00, 0x62, 0x95, 0x89, 0x95, 0x62, 0x00, 0x00, 0x02, 0x04, 0x3c, 0x4a, 0x52, 0x3c, 0x40, 0x80
			db	0x0c, 0x10, 0x20, 0x3c, 0x20, 0x10, 0x0c, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00
			db	0x00, 0x7e, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x00, 0x7c, 0x00
			db	0x10, 0x08, 0x04, 0x08, 0x10, 0x00, 0x7e, 0x00, 0x08, 0x10, 0x20, 0x10, 0x08, 0x00, 0x7e, 0x00
			db	0x0c, 0x12, 0x12, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x90, 0x90, 0x60
			db	0x18, 0x18, 0x00, 0x7e, 0x00, 0x18, 0x18, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x32, 0x4c, 0x00, 0x00
			db	0x30, 0x48, 0x48, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00
			db	0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x0f, 0x08, 0x08, 0x08, 0x08, 0xc8, 0x28, 0x18
			db	0x78, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00, 0x00, 0x30, 0x48, 0x10, 0x20, 0x78, 0x00, 0x00, 0x00
			db	0x00, 0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

; INT 8 millisecond counter

last_int8_msec	dw	0

; This is the format of the 36-byte tm structure, returned by the emulator's RTC query call

timetable:

tm_sec		equ $
tm_min		equ $+4
tm_hour		equ $+8
tm_mday		equ $+12
tm_mon		equ $+16
tm_year		equ $+20
tm_wday		equ $+24
tm_yday		equ $+28
tm_dst		equ $+32
tm_msec		equ $+36
