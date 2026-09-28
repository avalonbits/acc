;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_rems
	XREF	_acc_rt_udivmod

	.assume adl=1
	SEGMENT CODE

_acc_rt_rems:
	push	de
	push	iy
	ld	e, 0			; the sign of the dividend, in bit 0

	push	hl
	add	hl, hl
	pop	hl
	jr	nc, .rems_num_ok
	ld	e, 1
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.rems_num_ok:
	push	hl
	push	bc
	pop	hl
	add	hl, hl
	pop	hl
	jr	nc, .rems_den_ok
	push	hl			; the divisor's sign does not reach the
	ld	hl, 0			; remainder, but its magnitude is needed
	or	a, a
	sbc	hl, bc
	push	hl
	pop	bc
	pop	hl
.rems_den_ok:
	push	de
	call	_acc_rt_udivmod
	ex	de, hl			; the remainder is the answer
	pop	de
	bit	0, e
	jr	z, .rems_done
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.rems_done:
	pop	iy
	pop	de
	ret
