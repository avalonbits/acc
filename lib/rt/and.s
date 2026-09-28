;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_and

	.assume adl=1
	SEGMENT CODE

; ================================================================ arithmetic

; ---------------------------------------------------------------- bitwise
; hl = hl OP bc, a byte at a time, with both operands on the stack so that
; every byte of them has an address.

_acc_rt_and:
	push	de
	push	iy
	push	bc			; (iy+3..5) once iy is set
	push	hl			; (iy+0..2)
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	and	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	and	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	and	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret
