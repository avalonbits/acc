;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_divs
	XREF	_acc_rt_udivmod

	.assume adl=1
	SEGMENT CODE

; The signed forms reduce to the unsigned one: divide the magnitudes, then
; fix the sign. C99 requires the quotient to truncate towards zero and the
; remainder to take the sign of the dividend, which is what that gives.
;
; The sign of a 24-bit value is bit 23, which lives in the byte of HL that has
; no name -- but `add hl, hl` shifts it into the carry, and a push and a pop
; either side of that leaves the value untouched.

_acc_rt_divs:
	push	de
	push	iy
	ld	e, 0			; the sign of the result, in bit 0

	push	hl
	add	hl, hl
	pop	hl			; carry = the dividend is negative
	jr	nc, .divs_num_ok
	ld	e, 1
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.divs_num_ok:
	push	hl
	push	bc
	pop	hl
	add	hl, hl
	pop	hl			; carry = the divisor is negative
	jr	nc, .divs_den_ok
	ld	a, e
	xor	a, 1
	ld	e, a
	push	hl
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	push	hl
	pop	bc
	pop	hl
.divs_den_ok:
	push	de			; udivmod returns the remainder in de
	call	_acc_rt_udivmod
	pop	de
	bit	0, e
	jr	z, .divs_done
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.divs_done:
	pop	iy
	pop	de
	ret
