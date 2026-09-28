;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_itof
	XDEF	_acc_rt_uitof

	.assume adl=1
	SEGMENT CODE

; --------------------------------------------------- float
; IEEE-754 single precision, which is what agondev's float and double both
; are: a sign bit, eight exponent bits biased by 127, and twenty-three
; mantissa bits below a leading 1 that is not stored. Little endian, so the
; four bytes in memory are
;
;   b0  mantissa 7..0
;   b1  mantissa 15..8
;   b2  exponent bit 0 in bit 7, mantissa 22..16 below it
;   b3  sign in bit 7, exponent 7..1 below it
;
; The same four bytes agondev uses, which is the point: the two compilers
; have to be able to call each other.
;
; All four kinds of value are here: normals, denormals, infinities and NaNs.
; Denormals fill the gap between the smallest normal and zero, so a subtraction
; of two nearby small numbers gives their difference rather than zero; an
; exponent past the top gives an infinity rather than the largest finite
; number; and an operation with no answer -- infinity minus infinity, zero
; times infinity, zero over zero -- gives a NaN rather than something that
; looks like a number.
;
; What is not here is the part of IEEE-754 that is about the machine rather
; than the numbers: there are no exception flags, no rounding modes other than
; to nearest with ties to even, and a signalling NaN is treated as a quiet
; one. C99 makes all of that optional, and <fenv.h> is where a program would
; ask for it.


; hl = the int, de -> the four bytes to write.
;
; Every int on this machine converts exactly. A float keeps twenty-four bits
; of significand counting the one it does not store, and an int here is
; twenty-four bits wide, so there is never anything to round away -- which is
; why this is not the routine a long goes through.
_acc_rt_itof:
	push	iy
	push	bc
	push	hl			; saved
	push	hl			; the working copy, whose bytes need
	ld	iy, 0			; addresses the upper one can be
	add	iy, sp			; reached through

	ld	b, 0			; b = the sign, already in place
	bit	7, (iy + 2)
	jr	z, .itof_magnitude
	ld	b, 0x80
	ld	a, 0
	sub	a, (iy + 0)
	ld	(iy + 0), a
	ld	a, 0			; ld leaves the borrow alone
	sbc	a, (iy + 1)
	ld	(iy + 1), a
	ld	a, 0
	sbc	a, (iy + 2)
	ld	(iy + 2), a
	jr	.itof_magnitude

_acc_rt_uitof:
	push	iy
	push	bc
	push	hl
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	b, 0			; never negative, so no sign and no
					; magnitude to take

.itof_magnitude:
	ld	a, (iy + 0)		; zero is the one value with no leading
	or	a, (iy + 1)		; 1 to find
	or	a, (iy + 2)
	jr	nz, .itof_normalise

	xor	a, a
	ld	(de), a
	inc	de
	ld	(de), a
	inc	de
	ld	(de), a
	inc	de
	ld	(de), a
	dec	de
	dec	de
	dec	de
	jr	.itof_done

.itof_normalise:
	ld	c, 150			; 127 + 23: the exponent when the top
					; bit is already bit 23
.itof_shift:
	bit	7, (iy + 2)
	jr	nz, .itof_pack
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	dec	c
	jr	.itof_shift

.itof_pack:
	res	7, (iy + 2)		; the leading 1 is implied, not stored
	bit	0, c			; and the exponent's low bit takes the
	jr	z, .itof_exp_even	; place it leaves
	set	7, (iy + 2)
.itof_exp_even:
	ld	a, c
	srl	a
	or	a, b			; the sign goes above the exponent
	ld	c, a

	ld	a, (iy + 0)
	ld	(de), a
	inc	de
	ld	a, (iy + 1)
	ld	(de), a
	inc	de
	ld	a, (iy + 2)
	ld	(de), a
	inc	de
	ld	a, c
	ld	(de), a
	dec	de
	dec	de
	dec	de

.itof_done:
	pop	bc			; the working copy, discarded
	pop	hl
	pop	bc
	pop	iy
	ret
