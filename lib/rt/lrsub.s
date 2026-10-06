;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrsub

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl - a:bc. The top byte is e + ~a + the carry not borrowed --
; e - a - the borrow -- without a register to hold a in: cpl leaves the
; carry, and ccf turns the borrow into the carry adc wants.
_acc_rt_lrsub:
	or	a, a
	sbc	hl, bc
	cpl
	ccf
	adc	a, e
	ld	e, a
	ret
