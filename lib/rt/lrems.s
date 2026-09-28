;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrems
	XREF	acc_rt_labs_ix
	XREF	acc_rt_labs_iy
	XREF	acc_rt_lneg_iy
	XREF	acc_rt_ludivmod_core

	.assume adl=1
	SEGMENT CODE

_acc_rt_lrems:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)		; the remainder takes the dividend's
	and	a, 0x80			; sign
	ld	b, a
	call	acc_rt_labs_iy
	call	acc_rt_labs_ix
	call	acc_rt_ludivmod_core
	ld	a, (ix + 0)
	ld	(iy + 0), a
	ld	a, (ix + 1)
	ld	(iy + 1), a
	ld	a, (ix + 2)
	ld	(iy + 2), a
	ld	a, (ix + 3)
	ld	(iy + 3), a
	bit	7, b
	call	nz, acc_rt_lneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret
