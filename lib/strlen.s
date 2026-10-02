;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strlen
	XDEF	_strnlen

	.assume adl=1
	SEGMENT CODE

;---------------------------------------------------------------- strings
; The string functions of <string.h> that walk a string, and memcmp, here
; and in strcmp.s, strchr.s and strcpy.s: in C they are a loop of loads,
; tests and steps, five to twelve times slower than the block instructions
; -- cpir finds a byte, ldir copies, cpi compares and counts -- that do the
; same work here. lib/str.c has the rest of <string.h>.
;
; C functions: arguments three bytes each from (sp+3), the answer in HL, IX
; kept. Bytes are compared as unsigned char, as C says; the answer is their
; difference.
;
; cpir with BC = 0 counts down from 2^24, so after finding the zero of a
; string of length n it leaves BC = -(n+1), and 0 - BC is n+1.


; size_t strlen(const char *s)
_strlen:
	ld	hl, 3
	add	hl, sp
	ld	hl, (hl)
	xor	a, a
	ld	bc, 0
	cpir				; carry is still clear, from the xor
	sbc	hl, hl
	scf
	sbc	hl, bc			; 0 - BC - 1: n
	ret

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
