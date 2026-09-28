;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_or

	.assume adl=1
	SEGMENT CODE

_acc_rt_or:
	push	de
	push	iy
	push	bc
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	or	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	or	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	or	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret
