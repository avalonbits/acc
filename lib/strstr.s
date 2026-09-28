;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strstr

	.assume adl=1
	SEGMENT CODE

; char *strstr(const char *hay, const char *needle): each place in hay whose
; character is needle's first, and there the rest of needle compared a byte
; at a time; an empty needle is found where hay starts. The first character
; is held in C, so the scan past places that cannot match is a load, a test
; for the end and one compare.
_strstr:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+6)
	ld	a, (hl)
	or	a, a
	jr	nz, .sstr_go
	ld	hl, (iy+3)		; an empty needle
	ret
.sstr_go:
	ld	c, a			; needle's first character
	ld	hl, (iy+3)
.sstr_scan:
	ld	a, (hl)
	or	a, a
	jr	z, .sstr_none		; hay is done
	cp	a, c
	jr	z, .sstr_try
.sstr_next:
	inc	hl
	jr	.sstr_scan
.sstr_try:				; the rest of needle, from here
	push	hl
	ld	de, (iy+6)
.sstr_cmp:
	inc	hl
	inc	de
	ld	a, (de)
	or	a, a
	jr	z, .sstr_found		; needle is done: all of it matched
	cp	a, (hl)			; hay's end is a zero, which never does
	jr	z, .sstr_cmp
	pop	hl
	jr	.sstr_next
.sstr_found:
	pop	hl
	ret
.sstr_none:				; carry clear, from the or
	sbc	hl, hl
	ret
