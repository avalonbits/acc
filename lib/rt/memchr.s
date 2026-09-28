;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_memchr
	XREF	acc_rt_bc_is_zero

	.assume adl=1
	SEGMENT CODE

; Looking for a byte, which cpir does in one instruction: compare A with
; what HL points at, step HL on, count BC down, and stop at the first match
; or when the count runs out. It leaves HL one past what it found, and Z set
; only if it found it.
;
;	hl = where to look, a = the byte, bc = how many
;	returns hl = the byte, or nothing at all
_acc_rt_memchr:
	call	acc_rt_bc_is_zero
	jr	z, _rt_chr_none
	cpir
	jr	nz, _rt_chr_none
	dec	hl			; cpir stepped past what it matched
	ret
_rt_chr_none:
	ld	hl, 0
	ret
