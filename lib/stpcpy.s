;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
; Not a C99 name, so a member of its own: a program may have its own of it,
; and it must not come along with a C99 function that the program uses.
;

	XDEF	_stpcpy

	.assume adl=1
	SEGMENT CODE

; char *stpcpy(char *to, const char *from): strcpy, answering where the
; terminator went -- DE is past it when ldir is done.
_stpcpy:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+6)
	xor	a, a
	ld	bc, 0
	cpir
	sbc	hl, hl
	sbc	hl, bc			; n + 1, never 0
	push	hl
	pop	bc
	ld	hl, (iy+6)
	ld	de, (iy+3)
	ldir
	ex	de, hl
	dec	hl
	ret
