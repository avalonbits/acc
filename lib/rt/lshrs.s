;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lshrs

	.assume adl=1
	SEGMENT CODE

_acc_rt_lshrs:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshrs_loop:
	sra	(iy + 3)		; the sign is carried down, not zero
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	c
	jr	nz, .lshrs_loop
	pop	iy
	pop	bc
	ret
