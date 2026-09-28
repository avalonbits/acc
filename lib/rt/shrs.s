;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_shrs

	.assume adl=1
	SEGMENT CODE

_acc_rt_shrs:
	ld	a, c
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.shrs_loop:
	sra	(iy + 2)		; the sign is carried down, not zero
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .shrs_loop
	pop	hl
	pop	iy
	ret
