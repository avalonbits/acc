;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strncpy

	.assume adl=1
	SEGMENT CODE

; char *strncpy(char *to, const char *from, size_t n): C's own peculiarity,
; and it is not a mistake here -- what is copied is padded out with zeros to
; n, and a string n or more long is left without one.
;
; ldi copies a byte and counts BC down, and says in P/V whether BC is still
; not zero. The padding is the first zero by hand and ldir copying it along.
_strncpy:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when n is 0
	jr	z, .ncpy_done
	ld	de, (iy+3)
	ld	hl, (iy+6)
.ncpy_copy:
	ld	a, (hl)
	or	a, a
	jr	z, .ncpy_pad
	ldi
	jp	pe, .ncpy_copy
	jr	.ncpy_done
.ncpy_pad:				; BC bytes left, at DE
	ex	de, hl
	ld	(hl), a
	dec	bc
	push	hl
	or	a, a
	sbc	hl, hl
	adc	hl, bc
	pop	hl
	jr	z, .ncpy_done
	push	hl
	pop	de
	inc	de
	ldir
.ncpy_done:
	ld	hl, (iy+3)
	ret
