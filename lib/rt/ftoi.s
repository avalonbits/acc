;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
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
	ld	d, (iy + 1)		; 1 put back, in A, D and E, top first
	ld	a, (iy + 2)
	and	a, 0x7f
	or	a, 0x80
	ld	hl, 0			; zero, for a value below 1

	ld	iy, -3			; somewhere the three bytes can be put
	add	iy, sp			; together as one value
	ld	sp, iy

	push	af
	ld	a, b
	cp	a, 127			; below 1, so the integer part is 0
	jr	c, .ftoi_zero
	sub	a, 150			; how far the point is from the bottom
	jr	nc, .ftoi_left		; at or past it: already whole, or more
	neg				; places to go right, 1 to 23
	ld	b, a
	pop	af
.ftoi_bytes:
	ld	h, a			; eight at a time while there are
	ld	a, b
	cp	a, 8
	ld	a, h
	jr	c, .ftoi_bits
	ld	e, d
	ld	d, a
	xor	a, a
	ld	h, a
	ld	a, b
	sub	a, 8
	ld	b, a
	ld	a, h
	jr	nz, .ftoi_bytes
	jr	.ftoi_place
.ftoi_bits:
	srl	a			; and then one at a time
	rr	d
	rr	e
	djnz	.ftoi_bits
	jr	.ftoi_place

.ftoi_left:
	ld	b, a			; places to go left, which a value this
	pop	af			; large for an int wraps through
	inc	b
	dec	b
	jr	z, .ftoi_place
.ftoi_left_loop:
	sla	e
	rl	d
	rla
	djnz	.ftoi_left_loop

.ftoi_place:
	ld	(iy + 0), e
	ld	(iy + 1), d
	ld	(iy + 2), a
	ld	hl, (iy + 0)
	bit	0, c			; and the sign, as a subtract from zero
	jr	z, .ftoi_out
	ex	de, hl
	or	a, a
	sbc	hl, hl
	sbc	hl, de
	jr	.ftoi_out

.ftoi_zero:
	pop	af

.ftoi_out:
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
