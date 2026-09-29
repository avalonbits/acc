;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_shr

	.assume adl=1
	SEGMENT CODE

; HL >> k for a constant k from 1 to 8, called at _acc_rt_shr + 2(k - 1),
; the entry for k below, with A holding what fills from the top: 0 for an unsigned value, its sign in
; every bit for a signed one. Nothing shifts HL's top byte right, so A:HL
; is shifted left by 8 - k instead and its top three bytes taken, which
; is the value shifted right by k. Keeps everything but HL, A and flags.

_acc_rt_shr:			; k = 1
	add	hl, hl
	rla
.shr2:				; k = 2
	add	hl, hl
	rla
.shr3:				; k = 3
	add	hl, hl
	rla
.shr4:				; k = 4
	add	hl, hl
	rla
.shr5:				; k = 5
	add	hl, hl
	rla
.shr6:				; k = 6
	add	hl, hl
	rla
.shr7:				; k = 7
	add	hl, hl
	rla
.shr8:				; k = 8
	; HL = A:HL >> 8, through the stack. A goes where HL's top byte will
	; be read from, HL goes a byte below it, and a pop from one byte up
	; reads H, U and A. Nothing it reads is ever below SP.
	push	af
	inc	sp
	push	hl
	inc	sp
	pop	hl
	inc	sp
	ret
