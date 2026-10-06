;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrremu
	XREF	_acc_rt_lremu
	XREF	acc_rt_lrcall

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl % a:bc, unsigned: _acc_rt_lremu, in memory, through
; acc_rt_lrcall.
_acc_rt_lrremu:
	push	hl
	ld	hl, _acc_rt_lremu
	ex	(sp), hl
	jp	acc_rt_lrcall
