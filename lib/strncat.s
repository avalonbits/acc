;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strncat

	.assume adl=1
	SEGMENT CODE

; char *strncat(char *to, const char *from, size_t n): to's zero found with
; cpir, then at most n of from's characters after it with ldi, and a zero.
_strncat:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)
	xor	a, a
	ld	bc, 0
	cpir
	dec	hl			; to's zero
	ex	de, hl
	ld	hl, (iy+6)
	ld	bc, (iy+9)
	push	hl
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when n is 0
	pop	hl
	jr	z, .ncat_done		; and to has its zero already
.ncat_copy:
	ld	a, (hl)
	or	a, a
	jr	z, .ncat_end
	ldi
	jp	pe, .ncat_copy
	xor	a, a
.ncat_end:				; A is 0
	ld	(de), a
.ncat_done:
	ld	hl, (iy+3)
	ret
