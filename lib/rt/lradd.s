;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lradd

	.assume adl=1
	SEGMENT CODE

; ------------------------------------------------- long, in registers
; The routines named _acc_rt_lr... take a long in registers, as agondev's do:
; the left operand, and the answer, in E:UHL -- E the top byte, HL the low
; three -- and the right in A:UBC. They keep BC, D, IX and IY; A and the
; flags they do not keep. See lib/rt/README.md.

; e:hl = e:hl + a:bc
_acc_rt_lradd:
	add	hl, bc
	adc	a, e
	ld	e, a
	ret
