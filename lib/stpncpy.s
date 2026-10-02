;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
; Not a C99 name, so a member of its own: a program may have its own of it,
; and it must not come along with a C99 function that the program uses.
;

	XDEF	_stpncpy

	.assume adl=1
	SEGMENT CODE

; char *stpncpy(char *to, const char *from, size_t n): strncpy, answering
; where it stopped copying -- the first null written, or to + n.
_stpncpy:
	ld	iy, 0
	add	iy, sp
	ld	de, (iy+3)
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when n is 0
	jr	z, .stpn_done
	ld	hl, (iy+6)
.stpn_copy:
	ld	a, (hl)
	or	a, a
	jr	z, .stpn_pad
	ldi
	jp	pe, .stpn_copy
.stpn_done:				; to + n, or to when n is 0
	ex	de, hl
	ret
.stpn_pad:				; BC bytes left, at DE: the answer
	push	de
	ex	de, hl
	ld	(hl), a
	dec	bc
	push	hl
	or	a, a
	sbc	hl, hl
	adc	hl, bc
	pop	hl
	jr	z, .stpn_padded
	push	hl
	pop	de
	inc	de
	ldir
.stpn_padded:
	pop	hl
	ret
