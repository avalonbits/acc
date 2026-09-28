;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_memcmp
	XDEF	_strcmp
	XDEF	_strncmp

	.assume adl=1
	SEGMENT CODE

; int strcmp(const char *a, const char *b)
_strcmp:
	ld	iy, 0
	add	iy, sp
	ld	de, (iy+3)
	ld	hl, (iy+6)
.strcmp_loop:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, .str_diff
	or	a, a
	jr	z, .str_same		; both ended together
	inc	de
	inc	hl
	jr	.strcmp_loop

; A byte of a in A, of b at (hl), and they differ: a - b, as an int.
.str_diff:
	sub	a, (hl)			; carry when a's is below b's
	sbc	hl, hl
	ld	l, a
	ret

.str_same:				; carry clear
	sbc	hl, hl
	ret

; int strncmp(const char *a, const char *b, size_t n)
; cpi compares A with (hl), steps HL on and counts BC down, and says in P/V
; whether BC is still not zero.
_strncmp:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when n is 0: HL is 0 then
	ret	z
	ld	de, (iy+3)
	ld	hl, (iy+6)
.strncmp_loop:
	ld	a, (de)
	cpi
	jr	nz, .strn_diff
	jp	po, .str_same_clear	; n of them, all the same
	or	a, a
	jr	z, .str_same		; both ended together
	inc	de
	jr	.strncmp_loop

.strn_diff:
	dec	hl			; cpi stepped past it
	jr	.str_diff

.str_same_clear:
	or	a, a
	sbc	hl, hl
	ret

; int memcmp(const void *a, const void *b, size_t n)
_memcmp:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc
	ret	z
	ld	de, (iy+3)
	ld	hl, (iy+6)
.memcmp_loop:
	ld	a, (de)
	cpi
	jr	nz, .strn_diff
	inc	de			; the flags are cpi's still
	jp	pe, .memcmp_loop
	jr	.str_same_clear
