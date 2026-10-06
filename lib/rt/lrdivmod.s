;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	acc_rt_lrdivmod

	.assume adl=1
	SEGMENT CODE

; e:hl / a:bc, both unsigned, in registers: the quotient comes back in
; b:ix and the remainder in c:hl. af, bc, de, hl, ix and iy are not kept --
; lrdivu.s and lrremu.s keep what the routines in registers keep.
;
; The loop of lmul.s's acc_rt_ludivmod_core, which takes its numbers in
; memory: shift and subtract, a bit of quotient a turn, the quotient in
; b:ix, the remainder in c:hl, the divisor in iyl:de and the count in iyh.
; Taking them in registers, there is nothing to copy in and out: called
; through acc_rt_lrcall, a remainder cost the stack frame and both
; operands' copies on top of the division -- Adler-32 takes two a byte.
;
; A dividend's top bytes that are zero are skipped, eight turns each, as
; the core skips them: their turns would only shift zeros through a zero
; remainder. b:ix is shifted up a byte for each.
acc_rt_lrdivmod:
	ld	iyl, a			; the divisor: iyl:de
	push	bc
	ld	b, e			; the dividend: b:ix
	push	hl
	pop	ix
	pop	de
	ld	a, 32
.lrdm_skip:
	ld	iyh, a			; the count
	ld	a, b
	or	a, a
	jr	nz, .lrdm_divisor
	ld	a, iyh
	cp	a, 8
	jr	z, .lrdm_divisor	; the last byte's turns stay
	push	ix			; b = ix's top byte, and ix <<= 8
	ld	hl, 2
	add	hl, sp
	ld	b, (hl)
	pop	ix
	add	ix, ix
	add	ix, ix
	add	ix, ix
	add	ix, ix
	add	ix, ix
	add	ix, ix
	add	ix, ix
	add	ix, ix
	sub	a, 8
	jr	.lrdm_skip

.lrdm_divisor:
	ld	a, iyl			; a zero divisor is undefined in C:
	or	a, a			; say zero for both rather than loop
	jr	nz, .lrdm_go
	sbc	hl, hl
	sbc	hl, de
	jr	nz, .lrdm_go
	ld	ix, 0
	ld	b, 0
	ld	c, 0
	ret

.lrdm_go:
	or	a, a
	sbc	hl, hl			; the remainder: c:hl = 0
	ld	c, l

.lrdm_loop:
	add	ix, ix			; {remainder:quotient} <<= 1
	rl	b
	adc	hl, hl
	rl	c			; which leaves carry clear
	sbc	hl, de
	ld	a, c
	sbc	a, iyl
	jr	c, .lrdm_back
	ld	c, a
	inc	ix			; the quotient's bit
	dec	iyh
	jr	nz, .lrdm_loop
	ret
.lrdm_back:
	add	hl, de			; c was never written
	dec	iyh
	jr	nz, .lrdm_loop
	ret
