;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
; Not a C99 name, so a member of its own: a program may have its own of it,
; and it must not come along with a C99 function that the program uses.
;

	XDEF	_strnlen

	.assume adl=1
	SEGMENT CODE

; size_t strnlen(const char *s, size_t maxlen): s's length, but no more than
; maxlen -- cpir with maxlen as its count, so that s[maxlen] and past are not
; read. Found: maxlen less what was left, less the terminator; not: maxlen.
_strnlen:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+6)
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when maxlen is 0: HL is 0 then
	ret	z
	ld	hl, (iy+3)
	xor	a, a
	cpir
	ld	hl, (iy+6)		; maxlen; the flags are cpir's still
	ret	nz			; no terminator in reach
	scf
	sbc	hl, bc			; maxlen - BC - 1
	ret
