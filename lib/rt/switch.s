;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_switch

	.assume adl=1
	SEGMENT CODE

; A switch on an int, by table: the value in HL, and after the call a
; three-byte count, never 0 and never past two bytes, and that many entries of a case's value and its
; address, three bytes each. Jumps to the address of the entry whose value
; is HL's, or returns past the table, to what the switch does for none --
; with nothing kept but IX and IY, nothing being live where a switch jumps.
; An entry is passed over on its low byte, which tells most apart, and
; compared whole only where that is the same.
_acc_rt_switch:
	ex	(sp), hl		; HL the table, the value on the stack
	pop	de			; DE the value
	ld	c, (hl)
	inc	hl
	ld	b, (hl)			; BC the count
	inc	hl
	inc	hl
.switch_next:
	ld	a, (hl)
	cp	a, e
	jr	z, .switch_low
.switch_skip:
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	dec	bc
	ld	a, b
	or	a, c
	jr	nz, .switch_next
	jp	(hl)			; none: past the table
.switch_low:
	push	hl
	ld	hl, (hl)		; the entry's value, whole
	or	a, a
	sbc	hl, de
	pop	hl
	jr	nz, .switch_skip
	inc	hl
	inc	hl
	inc	hl
	ld	hl, (hl)		; its address
	jp	(hl)
