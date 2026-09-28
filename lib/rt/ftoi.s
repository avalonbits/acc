;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ftoi
	XDEF	acc_rt_fisnan

	.assume adl=1
	SEGMENT CODE

; hl -> the float; the int comes back in hl, truncated towards zero as C says.
; A value too large for an int is undefined in C and is left to wrap.
_acc_rt_ftoi:
	push	iy
	push	bc
	push	de
	push	hl
	pop	iy			; iy -> the float

	ld	a, (iy + 3)		; the exponent, which is split across
	and	a, 0x7f			; two bytes with the sign above it
	add	a, a
	ld	b, a
	ld	a, (iy + 2)
	rlca
	and	a, 1
	add	a, b
	ld	b, a			; b = the biased exponent

	ld	c, 0			; c = the sign
	bit	7, (iy + 3)
	jr	z, .ftoi_significand
	ld	c, 1

.ftoi_significand:
	ld	e, (iy + 0)		; the stored mantissa with its leading
	ld	d, (iy + 1)		; 1 put back
	ld	a, (iy + 2)
	and	a, 0x7f
	or	a, 0x80

	ld	hl, -3			; somewhere the three bytes can be
	add	hl, sp			; shifted as one value
	ld	sp, hl
	push	hl
	pop	iy
	ld	(iy + 0), e
	ld	(iy + 1), d
	ld	(iy + 2), a

	ld	a, b
	cp	a, 127			; below 1, so the integer part is 0
	jr	c, .ftoi_zero
	sub	a, 127			; how far the point has moved right
	ld	b, a
	ld	a, 23
	sub	a, b
	jr	c, .ftoi_left		; past the top of the significand

	or	a, a			; already an integer when a is zero
	jr	z, .ftoi_signed
	ld	b, a
.ftoi_right:
	srl	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	djnz	.ftoi_right
	jr	.ftoi_signed

.ftoi_left:
	neg				; a was 23 - shift and went negative
	ld	b, a
.ftoi_left_loop:
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	djnz	.ftoi_left_loop
	jr	.ftoi_signed

.ftoi_zero:
	ld	(iy + 0), 0
	ld	(iy + 1), 0
	ld	(iy + 2), 0

.ftoi_signed:
	bit	0, c
	jr	z, .ftoi_out
	ld	a, 0
	sub	a, (iy + 0)
	ld	(iy + 0), a
	ld	a, 0
	sbc	a, (iy + 1)
	ld	(iy + 1), a
	ld	a, 0
	sbc	a, (iy + 2)
	ld	(iy + 2), a

.ftoi_out:
	ld	hl, (iy + 0)
	ld	iy, 3			; give the three bytes back
	add	iy, sp
	ld	sp, iy
	pop	de
	pop	bc
	pop	iy
	ret

; The four bytes at (hl) rewritten as an unsigned key that sorts the same way
; the float does. Comparing floats is then the long comparison already here,
; rather than a second four-byte compare that knows about exponents.
;
; IEEE-754 was laid out so that the bits of two floats of the same sign
; already compare as integers -- the exponent sits above the mantissa for
; exactly that reason. Only the sign spoils it, and only in two ways: a
; negative float has its top bit set where a positive one does not, and among
; negatives the order runs backwards. Setting the top bit of a positive and
; complementing the whole of a negative fixes both.
;
; Zero is the case that needs saying out loud. -0.0 and 0.0 are different
; bytes and the same number, so -0.0 is turned into 0.0 before the transform
; and the two come out with one key. Without that they would be different
; keys, and `-0.0 < 0.0` would be true.


; Z clear if the float at (hl) is a NaN: every exponent bit set, which an
; infinity also has, and a mantissa that is not empty, which only a NaN has.
acc_rt_fisnan:
	push	iy
	push	hl
	pop	iy
	ld	a, (iy + 3)
	and	a, 0x7f
	cp	a, 0x7f
	jr	nz, .fisnan_no
	bit	7, (iy + 2)		; the exponent's low bit
	jr	z, .fisnan_no
	ld	a, (iy + 2)		; the mantissa, which an infinity leaves
	and	a, 0x7f			; empty
	or	a, (iy + 1)
	or	a, (iy + 0)
	pop	iy
	ret

.fisnan_no:
	xor	a, a
	pop	iy
	ret
