;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lremu
	XREF	acc_rt_ludivmod_core

	.assume adl=1
	SEGMENT CODE

_acc_rt_lremu:
	push	ix
	push	iy
	push	hl
	pop	iy
	push	de
	pop	ix
	call	acc_rt_ludivmod_core
	ld	a, (ix + 0)		; the remainder is the answer, and the
	ld	(iy + 0), a		; core left it where the divisor was
	ld	a, (ix + 1)
	ld	(iy + 1), a
	ld	a, (ix + 2)
	ld	(iy + 2), a
	ld	a, (ix + 3)
	ld	(iy + 3), a
	pop	iy
	pop	ix
	ret
