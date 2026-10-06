;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrneg
	XDEF	_acc_rt_lrnot

	.assume adl=1
	SEGMENT CODE

; e:hl = -e:hl: 0 - hl, its borrow taken from 0 - e.
_acc_rt_lrneg:
	push	bc
	push	hl
	pop	bc
	or	a, a
	sbc	hl, hl
	sbc	hl, bc
	ld	a, 0
	sbc	a, e
	ld	e, a
	pop	bc
	ret

; e:hl = ~e:hl: -1 - hl, and e complemented.
_acc_rt_lrnot:
	ld	a, e
	cpl
	ld	e, a
	push	bc
	push	hl
	pop	bc
	scf
	sbc	hl, hl			; -1, the carry kept
	or	a, a
	sbc	hl, bc
	pop	bc
	ret
