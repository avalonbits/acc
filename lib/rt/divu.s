;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_divu
	XREF	_acc_rt_udivmod

	.assume adl=1
	SEGMENT CODE

_acc_rt_divu:
	push	de
	call	_acc_rt_udivmod
	pop	de
	ret
