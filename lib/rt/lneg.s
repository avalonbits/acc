;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lneg
	XREF	acc_rt_lneg_iy

	.assume adl=1
	SEGMENT CODE

; --------------------------------------------------- long unary
; (hl) = -(hl) and (hl) = ~(hl), four bytes.
;
; The 24-bit forms are `0 - x` in HL and the same less one, which the chip can
; do in a register. A long cannot be held in one, so these work in the frame
; like the rest of the long routines.


_acc_rt_lneg:
	push	iy
	push	hl
	pop	iy
	call	acc_rt_lneg_iy
	pop	iy
	ret
