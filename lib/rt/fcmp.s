;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fcmp

	.assume adl=1
	SEGMENT CODE

;
; How the float at (hl) stands to the one at (de), in a: 0 below, 1 equal, 2
; above, 3 unordered. Neither operand is written to, so the caller may pass
; them where they are. Keeps everything but a and the flags.
;
; A NaN on either side is unordered. Otherwise the order of two floats is
; the order of their magnitudes, as unsigned numbers, when their signs agree
; -- turned round when both are negative -- and the sign's when they differ,
; except that +0 and -0 are equal.
;
_acc_rt_fcmp:
	push	ix
	push	iy
	push	hl
	pop	ix
	push	de
	pop	iy

	ld	a, (ix + 3)		; a NaN: every exponent bit set, and a
	or	a, 0x80			; mantissa that is not empty. The sign
	inc	a			; set, the top byte's seven are all ones
					; when this comes to zero
	jr	nz, .fcmp_left_ok
	bit	7, (ix + 2)
	jr	z, .fcmp_left_ok
	ld	a, (ix + 2)
	and	a, 0x7f
	or	a, (ix + 1)
	or	a, (ix + 0)
	jp	nz, .fcmp_unordered
.fcmp_left_ok:
	ld	a, (iy + 3)
	or	a, 0x80
	inc	a
	jr	nz, .fcmp_right_ok
	bit	7, (iy + 2)
	jr	z, .fcmp_right_ok
	ld	a, (iy + 2)
	and	a, 0x7f
	or	a, (iy + 1)
	or	a, (iy + 0)
	jp	nz, .fcmp_unordered
.fcmp_right_ok:

	ld	a, (ix + 3)
	xor	a, (iy + 3)
	jp	m, .fcmp_signs

	ld	a, (ix + 3)		; the same sign: the magnitudes, from the
	cp	a, (iy + 3)		; top byte down
	jr	nz, .fcmp_magnitude
	ld	a, (ix + 2)
	cp	a, (iy + 2)
	jr	nz, .fcmp_magnitude
	ld	a, (ix + 1)
	cp	a, (iy + 1)
	jr	nz, .fcmp_magnitude
	ld	a, (ix + 0)
	cp	a, (iy + 0)
	jr	nz, .fcmp_magnitude
	ld	a, 1
	jr	.fcmp_done

.fcmp_magnitude:
	jr	c, .fcmp_smaller	; the left's magnitude is below the right's
	bit	7, (ix + 3)		; larger: above if positive
	jr	z, .fcmp_above
	jr	.fcmp_below
.fcmp_smaller:
	bit	7, (ix + 3)
	jr	z, .fcmp_below

.fcmp_above:
	ld	a, 2
	jr	.fcmp_done

.fcmp_signs:
	ld	a, (ix + 3)		; different signs: two zeros are equal,
	or	a, (iy + 3)		; and otherwise the negative one is below
	and	a, 0x7f
	or	a, (ix + 2)
	or	a, (ix + 1)
	or	a, (ix + 0)
	or	a, (iy + 2)
	or	a, (iy + 1)
	or	a, (iy + 0)
	ld	a, 1
	jr	z, .fcmp_done
	bit	7, (ix + 3)
	jr	z, .fcmp_above

.fcmp_below:
	xor	a, a
	jr	.fcmp_done

.fcmp_unordered:
	ld	a, 3

.fcmp_done:
	pop	iy
	pop	ix
	ret
