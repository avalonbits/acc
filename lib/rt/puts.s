;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_puts

	.assume adl=1
	SEGMENT CODE

; Writing a run of bytes is a different restart: the buffer in HL, how many
; in BC, and the byte it stops at in A.
_acc_rt_puts:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+6)
	ld	bc, (ix+9)
	ld	a, (ix+12)
	rst.lil	$18
	pop	ix
	ret
