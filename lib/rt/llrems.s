;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llrems
	XREF	acc_rt_llabs_ix
	XREF	acc_rt_llabs_iy
	XREF	acc_rt_llneg_iy
	XREF	acc_rt_llrem_move
	XREF	acc_rt_lludivmod_core
	XREF	acc_rt_lldiv_buf
	XDEF	_acc_rt_llrems3
	XREF	acc_rt_wcopy8

	.assume adl=1
	SEGMENT CODE

_acc_rt_llrems3:			; the left operand copied in from (bc) first
	call	acc_rt_wcopy8
_acc_rt_llrems:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl
	pop	iy
	ex	de, hl			; the divisor copied, to be made positive
	ld	de, acc_rt_lldiv_buf	; there and not where the caller has it
	ld	bc, 8
	ldir
	lea	hl, iy + 0
	ld	de, acc_rt_lldiv_buf
	ld	ix, acc_rt_lldiv_buf
	ld	b, (iy + 7)
	call	acc_rt_llabs_iy
	call	acc_rt_llabs_ix
	call	acc_rt_lludivmod_core
	call	acc_rt_llrem_move
	bit	7, b
	call	nz, acc_rt_llneg_iy
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret
