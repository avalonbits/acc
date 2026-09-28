;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ladd
	XDEF	acc_rt_ladd_n

	.assume adl=1
	SEGMENT CODE

; ---------------------------------------------------------------- long
; A long is four bytes, wider than any register, so it lives in the frame and
; these work on it there: HL points at the destination, DE at the other
; operand, and the destination is overwritten.
;
; Little endian, so the loop runs from the low byte up and the carry chains
; the way the arithmetic needs.


_acc_rt_ladd:
	push	bc
	push	de
	push	hl
	ld	b, 4
acc_rt_ladd_n:				; b bytes: the long long one comes in here
	or	a, a			; no carry into the low byte
.ladd_loop:
	ld	a, (de)
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.ladd_loop
	pop	hl
	pop	de
	pop	bc
	ret
