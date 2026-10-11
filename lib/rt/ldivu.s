;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ldivu
	XREF	acc_rt_ludivmod_core
	XDEF	_acc_rt_ldivu3
	XREF	acc_rt_wcopy4

	.assume adl=1
	SEGMENT CODE

_acc_rt_ldivu3:			; the left operand copied in from (bc) first
	call	acc_rt_wcopy4
_acc_rt_ldivu:
	push	iy
	push	hl
	pop	iy
	call	acc_rt_ludivmod_core
	pop	iy
	ret
