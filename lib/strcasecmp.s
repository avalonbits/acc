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

; int strcasecmp(const char *a, const char *b): strncasecmp's loop with no
; count. See lib/strncasecmp.s.
_strcasecmp:
	ld	iy, 0
	add	iy, sp
	ld	de, (iy+3)		; a
	ld	hl, (iy+6)		; b
.case_loop:
	ld	a, (hl)
	call	.case_lower
	ld	c, a			; b's, lowered
	ld	a, (de)
	call	.case_lower
	sub	a, c			; carry when a's is below b's
	jr	nz, .case_diff
	or	a, c
	jr	z, .case_same		; both ended together
	inc	de
	inc	hl
	jr	.case_loop
.case_same:
	or	a, a
	sbc	hl, hl
	ret

.case_diff:				; a's less b's, as an int
	sbc	hl, hl
	ld	l, a
	ret

.case_lower:
	cp	a, 'A'
	ret	c
	cp	a, 'Z' + 1
	ret	nc
	add	a, 32
	ret
