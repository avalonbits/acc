;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrshru
	XDEF	_acc_rt_lrshrs

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl >> a, a taken as 0 to 31: unsigned, a 0 into the top bit, and
; signed, the sign. A bit goes down from e into hl's top byte, which has no
; name: hl is shifted on the stack, iy reaching it, a byte at a time.
_acc_rt_lrshru:
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.lrshru_loop:
	srl	e
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .lrshru_loop
	pop	hl
	pop	iy
	ret

_acc_rt_lrshrs:
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.lrshrs_loop:
	sra	e
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .lrshrs_loop
	pop	hl
	pop	iy
	ret
