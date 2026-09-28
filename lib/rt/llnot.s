;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llnot

	.assume adl=1
	SEGMENT CODE

_acc_rt_llnot:
	push	bc
	push	hl
	ld	b, 8
.llnot_loop:
	ld	a, (hl)
	cpl
	ld	(hl), a
	inc	hl
	djnz	.llnot_loop
	pop	hl
	pop	bc
	ret
