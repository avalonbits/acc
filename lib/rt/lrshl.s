;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrshl

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl << a, a taken as 0 to 31: add hl, hl carries bit 23 into rl e.
_acc_rt_lrshl:
	and	a, 31
	ret	z
.lrshl_loop:
	add	hl, hl
	rl	e
	dec	a
	jr	nz, .lrshl_loop
	ret
