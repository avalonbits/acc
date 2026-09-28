;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	acc_rt_bc_is_zero

	.assume adl=1
	SEGMENT CODE

; Z when BC is nothing. Clears the carry either way, which memmove reads.
; HL is wanted by all three callers, so the test goes through the stack
; rather than through it.
acc_rt_bc_is_zero:
	push	hl
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	hl
	ret
