;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lldivs
	XREF	acc_rt_llabs_ix
	XREF	acc_rt_llabs_iy
	XREF	acc_rt_llneg_iy
	XREF	acc_rt_lludivmod_core

	.assume adl=1
	SEGMENT CODE

; Magnitudes divided and the sign put back, as for a long: the quotient is
; negative when the operands differ in sign, the remainder takes the
; dividend's.
_acc_rt_lldivs:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 7)
	xor	a, (ix + 7)
	ld	b, a
	call	acc_rt_llabs_iy
	call	acc_rt_llabs_ix
	call	acc_rt_lludivmod_core
	bit	7, b
	call	nz, acc_rt_llneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret
