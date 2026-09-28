;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llrems
	XREF	acc_rt_llabs_ix
	XREF	acc_rt_llabs_iy
	XREF	acc_rt_llneg_iy
	XREF	acc_rt_llrem_move
	XREF	acc_rt_lludivmod_core

	.assume adl=1
	SEGMENT CODE

_acc_rt_llrems:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	b, (iy + 7)
	call	acc_rt_llabs_iy
	call	acc_rt_llabs_ix
	call	acc_rt_lludivmod_core
	call	acc_rt_llrem_move
	bit	7, b
	call	nz, acc_rt_llneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret
