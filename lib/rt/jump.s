;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_jump

	.assume adl=1
	SEGMENT CODE

; A switch on an int whose cases lie close together, by a table indexed by
; the value: in HL, and after the call the number of values from the
; lowest case's, a byte, the lowest, three bytes, and that many addresses
; and one more, three bytes each -- a case's, or past the table for a value
; no case has, which the last one is for a value outside them. Nothing
; kept but IX and IY.
_acc_rt_jump:
	ex	(sp), hl		; HL the table, the value on the stack
	ld	bc, 0
	ld	c, (hl)			; the count
	inc	hl
	ld	de, (hl)		; the lowest
	inc	hl
	inc	hl
	inc	hl
	ex	(sp), hl		; the value, the table on the stack
	or	a, a
	sbc	hl, de			; how far past the lowest
	pop	de
	or	a, a
	sbc	hl, bc
	add	hl, bc
	jr	c, .jump_in
	push	bc			; outside: the one past the cases
	pop	hl
.jump_in:
	push	hl
	pop	bc
	add	hl, hl
	add	hl, bc			; three bytes an address
	add	hl, de
	ld	hl, (hl)
	jp	(hl)
