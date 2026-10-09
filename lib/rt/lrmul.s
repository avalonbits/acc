;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrmul

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl * a:bc, wrapping; bc, d, ix and iy kept.
;
; In registers, as the 24-bit multiply is (mul.s): MLT's 8x8 -> 16 products
; of the bytes, the ten whose place is below bit 32, added by column. With
; the left as x0..x3 and the right as y0..y3, the low half is x0y0 with
; x0y1 + x1y0 a byte up; the high half the rest of that, x1y1, x2y0 and x0y2,
; and a byte up the low bytes of x2y1, x3y0, x0y3 and x1y2 -- worked out in
; HL's low sixteen bits, what is above them never read. The operands from a
; frame of their own, the answer's three low bytes put there too and loaded
; whole. Through acc_rt_lrcall into _acc_rt_lmul, which takes its longs in
; memory and copied them again, a multiply cost both copies and the frame;
; perf's sort spent 350 thousand cycles there.
_acc_rt_lrmul:
	push	ix
	push	bc
	ld	ix, -11
	add	ix, sp
	ld	sp, ix
	ld	(ix + 0), hl		; x0..x3 at ix+0, y0..y3 at ix+4
	ld	(ix + 3), e
	ld	(ix + 4), bc
	ld	(ix + 7), a

	ld	b, (ix + 0)
	ld	c, (ix + 4)
	mlt	bc			; x0y0
	ld	(ix + 8), c		; the answer's byte 0
	or	a, a
	sbc	hl, hl
	ld	l, b			; its high byte, up a byte
	ld	b, (ix + 0)
	ld	c, (ix + 5)
	mlt	bc			; x0y1
	add	hl, bc
	ld	b, (ix + 1)
	ld	c, (ix + 4)
	mlt	bc			; x1y0
	add	hl, bc
	ld	(ix + 9), l		; byte 1

	push	hl			; what is above it, down a byte
	inc	sp
	pop	hl
	dec	sp
	ld	b, (ix + 1)
	ld	c, (ix + 5)
	mlt	bc			; x1y1
	add	hl, bc
	ld	b, (ix + 2)
	ld	c, (ix + 4)
	mlt	bc			; x2y0
	add	hl, bc
	ld	b, (ix + 0)
	ld	c, (ix + 6)
	mlt	bc			; x0y2
	add	hl, bc
	ld	(ix + 10), l		; byte 2

	ld	a, h			; byte 3, with the low bytes past it
	ld	b, (ix + 2)
	ld	c, (ix + 5)
	mlt	bc			; x2y1
	add	a, c
	ld	b, (ix + 3)
	ld	c, (ix + 4)
	mlt	bc			; x3y0
	add	a, c
	ld	b, (ix + 0)
	ld	c, (ix + 7)
	mlt	bc			; x0y3
	add	a, c
	ld	b, (ix + 1)
	ld	c, (ix + 6)
	mlt	bc			; x1y2
	add	a, c

	ld	e, a
	ld	hl, (ix + 8)
	ld	ix, 11
	add	ix, sp
	ld	sp, ix
	pop	bc
	pop	ix
	ret
