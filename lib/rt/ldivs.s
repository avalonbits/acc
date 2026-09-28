;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ldivs
	XREF	acc_rt_labs_ix
	XREF	acc_rt_labs_iy
	XREF	acc_rt_lneg_iy
	XREF	acc_rt_ludivmod_core

	.assume adl=1
	SEGMENT CODE

; C99 has division truncate towards zero and the remainder take the sign of
; the dividend, which is what dividing the magnitudes and fixing the sign
; afterwards gives.
_acc_rt_ldivs:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)		; the quotient is negative when the
	xor	a, (ix + 3)		; operands differ in sign
	and	a, 0x80
	ld	b, a
	call	acc_rt_labs_iy
	call	acc_rt_labs_ix
	call	acc_rt_ludivmod_core
	bit	7, b
	call	nz, acc_rt_lneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret
