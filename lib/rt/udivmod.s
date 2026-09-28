;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_udivmod
	XREF	acc_rt_bc_is_zero

	.assume adl=1
	SEGMENT CODE

; ---------------------------------------------------------------- divide
; There is no divide instruction at all, so this is the long way: shift the
; dividend into a remainder a bit at a time and subtract the divisor whenever
; it fits. Twenty-four iterations.
;
; The signed forms reduce to the unsigned one. C99 requires division to
; truncate towards zero and the remainder to take the sign of the dividend,
; which is what taking both magnitudes and fixing the sign afterwards gives.
;
; Dividing by zero is undefined in C. These return zero rather than looping.


; hl = hl / bc, de = hl % bc, both unsigned. The common core.
_acc_rt_udivmod:
	call	acc_rt_bc_is_zero		; all twenty-four bits of it: `ld a, b`
	jr	nz, .div_go		; with `or a, c` reads only sixteen, and
	ld	hl, 0			; called every divisor that is a multiple
	ld	de, 0			; of 65536 nothing at all
	ret
.div_go:
	ld	de, 0			; the remainder
	ld	a, 24
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
