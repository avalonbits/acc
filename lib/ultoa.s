;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
; Not a C99 name, so a member of its own: a program may have its own of it,
; and it must not come along with a C99 function that the program uses, nor
; with the other two. They share acc_lib_toa, in lib/toa.s.
;

	XDEF	_ultoa
	XREF	acc_lib_toa

	.assume adl=1
	SEGMENT CODE

; char *ultoa(unsigned long value, char *str, int base)
_ultoa:
	ld	iy, 0
	add	iy, sp
	ld	d, 0			; never signed
	ld	hl, (iy+3)
	ld	e, (iy+6)
	jp	acc_lib_toa
