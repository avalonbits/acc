;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lxor
	XDEF	acc_rt_lxor_n
	XDEF	_acc_rt_lxor3
	XREF	acc_rt_wcopy4

	.assume adl=1
	SEGMENT CODE

_acc_rt_lxor3:			; the left operand copied in from (bc) first
	call	acc_rt_wcopy4
_acc_rt_lxor:
	push	bc
	push	de
	push	hl
	ld	b, 4
acc_rt_lxor_n:
.lxor_loop:
	ld	a, (de)
	xor	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lxor_loop
	pop	hl
	pop	de
	pop	bc
	ret
