;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrremu
	XREF	acc_rt_lrdivmod

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl % a:bc, unsigned, in registers (lrdivmod.s): the remainder,
; which comes back in c:hl. bc, d, ix and iy kept.
_acc_rt_lrremu:
	push	iy
	push	ix
	push	bc
	push	de
	call	acc_rt_lrdivmod
	ld	a, c
	pop	de
	ld	e, a
	pop	bc
	pop	ix
	pop	iy
	ret
