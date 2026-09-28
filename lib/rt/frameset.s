;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_frameset

	.assume adl=1
	SEGMENT CODE

; ================================================================ the frame
; The prologue every function begins with, as agondev's __frameset: called
; with HL the frame's size, negated, it saves IX, points it at the saved
; IX, and makes room for the frame below. The return address is taken off
; first and jumped to at the end, so the frame starts where the caller's
; call left SP. HL and DE go; nothing is in them at a function's entry.
; frameset0 is the same for a function with no frame.


_acc_rt_frameset:
	pop de
	push ix
	ld ix, 0
	add ix, sp
	add hl, sp
	ld sp, hl
	ex de, hl
	jp (hl)
