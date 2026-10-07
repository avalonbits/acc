;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
; Not a C99 name, so a member of its own: a program may have its own of it,
; and it must not come along with a C99 function that the program uses.
;

	XDEF	_strcasecmp

	.assume adl=1
	SEGMENT CODE

; int strcasecmp(const char *a, const char *b): strcmp's loop, the two bytes
; compared as they are first. Only where they differ are both lowered, in
; place, to see whether they are the same letter -- an assembler looking a
; word up in its tables compares mostly bytes that are equal or differ at
; once, and lowering every one of them through a call each cost seven times
; agondev's strcasecmp.
_strcasecmp:
	ld	iy, 0
	add	iy, sp
	ld	de, (iy+3)		; a
	ld	hl, (iy+6)		; b
.case_loop:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, .case_fold		; not the same byte: the same letter?
	or	a, a
	jr	z, .case_same		; both ended together
.case_next:
	inc	de
	inc	hl
	jr	.case_loop
.case_fold:
	ld	b, a			; a's
	ld	a, (hl)			; b's, lowered into c
	cp	a, 'A'
	jr	c, .case_b_low
	cp	a, 'Z' + 1
	jr	nc, .case_b_low
	add	a, 32
.case_b_low:
	ld	c, a
	ld	a, b			; and a's
	cp	a, 'A'
	jr	c, .case_a_low
	cp	a, 'Z' + 1
	jr	nc, .case_a_low
	add	a, 32
.case_a_low:
	sub	a, c			; carry when a's is below b's
	jr	z, .case_next		; the same letter; not an end, as they differed
	sbc	hl, hl			; a's less b's, as an int
	ld	l, a
	ret
.case_same:
	or	a, a
	sbc	hl, hl
	ret
