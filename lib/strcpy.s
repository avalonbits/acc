;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strcat
	XDEF	_strcpy
	XDEF	_stpcpy

	.assume adl=1
	SEGMENT CODE

; char *strcpy(char *to, const char *from): the length, and then ldir of
; that and its zero.
_strcpy:
	ld	iy, 0
	add	iy, sp
	ld	de, (iy+3)
.strcpy_from:
	ld	hl, (iy+6)
	xor	a, a
	ld	bc, 0
	cpir
	sbc	hl, hl
	sbc	hl, bc			; n + 1, never 0
	push	hl
	pop	bc
	ld	hl, (iy+6)
	ldir
	ld	hl, (iy+3)
	ret

; char *strcat(char *to, const char *from): strcpy to where to's zero is.
_strcat:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)
	xor	a, a
	ld	bc, 0
	cpir
	dec	hl
	ex	de, hl
	jr	.strcpy_from

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
