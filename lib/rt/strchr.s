;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_strchr
	XDEF	_strrchr

	.assume adl=1
	SEGMENT CODE

; char *strchr(const char *s, int c): the length first, then c looked for
; in that and the zero after it -- which is how a c of 0 finds the end.
_strchr:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)
	xor	a, a
	ld	bc, 0
	cpir
	sbc	hl, hl
	sbc	hl, bc			; n + 1
	push	hl
	pop	bc
	ld	hl, (iy+3)
	ld	a, (iy+6)
	cpir
	dec	hl
	ret	z
.str_none:
	or	a, a
	sbc	hl, hl
	ret

; char *strrchr(const char *s, int c): the same, searched from the zero
; back to the start with cpdr.
_strrchr:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)
	xor	a, a
	ld	bc, 0
	cpir
	dec	hl			; the zero
	ex	de, hl
	sbc	hl, hl
	sbc	hl, bc			; n + 1
	push	hl
	pop	bc
	ex	de, hl
	ld	a, (iy+6)
	cpdr
	inc	hl			; cpdr stepped back past it
	ret	z
	jr	.str_none
