;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_mul

	.assume adl=1
	SEGMENT CODE

; ---------------------------------------------------------------- multiply
; hl = hl * bc, twenty-four bits, wrapping. A and the flags clobbered,
; everything else kept.
;
; MLT is the only multiplier and it is 8x8 -> 16, so a 24-bit product is
; built from the partial products of the bytes. Only the low three bytes of
; the result are kept, so the pairs that land entirely above bit 23 are not
; computed at all: with the operands as l,h,u and c,b,y that leaves
;
;   l*c               the low pair, bytes 0 and 1
;   l*b + h*c         bytes 1 and 2
;   l*y + h*b + u*c   byte 2 alone, the low bytes only
;
; All in registers: the top bytes, u and y, which have no names, read by
; pushing both operands a byte apart and popping D from between them. The
; middle pair, with byte 2's sum added to its top, shifted up a byte by
; eight adds; MLT leaves the top byte of its pair clear, so what it makes
; adds as it is. With the operands read from a frame through IY, as this
; was, a multiply took 163 cycles; it is 43% of perf's matmul.

_acc_rt_mul:
	push	de
	ld	d, h
	ld	e, b
	mlt	de			; h*b
	ld	a, e
	dec	sp			; the operands a byte apart: c b y l h u
	push	hl
	push	bc
	inc	sp
	pop	de			; e = b, d = y
	ld	e, l
	mlt	de			; l*y
	add	a, e
	pop	de			; e = h, d = u: the stack as it was
	ld	e, c
	mlt	de			; u*c
	add	a, e			; a = byte 2's low products
	ld	d, l
	ld	e, c
	mlt	de			; l*c
	push	de
	ld	d, l
	ld	e, b
	mlt	de			; l*b
	ld	l, c
	mlt	hl			; h*c
	add	hl, de			; the middle pair
	add	a, h
	ld	h, a			; byte 2's sum on its top
	add	hl, hl			; up a byte
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	pop	de
	add	hl, de			; and l*c
	pop	de
	ret
