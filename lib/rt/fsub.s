;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fadd
	XDEF	_acc_rt_fsub
	XDEF	acc_rt_fadd_infinity
	XDEF	acc_rt_fadd_nan
	XDEF	acc_rt_fadd_round
	XDEF	acc_rt_fadd_subnormal
	XDEF	acc_rt_fadd_zero
	XREF	acc_rt_funpack

	.assume adl=1
	SEGMENT CODE

; The right operand is written to, which is why helper_writes_right in gen.c
; names this routine: acc passes a right operand where the program keeps it
; unless the routine is one of the few that change it.
_acc_rt_fsub:
	push	hl			; the same as adding the right operand
	push	de			; with its sign turned over, and the
	push	bc			; operand is the caller's scratch slot,
	ex	de, hl			; dead once the operator has been applied
	ld	bc, 3
	add	hl, bc
	ld	a, (hl)
	xor	a, 0x80
	ld	(hl), a
	pop	bc
	pop	de
	pop	hl
	; fall through

_acc_rt_fadd:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl

	ld	ix, -24			; the same size fmul uses, because the
	add	ix, sp			; two share everything from the
	ld	sp, ix			; rounding onwards
	ld	(ix + 12), hl		; the destination, which is also the left
					; operand, kept where clobbering hl
					; cannot lose it

	push	ix
	pop	iy
	call	acc_rt_funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl			; the right operand's address
	call	acc_rt_funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	; An infinity or a NaN on either side is answered here rather than put
	; through the arithmetic, which has no meaning for either. funpack
	; leaves both with an exponent of 255 and tells them apart by the
	; significand: empty is an infinity, anything else a NaN.
	ld	a, (ix + 8)
	cp	a, 255
	jp	z, .fadd_left_special
	ld	a, (ix + 9)
	cp	a, 255
	jp	z, .fadd_right_special

	; The larger exponent has to be the left one, so that aligning only
	; ever shifts the right operand down.
	ld	a, (ix + 8)
	cp	a, (ix + 9)
	jr	nc, .fadd_aligned_order
	call	.fadd_swap
.fadd_aligned_order:
	ld	a, (ix + 8)
	cp	a, (ix + 9)
	jr	nz, .fadd_align
	; Equal exponents: the larger significand has to be on the left too,
	; because a subtract here must not borrow past the top.
	ld	a, (ix + 3)
	cp	a, (ix + 7)
	jr	c, .fadd_need_swap
	jr	nz, .fadd_align
	ld	a, (ix + 2)
	cp	a, (ix + 6)
	jr	c, .fadd_need_swap
	jr	nz, .fadd_align
	ld	a, (ix + 1)
	cp	a, (ix + 5)
	jr	c, .fadd_need_swap
	jr	.fadd_align
.fadd_need_swap:
	call	.fadd_swap

.fadd_align:
	ld	a, (ix + 8)		; how far the right operand is down
	sub	a, (ix + 9)
	jr	z, .fadd_combine
	cp	a, 33			; further than the field is wide: all
	jr	c, .fadd_shift_loop	; that is left of it is a sticky bit
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	ld	(ix + 4), 0
	ld	(ix + 5), 0
	ld	(ix + 6), 0
	ld	(ix + 7), 0
	jr	z, .fadd_combine
	ld	(ix + 4), 1
	jp	.fadd_combine

.fadd_shift_loop:
	ld	b, a
.fadd_shift:
	srl	(ix + 7)
	rr	(ix + 6)
	rr	(ix + 5)
	rr	(ix + 4)
	jr	nc, .fadd_shift_next
	set	0, (ix + 4)		; what falls off the bottom is not lost,
					; it is remembered in the lowest bit
.fadd_shift_next:
	djnz	.fadd_shift

.fadd_combine:
	ld	a, (ix + 10)		; like signs add, unlike signs subtract
	xor	a, (ix + 11)
	jp	m, .fadd_subtract

	ld	a, (ix + 0)
	add	a, (ix + 4)
	ld	(ix + 0), a
	ld	a, (ix + 1)
	adc	a, (ix + 5)
	ld	(ix + 1), a
	ld	a, (ix + 2)
	adc	a, (ix + 6)
	ld	(ix + 2), a
	ld	a, (ix + 3)
	adc	a, (ix + 7)
	ld	(ix + 3), a
	jr	nc, .fadd_normalise
	; carried out of the top: one place right, and one more exponent
	rr	(ix + 3)		; the carry is the bit coming back in
	rr	(ix + 2)
	rr	(ix + 1)
	rr	(ix + 0)
	jr	nc, .fadd_carry_exp
	set	0, (ix + 0)
.fadd_carry_exp:
	inc	(ix + 8)
	jp	z, acc_rt_fadd_infinity
	jp	acc_rt_fadd_round

.fadd_subtract:
	ld	a, (ix + 0)
	sub	a, (ix + 4)
	ld	(ix + 0), a
	ld	a, (ix + 1)
	sbc	a, (ix + 5)
	ld	(ix + 1), a
	ld	a, (ix + 2)
	sbc	a, (ix + 6)
	ld	(ix + 2), a
	ld	a, (ix + 3)
	sbc	a, (ix + 7)
	ld	(ix + 3), a

.fadd_normalise:
	ld	a, (ix + 0)		; an exact cancellation is +0, which is
	or	a, (ix + 1)		; what IEEE asks for in this rounding
	or	a, (ix + 2)		; mode, whatever the operands' signs
	or	a, (ix + 3)
	jr	nz, .fadd_norm_loop
	ld	(ix + 10), 0
	jp	acc_rt_fadd_zero
.fadd_norm_loop:
	bit	7, (ix + 3)
	jr	nz, acc_rt_fadd_round
	ld	a, (ix + 8)		; exponent 1 is as low as a number goes.
	cp	a, 2			; What is left without a leading bit is
	jr	c, acc_rt_fadd_round		; a denormal, and packs as one.
	sla	(ix + 0)
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	dec	(ix + 8)
	jr	.fadd_norm_loop

; The exponent in hl is at or below zero, so the result is too small to have
; a leading bit of its own. Shifting the significand down by as much as the
; exponent is short brings it to the denormal scale, where the exponent is 1
; and the leading bit is wherever it falls. The bits pushed out are kept in
; the lowest one, so that rounding still sees the difference between a value
; on the halfway mark and one just above it.
acc_rt_fadd_subnormal:
	ld	de, 1
	ex	de, hl
	or	a, a
	sbc	hl, de			; hl = 1 - the exponent
	ld	de, 26
	or	a, a
	sbc	hl, de
	jp	nc, acc_rt_fadd_zero		; further down than the field is wide
	add	hl, de
	push	hl
	pop	de
	ld	b, e
	ld	(ix + 8), 1
.fadd_sub_loop:
	srl	(ix + 3)
	rr	(ix + 2)
	rr	(ix + 1)
	rr	(ix + 0)
	jr	nc, .fadd_sub_next
	set	0, (ix + 0)
.fadd_sub_next:
	djnz	.fadd_sub_loop
	; fall through to rounding

acc_rt_fadd_round:
	ld	a, (ix + 0)		; the eight bits below the number
	cp	a, 0x80
	jr	c, .fadd_pack		; below half: truncate
	jr	nz, .fadd_round_up	; above half: up
	bit	0, (ix + 1)		; exactly half: to the even significand
	jr	z, .fadd_pack
.fadd_round_up:
	ld	a, (ix + 1)
	add	a, 1
	ld	(ix + 1), a
	ld	a, (ix + 2)
	adc	a, 0
	ld	(ix + 2), a
	ld	a, (ix + 3)
	adc	a, 0
	ld	(ix + 3), a
	jr	nc, .fadd_pack
	; rounding carried out of the top, so the number is a power of two
	ld	(ix + 3), 0x80
	inc	(ix + 8)
	jp	z, acc_rt_fadd_infinity

.fadd_pack:
	ld	a, (ix + 8)
	cp	a, 255
	jp	nc, acc_rt_fadd_infinity

	; Which exponent gets written is what says normal from denormal, and
	; the significand's leading bit is what decides. A normal one has it
	; set and it is implied rather than stored; a denormal has it clear
	; and a stored exponent of zero says so. Rounding a denormal up can
	; set it, which turns the result into the smallest normal -- and that
	; needs no special case here, because by then the bit is set and the
	; exponent to write is the 1 it already has.
	bit	7, (ix + 3)
	jr	nz, .fadd_pack_write
	ld	(ix + 8), 0

.fadd_pack_write:
	ld	hl, (ix + 12)
	ld	a, (ix + 1)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 2)
	ld	(hl), a
	inc	hl
	ld	a, (ix + 3)
	and	a, 0x7f			; the leading 1 goes back to being implied
	ld	b, a
	ld	a, (ix + 8)
	rrca				; the exponent's low bit sits above the
	and	a, 0x80			; mantissa
	or	a, b
	ld	(hl), a
	inc	hl
	ld	a, (ix + 8)
	srl	a
	or	a, (ix + 10)		; and the sign above the exponent
	ld	(hl), a
	jp	.fadd_done

; An exponent past the top. IEEE says the answer is an infinity of the right
; sign, and a program that goes on to compute with one gets an infinity or a
; NaN out rather than a large number that looks like an ordinary answer.
acc_rt_fadd_infinity:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	a, (ix + 10)
	or	a, 0x7f
	ld	(hl), a
	dec	hl
	ld	(hl), 0x80		; the exponent's low bit, which lives
	jp	.fadd_done		; above the mantissa

; A quiet NaN, which is what an operation with no answer produces: every
; exponent bit set, as an infinity has, and a mantissa that is not empty. The
; top mantissa bit is the one that makes it quiet rather than signalling.
acc_rt_fadd_nan:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), 0xc0
	inc	hl
	ld	(hl), 0x7f
	jp	.fadd_done

; Zero takes the sign it was given. An exact cancellation clears that sign
; first, because IEEE makes x - x a positive zero in this rounding mode
; whatever the operands were.
acc_rt_fadd_zero:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	a, (ix + 10)
	ld	(hl), a

.fadd_done:
	ld	hl, 24
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

.fadd_left_special:
	ld	a, (ix + 0)
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jp	nz, acc_rt_fadd_nan		; a NaN takes everything with it

	ld	a, (ix + 9)		; an infinity, and the right side finite:
	cp	a, 255			; no finite number moves an infinity
	jp	nz, acc_rt_fadd_infinity

	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan		; a NaN on the right

	ld	a, (ix + 10)		; two infinities: the same sign gives
	xor	a, (ix + 11)		; that infinity back, and opposite signs
	jp	nz, acc_rt_fadd_nan		; have no answer at all
	jp	acc_rt_fadd_infinity

.fadd_right_special:
	ld	a, (ix + 4)
	or	a, (ix + 5)
	or	a, (ix + 6)
	or	a, (ix + 7)
	jp	nz, acc_rt_fadd_nan

	ld	a, (ix + 11)		; the sum is the right side's infinity,
	ld	(ix + 10), a		; sign and all
	jp	acc_rt_fadd_infinity

; The two operands exchanged: the significands, which are four bytes four
; apart, and then the exponents and the signs, which are one byte one apart.
; Walking six pairs four apart instead was the same loop written once for
; three groups that are not laid out alike, and it swapped the right
; significand with the left exponent.
.fadd_swap:
	push	bc
	push	iy
	push	ix
	pop	iy
	ld	b, 4
.fadd_swap_sig:
	ld	a, (iy + 0)
	ld	c, a
	ld	a, (iy + 4)
	ld	(iy + 0), a
	ld	a, c
	ld	(iy + 4), a
	inc	iy
	djnz	.fadd_swap_sig

	ld	a, (ix + 8)
	ld	c, a
	ld	a, (ix + 9)
	ld	(ix + 8), a
	ld	a, c
	ld	(ix + 9), a

	ld	a, (ix + 10)
	ld	c, a
	ld	a, (ix + 11)
	ld	(ix + 10), a
	ld	a, c
	ld	(ix + 11), a
	pop	iy
	pop	bc
	ret
