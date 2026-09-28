;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_mos_ix

	.assume adl=1
	SEGMENT CODE

; The system variables come back in IX, which has to be got out from under
; the saved one: push what MOS gave, take it into HL, then put the old one
; back.
_acc_rt_mos_ix:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+9)
	ld	de, (ix+12)
	ld	bc, (ix+15)
	ld	a, (ix+6)
	rst.lil	$08
	push	ix
	pop	hl
	pop	ix
	ret
