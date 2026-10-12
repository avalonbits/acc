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
	XREF	acc_rt_ldiv_buf
	XDEF	_acc_rt_lrems3
	XREF	acc_rt_wcopy4

	.assume adl=1
	SEGMENT CODE

_acc_rt_lrems3:			; the left operand copied in from (bc) first
	call	acc_rt_wcopy4
_acc_rt_lrems:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl
	pop	iy
	ex	de, hl			; the divisor copied, to be made positive
	ld	de, acc_rt_ldiv_buf	; there and not where the caller has it
	ld	bc, 4
	ldir
	lea	hl, iy + 0
	ld	de, acc_rt_ldiv_buf
	ld	ix, acc_rt_ldiv_buf
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
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret
