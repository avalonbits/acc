;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_udivmod

	.assume adl=1
	SEGMENT CODE

; ---------------------------------------------------------------- divide
; There is no divide instruction at all, so this is the long way: shift the
; dividend into a remainder a bit at a time and subtract the divisor whenever
; it fits. Twenty-four iterations -- sixteen, or eight, where the dividend's
; top byte, or two, is zero: a zero shifted into the remainder adds nothing
; to it or to the quotient, so those rounds are taken eight at a time, the
; dividend shifted up a byte. AED's undo ring takes a remainder of an
; offset under 65536 at every byte it reads back, and division was 30% of
; its benchmark.
;
; The signed forms reduce to the unsigned one. C99 requires division to
; truncate towards zero and the remainder to take the sign of the dividend,
; which is what taking both magnitudes and fixing the sign afterwards gives.
;
; Dividing by zero is undefined in C. These return zero rather than looping.


; hl = hl / bc, de = hl % bc, both unsigned. The common core.
_acc_rt_udivmod:
	push	hl			; all twenty-four bits of the divisor, here
	ld	hl, 0			; rather than through acc_rt_bc_is_zero,
	or	a, a			; a call and a return on every divide
	sbc	hl, bc
	pop	hl
	jr	nz, .div_go
	ld	hl, 0
	ld	de, 0
	ret
.div_go:
	ld	a, 24
	ld	de, 010000h
.div_skip:
	or	a, a			; carry out of the add below: the dividend
	sbc	hl, de			; is under 65536, its top byte zero
	add	hl, de
	jr	nc, .div_start
	add	hl, hl			; a byte up, eight rounds fewer
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	sub	a, 8
	cp	a, 8			; down to eight: what is left of it is a byte
	jr	nz, .div_skip
.div_start:
	ld	de, 0			; the remainder
.div_loop:
	add	hl, hl			; the quotient shifts in at the bottom
	ex	de, hl
	adc	hl, hl			; remainder = remainder * 2 + the bit out
	; That shift cannot carry out of twenty-four bits. Before round i the
	; remainder holds the top i-1 bits of the dividend taken modulo the
	; divisor, so it is below 2^(i-1); at the last round that is below
	; 2^23, and twice it still fits. So the carry is clear here and the
	; subtract below needs nothing to clear it.
	sbc	hl, bc
	jr	nc, .div_fits
	add	hl, bc			; it did not fit: put the remainder back
	ex	de, hl
	jr	.div_next
.div_fits:
	ex	de, hl
	inc	l			; the bit fits, so record it. L is even
					; here, so this cannot carry into H
.div_next:
	dec	a
	jr	nz, .div_loop
	ret
