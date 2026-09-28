;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lcmpord
	XDEF	acc_rt_lcmpord_n

	.assume adl=1
	SEGMENT CODE

; The flags of (hl) - (de) as a four-byte subtract, for ordering. The last
; sbc leaves S, P/V and C describing the whole width, which is what the
; caller's branch sequence reads. Neither operand is changed.
_acc_rt_lcmpord:
	push	bc
	push	de
	push	hl
	ld	b, 4
acc_rt_lcmpord_n:
	or	a, a
.lcmpord_loop:
	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	inc	hl
	inc	de
	djnz	.lcmpord_loop
	pop	hl			; pop leaves the flags alone
	pop	de
	pop	bc
	ret
