;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fkey
	XDEF	acc_rt_fnorm
	XDEF	acc_rt_funpack

	.assume adl=1
	SEGMENT CODE

_acc_rt_fkey:
	push	iy
	push	bc
	push	hl
	pop	iy

	ld	a, (iy + 0)		; zero, whatever its sign, becomes +0
	or	a, (iy + 1)
	or	a, (iy + 2)
	ld	b, a
	ld	a, (iy + 3)
	and	a, 0x7f
	or	a, b
	jr	nz, .fkey_signed
	ld	(iy + 3), 0

.fkey_signed:
	bit	7, (iy + 3)
	jr	nz, .fkey_negative
	set	7, (iy + 3)		; positive: above every negative
	jr	.fkey_done

.fkey_negative:
	ld	a, (iy + 0)		; negative: below every positive, and
	cpl				; in the opposite order to its bits
	ld	(iy + 0), a
	ld	a, (iy + 1)
	cpl
	ld	(iy + 1), a
	ld	a, (iy + 2)
	cpl
	ld	(iy + 2), a
	ld	a, (iy + 3)
	cpl
	ld	(iy + 3), a

.fkey_done:
	pop	bc
	pop	iy
	ret

; --------------------------------------------------- float add and subtract
; (hl) = (hl) + (de), and the same less the sign of the right operand.
;
; Both significands are unpacked into a thirty-two bit field with the
; twenty-four bits of the number at the top and eight spare bits below them.
; Those eight are what makes the result the one IEEE asks for rather than
; merely close: aligning the smaller operand shifts bits down into them, and
; the bit that falls off the bottom is kept by OR-ing it back into the lowest
; one, so that a value that is not exactly half way up cannot pretend to be.
; Rounding then reads that byte -- above half rounds up, below half truncates,
; and exactly half goes to the even significand, which is the tie rule that
; keeps a long run of sums from drifting.
;
; The frame, reached through ix:
;   ix+0..3   the left significand, the number in bits 31..8
;   ix+4..7   the right, the same way
;   ix+8      the left exponent          ix+9   the right
;   ix+10     the left sign in bit 7     ix+11  the right


; Unpack the float at (hl) into the four bytes at (iy), the exponent into b and
; the sign into c. An exponent of zero is a zero or a denormal, and both come
; out as zero: there are no denormals here.
; The significand at iy shifted up until its leading bit is at the top of
; its four bytes, and in A how many places that took: none for a normal
; number, some for a denormal. Only for one that is not zero.
acc_rt_fnorm:
	xor	a, a
.fnorm_loop:
	bit	7, (iy + 3)
	ret	nz
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	inc	a
	jr	.fnorm_loop

acc_rt_funpack:
	ld	a, (hl)
	ld	(iy + 1), a		; mantissa 7..0
	inc	hl
	ld	a, (hl)
	ld	(iy + 2), a		; mantissa 15..8
	inc	hl
	ld	a, (hl)
	and	a, 0x7f
	or	a, 0x80			; the leading 1, which is not stored
	ld	(iy + 3), a
	ld	(iy + 0), 0		; the eight spare bits below the number
	ld	a, (hl)
	rlca				; the exponent's low bit
	and	a, 1
	ld	b, a
	inc	hl
	ld	a, (hl)
	ld	c, a
	and	a, 0x7f
	add	a, a
	add	a, b
	ld	b, a			; b = the biased exponent
	ld	a, c
	and	a, 0x80
	ld	c, a			; c = the sign, in place
	dec	hl
	dec	hl
	dec	hl

	; What the stored exponent means at its two ends.
	;
	; Zero means there is no leading 1 to put back, so the one written
	; above has to come off again. If what is left is nothing the number
	; is zero; if it is not, the number is a denormal -- a value too small
	; to have a leading 1 of its own, filling in the gap between the
	; smallest normal float and zero. A denormal is 0.mantissa x 2^-126,
	; and a normal at exponent 1 is 1.mantissa x 2^-126, so the two are on
	; the same scale: calling the denormal's exponent 1 and leaving its
	; leading bit clear makes every routine after this treat it like any
	; other number, with no denormal case in the arithmetic at all.
	;
	; 255 means an infinity when the mantissa is empty and a NaN when it
	; is not. The leading 1 comes off there too, so that the caller can
	; tell the two apart by whether the significand is zero.
	ld	a, b
	cp	a, 255
	jr	z, .funpack_special
	or	a, a
	ret	nz

	res	7, (iy + 3)		; no leading 1 after all
	ld	a, (iy + 1)
	or	a, (iy + 2)
	or	a, (iy + 3)
	ret	z			; nothing left: the number is zero
	inc	b			; a denormal, on the same scale as a
	ret				; normal at exponent 1

.funpack_special:
	res	7, (iy + 3)		; zero significand means an infinity,
	ret				; anything else a NaN
