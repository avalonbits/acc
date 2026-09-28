;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llshl

	.assume adl=1
	SEGMENT CODE

; (hl) shifted by the low byte of (de), masked to six bits. A bit at a time,
; each a pass over the eight bytes: rl and rr through (hl) carry the bit from
; one byte into the next, and the passes restart from the saved pointer.
_acc_rt_llshl:
	ld	a, (de)
	and	a, 63
	ret	z
	push	bc
	push	hl
	ld	c, a
.llshl_bit:
	pop	hl
	push	hl
	ld	b, 8
	or	a, a			; a zero comes in at the bottom
.llshl_byte:
	rl	(hl)
	inc	hl
	djnz	.llshl_byte
	dec	c
	jr	nz, .llshl_bit
	pop	hl
	pop	bc
	ret
