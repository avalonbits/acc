;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_sdiv

	.assume adl=1
	SEGMENT CODE

; A signed HL / 2^k for k from 1 to 8, called at _acc_rt_sdiv + 2(k - 1),
; the entry for k below, with A holding HL's sign in every bit. It is shrk.s's shift, which rounds down,
; and then one more for a negative value that had bits shifted out, since
; division rounds toward zero. Those bits are in L once the shifts are
; done, at its top, and zeros below them. Keeps everything but HL, A and
; flags.

_acc_rt_sdiv:			; k = 1
	add	hl, hl
	rla
.sdiv2:				; k = 2
	add	hl, hl
	rla
.sdiv3:				; k = 3
	add	hl, hl
	rla
.sdiv4:				; k = 4
	add	hl, hl
	rla
.sdiv5:				; k = 5
	add	hl, hl
	rla
.sdiv6:				; k = 6
	add	hl, hl
	rla
.sdiv7:				; k = 7
	add	hl, hl
	rla
.sdiv8:				; k = 8
	bit	7, a			; Z: not negative, and nothing to add
	jr	z, .sdiv_take
	inc	l
	dec	l			; Z: negative, but nothing shifted out
.sdiv_take:
	push	af			; as in shrk.s; none of these moves a flag
	inc	sp
	push	hl
	inc	sp
	pop	hl
	inc	sp
	ret	z
	inc	hl
	ret
