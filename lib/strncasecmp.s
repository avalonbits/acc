;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strncasecmp

	.assume adl=1
	SEGMENT CODE

; int strncasecmp(const char *a, const char *b, size_t n): strncmp with the
; case of letters taken out, as tolower has it in the C locale -- 'A' to 'Z'
; and nothing else. The count is kept in its own argument's slot, which is
; the callee's to change, so that BC is free for the characters.
_strncasecmp:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when n is 0: HL is 0 then
	ret	z
	ld	de, (iy+3)		; a
	ld	hl, (iy+6)		; b
.ncase_loop:
	ld	a, (hl)
	call	.ncase_lower
	ld	c, a			; b's, lowered
	ld	a, (de)
	call	.ncase_lower
	sub	a, c			; carry when a's is below b's
	jr	nz, .ncase_diff
	or	a, c
	jr	z, .ncase_same		; both ended together
	inc	de
	inc	hl
	push	hl
	ld	hl, (iy+9)
	dec	hl
	ld	(iy+9), hl
	ld	bc, 0
	or	a, a
	sbc	hl, bc			; Z when n of them were the same
	pop	hl
	jr	nz, .ncase_loop
.ncase_same:
	or	a, a
	sbc	hl, hl
	ret

.ncase_diff:				; a's less b's, as an int
	sbc	hl, hl
	ld	l, a
	ret

.ncase_lower:
	cp	a, 'A'
	ret	c
	cp	a, 'Z' + 1
	ret	nc
	add	a, 32
	ret
