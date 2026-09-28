;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lladd
	XREF	acc_rt_ladd_n

	.assume adl=1
	SEGMENT CODE

; ================================================================ long long
; A long long is eight bytes and lives in the frame as a long does, so these
; take the same shape: HL points at the destination and left operand, DE at
; the right one, and everything but the destination is preserved. Where a
; long routine is a loop over its bytes, the long long one is the same loop
; entered with eight in B.


_acc_rt_lladd:
	push	bc
	push	de
	push	hl
	ld	b, 8
	jp	acc_rt_ladd_n
