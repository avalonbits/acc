;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fdiv
	XREF	acc_rt_fadd_infinity
	XREF	acc_rt_fadd_nan
	XREF	acc_rt_fadd_round
	XREF	acc_rt_fadd_subnormal
	XREF	acc_rt_fadd_zero
	XREF	acc_rt_fnorm
	XREF	acc_rt_funpack

	.assume adl=1
	SEGMENT CODE

; --------------------------------------------------- float divide
; (hl) = (hl) / (de).
;
; Both significands are twenty-four bits with the leading one set, so their
; ratio is between a half and two and the quotient needs at most one place of
; shifting -- which is decided up front by comparing them, and shows up as
; which of two biases the exponent takes.
;
; The division itself is the restoring loop the integers use, except that
; everything in it fits in a register: hl holds the remainder and de the
; divisor, both twenty-four bits, so a step is `add hl, hl` and one `sbc`.
; Doubling the remainder can carry out of twenty-four bits, and as in the long
; division that carry means the divisor fits whatever the borrow says.
;
; Twenty-four bits of quotient are produced, then one more as the guard bit,
; and whether anything is left over afterwards is the sticky: a remainder of
; zero with the guard set is a true tie and goes to the even significand,
; while any remainder at all makes it round up.
;
; The frame is fadd's, so the packing at the end of fadd finishes this too.


_acc_rt_fdiv:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24
	add	ix, sp
	ld	sp, ix
	ld	(ix + 12), hl

	push	ix
	pop	iy
	call	acc_rt_funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl
	call	acc_rt_funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	ld	a, (ix + 10)		; the sign is the two signs differing
	xor	a, (ix + 11)
	and	a, 0x80
	ld	(ix + 10), a

	ld	a, (ix + 8)		; an infinity or a NaN, as in fadd
	cp	a, 255
	jp	z, .fdiv_left_special
	ld	a, (ix + 9)
	cp	a, 255
	jp	z, .fdiv_right_special

	ld	a, (ix + 8)
	or	a, a
	jr	nz, .fdiv_nonzero
	ld	a, (ix + 9)		; zero over zero has no answer; zero
	or	a, a			; over anything else is zero
	jp	z, acc_rt_fadd_nan
	jp	acc_rt_fadd_zero

.fdiv_nonzero:
	ld	a, (ix + 9)		; a finite number over zero is an
	or	a, a			; infinity, which is what IEEE says and
	jp	z, acc_rt_fadd_infinity	; C leaves undefined

	jp	.fdiv_finite

.fdiv_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, acc_rt_fadd_nan

	ld	a, (ix + 9)		; an infinity over an infinity has no
	cp	a, 255			; answer; over anything else it is an
	jp	nz, acc_rt_fadd_infinity	; infinity
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan
	jp	acc_rt_fadd_nan

.fdiv_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan
	jp	acc_rt_fadd_zero		; a finite number over an infinity

.fdiv_finite:
	; Denormals shifted up to a leading bit, as in fmul: the dividend's
	; places come off the exponent and the divisor's go on.
	push	ix
	pop	iy
	call	acc_rt_fnorm
	ld	(ix + 15), a
	lea	iy, ix + 4
	call	acc_rt_fnorm
	ld	(ix + 23), a

	; Which bias the exponent takes, and how many bits the loop has to
	; produce: a dividend at least the divisor gives its leading 1 at once,
	; and one fewer iteration is needed for the same twenty-four bits.
	ld	de, (ix + 5)		; the divisor's significand
	ld	hl, (ix + 1)		; the dividend's
	or	a, a
	sbc	hl, de
	jr	nc, .fdiv_ge

	add	hl, de			; smaller: the subtract did not happen,
	ld	(ix + 16), hl		; so the whole dividend is the remainder
	ld	(ix + 0), 0		; and the whole quotient comes from the
	ld	(ix + 1), 0		; loop, one exponent lower
	ld	(ix + 2), 0
	ld	(ix + 3), 0
	ld	b, 32
	ld	c, 126
	jr	.fdiv_exponent

.fdiv_ge:
	ld	(ix + 16), hl		; what the leading 1 left behind
	ld	(ix + 0), 1		; and that 1, already in place
	ld	(ix + 1), 0
	ld	(ix + 2), 0
	ld	(ix + 3), 0
	ld	b, 31
	ld	c, 127

.fdiv_exponent:
	push	bc			; the count and the bias
	ld	hl, 0
	ld	l, (ix + 8)
	ld	de, 0
	ld	e, (ix + 9)
	or	a, a
	sbc	hl, de			; the exponents' difference, which may
	ld	de, 0			; have gone negative
	ld	e, c
	add	hl, de			; plus the bias
	ld	e, (ix + 23)		; plus the divisor's shift
	add	hl, de
	ld	e, (ix + 15)		; less the dividend's
	or	a, a
	sbc	hl, de

	ld	(ix + 20), hl		; kept until there is a quotient to go
	pop	bc			; with it, because whether it is in
					; range decides nothing until then

	ld	hl, (ix + 16)		; the remainder, in a register for the
	ld	de, (ix + 5)		; whole loop, as is the divisor

	; The quotient is built across the whole thirty-two bit field rather
	; than the twenty-four of the significand, so that its leading bit
	; lands at bit 31 and the eight below the number come out of the
	; division too. That is the shape everything else rounds and packs,
	; and it is why the count is 31 or 32 and not 24 or 25: the extra
	; iterations are the guard byte.
.fdiv_loop:
	sla	(ix + 0)		; the quotient makes room for the bit
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	add	hl, hl			; and the remainder doubles
	jr	c, .fdiv_force
	or	a, a
	sbc	hl, de
	jr	nc, .fdiv_fits
	add	hl, de			; it did not fit, so put it back
	jr	.fdiv_next
.fdiv_force:
	or	a, a			; above twenty-four bits, so the divisor
	sbc	hl, de			; fits however the borrow reads
.fdiv_fits:
	inc	(ix + 0)		; the shift left bit 0 clear
.fdiv_next:
	djnz	.fdiv_loop

	; Whatever is left of the remainder goes into the lowest bit, so that a
	; quotient that stopped exactly on the halfway mark can be told from
	; one that merely rounds to it.
	ld	de, 0			; hl == 0 over all three bytes: `ld a, h`
	or	a, a			; would reach two of them and the third
	sbc	hl, de			; has no name, but a subtract of zero
	jr	z, .fdiv_rounded	; sets Z from the whole width
	set	0, (ix + 0)

.fdiv_rounded:
	ld	hl, (ix + 20)		; and now the exponent has to answer for
	push	hl			; itself
	pop	de
	ld	a, d
	or	a, a
	jr	nz, .fdiv_wide_exponent
	ld	a, e
	or	a, a
	jp	z, acc_rt_fadd_subnormal
	cp	a, 255
	jp	nc, acc_rt_fadd_infinity
	ld	(ix + 8), e
	jp	acc_rt_fadd_round

.fdiv_wide_exponent:
	cp	a, 0xff			; the high byte says which end it ran off
	jp	z, acc_rt_fadd_subnormal
	jp	acc_rt_fadd_infinity
