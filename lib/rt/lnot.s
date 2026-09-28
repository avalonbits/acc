;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lnot

	.assume adl=1
	SEGMENT CODE

_acc_rt_lnot:
	push	iy
	push	hl
	pop	iy
	ld	a, (iy + 0)
	cpl
	ld	(iy + 0), a
	ld	a, (iy + 1)
	cpl
	ld	(iy + 1), a
	ld	a, (iy + 2)
	cpl
	ld	(iy + 2), a
	ld	a, (iy + 3)
	cpl
	ld	(iy + 3), a
	pop	iy
	ret
