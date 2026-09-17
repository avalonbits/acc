;
; The operators the eZ80 has no instruction for.
;
; A 24-bit AND is not an instruction: AND is eight bits wide and the upper
; byte of HL has no name, so two thirds of the register can be reached and the
; rest cannot. Shifts are worse -- there is no barrel shifter, so a shift is a
; loop over the bits whatever the amount. Multiply has MLT, which is 8x8, and
; divide has nothing at all.
;
; agondev's compiler calls into libagon for all of this. acc has nothing to
; link against, so it carries these and emits the ones a program uses into
; that program's image. Each is self-contained: none calls another, so the
; ones a program does not use cost it nothing and the ones it does need no
; relocation beyond the call itself.
;
; The convention is the one agondev uses for the same operations, so that the
; two could be mixed later: left operand in HL, right in BC, result in HL.
; Everything else is preserved -- acc's register allocator may have live
; values in DE, and IY is its scratch.
;
; The upper byte is reached through the stack. Pushing HL puts all three bytes
; somewhere addressable, and (iy+d) can then get at the one that has no name.
;
	.assume adl=1
	.section .text,"ax",@progbits

	.global _acc_rt_and
	.global _acc_rt_or
	.global _acc_rt_xor
	.global _acc_rt_shl
	.global _acc_rt_shru
	.global _acc_rt_shrs
	.global _acc_rt_mul

; ---------------------------------------------------------------- bitwise
; hl = hl OP bc, a byte at a time, with both operands on the stack so that
; every byte of them has an address.

_acc_rt_and:
	push	de
	push	iy
	push	bc			; (iy+3..5) once iy is set
	push	hl			; (iy+0..2)
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	and	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	and	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	and	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

_acc_rt_or:
	push	de
	push	iy
	push	bc
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	or	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	or	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	or	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

_acc_rt_xor:
	push	de
	push	iy
	push	bc
	push	hl
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 0)
	xor	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	xor	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	xor	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

; ---------------------------------------------------------------- shifts
; hl = hl shifted by c places. C leaves a shift of the width or more
; undefined; the count is masked to five bits so the loop terminates, and a
; count of 24 or more then shifts every bit out, which is a defensible answer.

_acc_rt_shl:
	ld	a, c
	and	a, 31
	ret	z
.shl_loop:
	add	hl, hl
	dec	a
	jr	nz, .shl_loop
	ret

; Right shifts go through the stack: there is no way to shift HL right as
; three bytes, but (iy+d) reaches each of them in turn.

_acc_rt_shru:
	ld	a, c
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.shru_loop:
	srl	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .shru_loop
	pop	hl
	pop	iy
	ret

_acc_rt_shrs:
	ld	a, c
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.shrs_loop:
	sra	(iy + 2)		; the sign is carried down, not zero
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .shrs_loop
	pop	hl
	pop	iy
	ret

; ---------------------------------------------------------------- multiply
; hl = hl * bc, twenty-four bits, wrapping.
;
; MLT is the only multiplier and it is 8x8 -> 16, so a 24-bit product is
; built from the partial products of the bytes. Only the low three bytes of
; the result are kept, so the pairs that land entirely above bit 23 are not
; computed at all: with the operands as l,h,u and c,b,y that leaves
;
;   l*c          the low pair, contributing to bytes 0 and 1
;   l*b + h*c    contributing to bytes 1 and 2
;   l*y + h*b + u*c   contributing to byte 2 only
;
; Six multiplies rather than the twenty-four iterations a shift-and-add loop
; would take.

_acc_rt_mul:
	push	de
	push	iy
	push	bc			; (iy+3..5): the right operand
	push	hl			; (iy+0..2): the left
	ld	iy, 0
	add	iy, sp

	; byte 2 of the result: the three pairs that reach it, low byte only
	ld	b, (iy + 0)		; l
	ld	c, (iy + 5)		; y
	mlt	bc
	ld	a, c
	ld	b, (iy + 1)		; h
	ld	c, (iy + 4)		; b
	mlt	bc
	add	a, c
	ld	b, (iy + 2)		; u
	ld	c, (iy + 3)		; c
	mlt	bc
	add	a, c
	ld	e, a			; e = byte 2 so far

	; bytes 1 and 2: l*b + h*c, both sixteen-bit
	ld	b, (iy + 0)
	ld	c, (iy + 4)
	mlt	bc
	ld	d, b			; keep the high half
	ld	a, c
	ld	b, (iy + 1)
	ld	c, (iy + 3)
	mlt	bc
	add	a, c			; a = byte 1 so far
	ld	c, a
	ld	a, d
	adc	a, b			; carries into byte 2
	add	a, e
	ld	e, a			; e = byte 2
	ld	d, c			; d = byte 1

	; bytes 0 and 1: l*c
	ld	b, (iy + 0)
	ld	c, (iy + 3)
	mlt	bc
	ld	a, b
	add	a, d			; byte 1
	ld	d, a
	jr	nc, .mul_no_carry
	inc	e
.mul_no_carry:
	; assemble: c = byte 0, d = byte 1, e = byte 2
	ld	(iy + 0), c
	ld	(iy + 1), d
	ld	(iy + 2), e
	pop	hl
	pop	bc
	pop	iy
	pop	de
	ret

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

	.global _acc_rt_divu
	.global _acc_rt_remu
	.global _acc_rt_divs
	.global _acc_rt_rems

; hl = hl / bc, de = hl % bc, both unsigned. The common core.
_acc_rt_udivmod:
	push	iy
	ld	a, b
	or	a, c
	jr	nz, .div_go
	ld	hl, 0			; divide by zero: undefined, so say zero
	ld	de, 0
	pop	iy
	ret
.div_go:
	push	bc			; the divisor, so (iy+0..2) reaches it
	ld	iy, 0
	add	iy, sp
	ld	de, 0			; the remainder
	ld	a, 24
.div_loop:
	add	hl, hl			; the quotient shifts in at the bottom
	ex	de, hl
	adc	hl, hl			; remainder = remainder * 2 + the bit out
	; remainder - divisor, keeping it only if it does not borrow
	push	hl
	ld	bc, (iy + 0)
	or	a, a
	sbc	hl, bc
	jr	c, .div_too_small
	pop	bc			; discard the old remainder
	ex	de, hl
	inc	l			; the bit fits, so record it
	jr	.div_next
.div_too_small:
	pop	hl
	ex	de, hl
.div_next:
	dec	a
	jr	nz, .div_loop
	pop	bc
	pop	iy
	ret

_acc_rt_divu:
	push	de
	call	_acc_rt_udivmod
	pop	de
	ret

_acc_rt_remu:
	push	de			; the caller may have a live value there
	call	_acc_rt_udivmod
	ex	de, hl			; the remainder is the answer
	pop	de
	ret

; The signed forms reduce to the unsigned one: divide the magnitudes, then
; fix the sign. C99 requires the quotient to truncate towards zero and the
; remainder to take the sign of the dividend, which is what that gives.
;
; The sign of a 24-bit value is bit 23, which lives in the byte of HL that has
; no name -- but `add hl, hl` shifts it into the carry, and a push and a pop
; either side of that leaves the value untouched.

_acc_rt_divs:
	push	de
	push	iy
	ld	e, 0			; the sign of the result, in bit 0

	push	hl
	add	hl, hl
	pop	hl			; carry = the dividend is negative
	jr	nc, .divs_num_ok
	ld	e, 1
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.divs_num_ok:
	push	hl
	push	bc
	pop	hl
	add	hl, hl
	pop	hl			; carry = the divisor is negative
	jr	nc, .divs_den_ok
	ld	a, e
	xor	a, 1
	ld	e, a
	push	hl
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	push	hl
	pop	bc
	pop	hl
.divs_den_ok:
	push	de			; udivmod returns the remainder in de
	call	_acc_rt_udivmod
	pop	de
	bit	0, e
	jr	z, .divs_done
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.divs_done:
	pop	iy
	pop	de
	ret

_acc_rt_rems:
	push	de
	push	iy
	ld	e, 0			; the sign of the dividend, in bit 0

	push	hl
	add	hl, hl
	pop	hl
	jr	nc, .rems_num_ok
	ld	e, 1
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.rems_num_ok:
	push	hl
	push	bc
	pop	hl
	add	hl, hl
	pop	hl
	jr	nc, .rems_den_ok
	push	hl			; the divisor's sign does not reach the
	ld	hl, 0			; remainder, but its magnitude is needed
	or	a, a
	sbc	hl, bc
	push	hl
	pop	bc
	pop	hl
.rems_den_ok:
	push	de
	call	_acc_rt_udivmod
	ex	de, hl			; the remainder is the answer
	pop	de
	bit	0, e
	jr	z, .rems_done
	push	bc
	push	hl
	pop	bc
	ld	hl, 0
	or	a, a
	sbc	hl, bc
	pop	bc
.rems_done:
	pop	iy
	pop	de
	ret

; ---------------------------------------------------------------- long
; A long is four bytes, wider than any register, so it lives in the frame and
; these work on it there: HL points at the destination, DE at the other
; operand, and the destination is overwritten.
;
; Little endian, so the loop runs from the low byte up and the carry chains
; the way the arithmetic needs.

	.global _acc_rt_ladd
	.global _acc_rt_lsub
	.global _acc_rt_land
	.global _acc_rt_lor
	.global _acc_rt_lxor
	.global _acc_rt_lcmpeq
	.global _acc_rt_lcmpord

_acc_rt_ladd:
	push	bc
	push	de
	push	hl
	or	a, a			; no carry into the low byte
	ld	b, 4
.ladd_loop:
	ld	a, (de)
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.ladd_loop
	pop	hl
	pop	de
	pop	bc
	ret

; Only (hl) has an `sbc a, (rr)` form, so the right-hand byte goes through C
; on the way. Neither `ld c, a` nor `ld a, (hl)` touches the carry, so the
; borrow still chains from one byte to the next.
_acc_rt_lsub:
	push	bc
	push	de
	push	hl
	or	a, a
	ld	b, 4
.lsub_loop:
	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lsub_loop
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_land:
	push	bc
	push	de
	push	hl
	ld	b, 4
.land_loop:
	ld	a, (de)
	and	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.land_loop
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_lor:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lor_loop:
	ld	a, (de)
	or	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lor_loop
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_lxor:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lxor_loop:
	ld	a, (de)
	xor	a, (hl)
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lxor_loop
	pop	hl
	pop	de
	pop	bc
	ret

; Z set if the four bytes at (hl) equal those at (de). Neither is changed.
; Compared a byte at a time and stopped at the first difference, rather than
; subtracted: a subtract's Z would describe only the byte it was done on.
_acc_rt_lcmpeq:
	push	bc
	push	de
	push	hl
	ld	b, 4
.lcmpeq_loop:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, .lcmpeq_differ
	inc	hl
	inc	de
	djnz	.lcmpeq_loop
	xor	a, a			; every byte matched: Z
	jr	.lcmpeq_done
.lcmpeq_differ:
	ld	a, 1
	or	a, a			; NZ, whatever the bytes were
.lcmpeq_done:
	pop	hl			; pop leaves the flags alone
	pop	de
	pop	bc
	ret

; The flags of (hl) - (de) as a four-byte subtract, for ordering. The last
; sbc leaves S, P/V and C describing the whole width, which is what the
; caller's branch sequence reads. Neither operand is changed.
_acc_rt_lcmpord:
	push	bc
	push	de
	push	hl
	or	a, a
	ld	b, 4
.lcmpord_loop:
	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	inc	hl
	inc	de
	djnz	.lcmpord_loop
	pop	hl			; pop leaves the flags alone
	pop	de
	pop	bc
	ret

; --------------------------------------------------- long shifts
; (hl) = (hl) shifted by the low byte of (de).
;
; The count arrives as a long because vbinop_long converts both operands, but
; only its low byte can matter: as with the 24-bit shifts the count is masked
; to five bits, so the loop terminates and a count of 32 or more shifts every
; bit out.
;
; iy points at the destination, because (iy+d) reaches all four bytes and
; there is no (hl+d).

	.global _acc_rt_lshl
	.global _acc_rt_lshru
	.global _acc_rt_lshrs

_acc_rt_lshl:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshl_loop:
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	dec	c
	jr	nz, .lshl_loop
	pop	iy
	pop	bc
	ret

_acc_rt_lshru:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshru_loop:
	srl	(iy + 3)
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	c
	jr	nz, .lshru_loop
	pop	iy
	pop	bc
	ret

_acc_rt_lshrs:
	ld	a, (de)
	and	a, 31
	ret	z
	push	bc
	push	iy
	push	hl
	pop	iy
	ld	c, a
.lshrs_loop:
	sra	(iy + 3)		; the sign is carried down, not zero
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	c
	jr	nz, .lshrs_loop
	pop	iy
	pop	bc
	ret

; --------------------------------------------------- long multiply
; (hl) = (hl) * (de), four bytes, wrapping.
;
; The same shape as the 24-bit multiply: MLT is 8x8 -> 16, so the product is
; assembled from the partial products of the bytes, and a pair whose place
; value is already past bit 31 is not computed at all. With the operands as
; a0..a3 and b0..b3 that leaves ten of the sixteen pairs.
;
; Both operands are copied to a stack buffer first because the destination is
; the left operand: accumulating into it in place would overwrite bytes that
; later partial products still have to read. ix reaches the copies -- the left
; at ix+0..3 and the right at ix+4..7 -- and iy the destination.

	.global _acc_rt_lmul

; Add the 16-bit product of left byte \i and right byte \j into the result at
; byte \k, carrying up as far as byte 3. Four shapes rather than one with a
; count, because what the top byte drops is the point: at \k = 3 the high half
; of the product is past the width and is not added at all.
	.macro	LMUL_AT_0 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 0)
	add	a, c
	ld	(iy + 0), a
	ld	a, (iy + 1)
	adc	a, b
	ld	(iy + 1), a
	ld	a, (iy + 2)
	adc	a, 0
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, 0
	ld	(iy + 3), a
	.endm

	.macro	LMUL_AT_1 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 1)
	add	a, c
	ld	(iy + 1), a
	ld	a, (iy + 2)
	adc	a, b
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, 0
	ld	(iy + 3), a
	.endm

	.macro	LMUL_AT_2 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 2)
	add	a, c
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, b
	ld	(iy + 3), a
	.endm

	.macro	LMUL_AT_3 i, j
	ld	b, (ix + \i)
	ld	c, (ix + 4 + \j)
	mlt	bc
	ld	a, (iy + 3)
	add	a, c
	ld	(iy + 3), a
	.endm

_acc_rt_lmul:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl
	push	hl
	pop	iy			; iy -> the destination, which is also
					; the left operand
	ld	hl, -8			; eight bytes of room for the copies
	add	hl, sp
	ld	sp, hl
	push	hl
	pop	ix

	ld	a, (iy + 0)
	ld	(ix + 0), a
	ld	a, (iy + 1)
	ld	(ix + 1), a
	ld	a, (iy + 2)
	ld	(ix + 2), a
	ld	a, (iy + 3)
	ld	(ix + 3), a
	ld	a, (de)
	ld	(ix + 4), a
	inc	de
	ld	a, (de)
	ld	(ix + 5), a
	inc	de
	ld	a, (de)
	ld	(ix + 6), a
	inc	de
	ld	a, (de)
	ld	(ix + 7), a

	ld	(iy + 0), 0
	ld	(iy + 1), 0
	ld	(iy + 2), 0
	ld	(iy + 3), 0

	LMUL_AT_0 0, 0
	LMUL_AT_1 0, 1
	LMUL_AT_1 1, 0
	LMUL_AT_2 0, 2
	LMUL_AT_2 1, 1
	LMUL_AT_2 2, 0
	LMUL_AT_3 0, 3
	LMUL_AT_3 1, 2
	LMUL_AT_3 2, 1
	LMUL_AT_3 3, 0

	ld	hl, 8
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

; --------------------------------------------------- long divide
; (hl) = (hl) / (de) and (de) = (hl) % (de), four bytes.
;
; The same restoring long division as the 24-bit routine, thirty-two
; iterations of it: shift a bit out of the dividend into the remainder and
; subtract the divisor whenever it fits, with the quotient growing into the
; space the dividend vacates. Both answers come back, because a caller that
; wanted the remainder would otherwise have to divide twice.
;
; The remainder needs thirty-three bits, not thirty-two. It stays below the
; divisor, so after doubling it can reach 2*(2^32-1)+1, and a divisor above
; 2^31 makes that overflow. The bit that falls out of the top is kept in b,
; and when it is set the subtract fits whatever the borrow says.
;
; The divisor is overwritten by the remainder, which is what the caller wants
; and is safe besides: vbinop_long gives each operand its own scratch slot and
; the right one is dead once the operator has been applied.
;
; Dividing by zero is undefined in C. These leave zero rather than looping.

	.global _acc_rt_ldivu
	.global _acc_rt_lremu
	.global _acc_rt_ldivs
	.global _acc_rt_lrems

; The magnitude of the four bytes at iy, and the same at ix. Flags only; the
; pointers and every other register come back unchanged.
.labs_iy:
	bit	7, (iy + 3)
	ret	z
.lneg_iy:
	ld	a, 0
	sub	a, (iy + 0)
	ld	(iy + 0), a
	ld	a, 0			; ld leaves the borrow alone
	sbc	a, (iy + 1)
	ld	(iy + 1), a
	ld	a, 0
	sbc	a, (iy + 2)
	ld	(iy + 2), a
	ld	a, 0
	sbc	a, (iy + 3)
	ld	(iy + 3), a
	ret

.labs_ix:
	bit	7, (ix + 3)
	ret	z
	ld	a, 0
	sub	a, (ix + 0)
	ld	(ix + 0), a
	ld	a, 0
	sbc	a, (ix + 1)
	ld	(ix + 1), a
	ld	a, 0
	sbc	a, (ix + 2)
	ld	(ix + 2), a
	ld	a, 0
	sbc	a, (ix + 3)
	ld	(ix + 3), a
	ret

; iy -> the dividend, which becomes the quotient; de -> the divisor, which is
; overwritten by the remainder. Both unsigned.
;
; The divisor is copied to a stack buffer and the remainder built beside it,
; at ix+0..3 and ix+4..7. Neither can live in the caller's frame: the divisor
; slot is only four bytes wide and what follows it belongs to some other
; value, so the remainder has nowhere there to go.
.ludivmod_core:
	push	bc
	push	de
	push	hl
	push	ix

	ld	hl, -8
	add	hl, sp
	ld	sp, hl
	push	hl
	pop	ix

	ld	a, (de)			; the divisor, copied
	ld	(ix + 0), a
	inc	de
	ld	a, (de)
	ld	(ix + 1), a
	inc	de
	ld	a, (de)
	ld	(ix + 2), a
	inc	de
	ld	a, (de)
	ld	(ix + 3), a
	dec	de
	dec	de
	dec	de

	ld	a, (ix + 0)		; a zero divisor is undefined in C
	or	a, (ix + 1)
	or	a, (ix + 2)
	or	a, (ix + 3)
	jr	nz, .ldiv_go

	ld	(iy + 0), 0		; say zero rather than loop
	ld	(iy + 1), 0
	ld	(iy + 2), 0
	ld	(iy + 3), 0
	jp	.ldiv_store

.ldiv_go:
	ld	(ix + 4), 0		; the remainder
	ld	(ix + 5), 0
	ld	(ix + 6), 0
	ld	(ix + 7), 0
	ld	c, 32

.ldiv_loop:
	; {remainder:quotient} <<= 1, the bit out of the dividend going in at
	; the bottom of the remainder
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	rl	(ix + 4)
	rl	(ix + 5)
	rl	(ix + 6)
	rl	(ix + 7)
	ld	a, 0
	adc	a, 0			; b = the thirty-third bit
	ld	b, a

	; remainder - divisor, kept only if it does not borrow
	ld	a, (ix + 4)
	sub	a, (ix + 0)
	ld	(ix + 4), a
	ld	a, (ix + 5)		; ld leaves the borrow alone
	sbc	a, (ix + 1)
	ld	(ix + 5), a
	ld	a, (ix + 6)
	sbc	a, (ix + 2)
	ld	(ix + 6), a
	ld	a, (ix + 7)
	sbc	a, (ix + 3)
	ld	(ix + 7), a
	jr	nc, .ldiv_fits
	bit	0, b			; it borrowed, but the bit above the
	jr	nz, .ldiv_fits		; width says it fitted after all

	ld	a, (ix + 4)		; put the remainder back
	add	a, (ix + 0)
	ld	(ix + 4), a
	ld	a, (ix + 5)
	adc	a, (ix + 1)
	ld	(ix + 5), a
	ld	a, (ix + 6)
	adc	a, (ix + 2)
	ld	(ix + 6), a
	ld	a, (ix + 7)
	adc	a, (ix + 3)
	ld	(ix + 7), a
	jr	.ldiv_next

.ldiv_fits:
	set	0, (iy + 0)		; the bit the shift left empty
.ldiv_next:
	dec	c
	jr	nz, .ldiv_loop

.ldiv_store:
	ld	a, (ix + 4)		; the remainder belongs where the
	ld	(de), a			; divisor was
	inc	de
	ld	a, (ix + 5)
	ld	(de), a
	inc	de
	ld	a, (ix + 6)
	ld	(de), a
	inc	de
	ld	a, (ix + 7)
	ld	(de), a

	ld	hl, 8
	add	hl, sp
	ld	sp, hl
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret

_acc_rt_ldivu:
	push	iy
	push	hl
	pop	iy
	call	.ludivmod_core
	pop	iy
	ret

_acc_rt_lremu:
	push	ix
	push	iy
	push	hl
	pop	iy
	push	de
	pop	ix
	call	.ludivmod_core
	ld	a, (ix + 0)		; the remainder is the answer, and the
	ld	(iy + 0), a		; core left it where the divisor was
	ld	a, (ix + 1)
	ld	(iy + 1), a
	ld	a, (ix + 2)
	ld	(iy + 2), a
	ld	a, (ix + 3)
	ld	(iy + 3), a
	pop	iy
	pop	ix
	ret

; C99 has division truncate towards zero and the remainder take the sign of
; the dividend, which is what dividing the magnitudes and fixing the sign
; afterwards gives.
_acc_rt_ldivs:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)		; the quotient is negative when the
	xor	a, (ix + 3)		; operands differ in sign
	and	a, 0x80
	ld	b, a
	call	.labs_iy
	call	.labs_ix
	call	.ludivmod_core
	bit	7, b
	call	nz, .lneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret

_acc_rt_lrems:
	push	ix
	push	iy
	push	bc
	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)		; the remainder takes the dividend's
	and	a, 0x80			; sign
	ld	b, a
	call	.labs_iy
	call	.labs_ix
	call	.ludivmod_core
	ld	a, (ix + 0)
	ld	(iy + 0), a
	ld	a, (ix + 1)
	ld	(iy + 1), a
	ld	a, (ix + 2)
	ld	(iy + 2), a
	ld	a, (ix + 3)
	ld	(iy + 3), a
	bit	7, b
	call	nz, .lneg_iy
	pop	bc
	pop	iy
	pop	ix
	ret

; --------------------------------------------------- long unary
; (hl) = -(hl) and (hl) = ~(hl), four bytes.
;
; The 24-bit forms are `0 - x` in HL and the same less one, which the chip can
; do in a register. A long cannot be held in one, so these work in the frame
; like the rest of the long routines.

	.global _acc_rt_lneg
	.global _acc_rt_lnot

_acc_rt_lneg:
	push	iy
	push	hl
	pop	iy
	call	.lneg_iy
	pop	iy
	ret

_acc_rt_lnot:
	push	iy
	push	hl
	pop	iy
	ld	a, (iy + 0)
	cpl
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
	pop	iy
	ret

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
; What is here covers the numbers a program computes with. Denormals are
; flushed to zero rather than represented, and nothing generates an infinity
; or a NaN -- an overflow saturates and an underflow goes to zero. That is
; short of C99 and is written down as such; it is not a silent divergence.

	.global _acc_rt_itof
	.global _acc_rt_uitof
	.global _acc_rt_ftoi

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

	.global _acc_rt_fkey

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

	.global _acc_rt_fadd
	.global _acc_rt_fsub

; Unpack the float at (hl) into the four bytes at (iy), the exponent into b and
; the sign into c. An exponent of zero is a zero or a denormal, and both come
; out as zero: there are no denormals here.
.funpack:
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

	ld	a, b			; exponent zero: no leading 1 after all
	or	a, a
	ret	nz
	ld	(iy + 0), 0
	ld	(iy + 1), 0
	ld	(iy + 2), 0
	ld	(iy + 3), 0
	ret

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
	call	.funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl			; the right operand's address
	call	.funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

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
	jp	z, .fadd_overflow
	jp	.fadd_round

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
	or	a, (ix + 2)		; mode
	or	a, (ix + 3)
	jp	z, .fadd_zero
.fadd_norm_loop:
	bit	7, (ix + 3)
	jr	nz, .fadd_round
	sla	(ix + 0)
	rl	(ix + 1)
	rl	(ix + 2)
	rl	(ix + 3)
	dec	(ix + 8)
	jr	nz, .fadd_norm_loop
	jp	.fadd_zero		; shifted down past the smallest exponent

.fadd_round:
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
	jp	z, .fadd_overflow

.fadd_pack:
	ld	a, (ix + 8)
	or	a, a
	jp	z, .fadd_zero		; no exponent left to write it with
	cp	a, 255
	jp	nc, .fadd_overflow

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

.fadd_overflow:
	; Nothing here makes an infinity, so the largest finite float is the
	; answer. It is wrong, and it is wrong by less than an infinity would
	; be for a program that goes on to compute with it.
	ld	hl, (ix + 12)
	ld	(hl), 0xff
	inc	hl
	ld	(hl), 0xff
	inc	hl
	ld	(hl), 0x7f
	inc	hl
	ld	a, (ix + 10)
	or	a, 0x7f
	ld	(hl), a
	jp	.fadd_done

.fadd_zero:
	ld	hl, (ix + 12)
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
	ld	(hl), a
	inc	hl
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

; --------------------------------------------------- float multiply
; (hl) = (hl) * (de).
;
; The significands are twenty-four bits each and their product is forty-eight,
; built from the nine partial products of their bytes -- MLT again, and this
; time none of the nine can be dropped, because the top of the product is
; exactly the part that is kept.
;
; Two twenty-four bit numbers with their leading bit set multiply to something
; in [2^46, 2^48), so the answer needs at most one shift to put its leading
; bit at 47. Which of the two cases it is decides the exponent: a product that
; already reaches bit 47 is between 2 and 4 and takes one more exponent than
; the sum of the operands'.
;
; The frame is laid out like fadd's on purpose -- significand at ix+1..3,
; exponent at ix+8, sign at ix+10, destination at ix+12 -- so that the
; rounding and packing at the end of fadd serve this too. What is above ix+15
; is the part fadd has no use for: the forty-eight bit product, and the
; exponent before it is known to fit in a byte.

	.global _acc_rt_fmul

; The 16-bit product of significand byte \i of the left and \j of the right,
; added into the running product at byte \k and carried up to the top.
	.macro	FMUL_AT i, j, k
	ld	b, (ix + 1 + \i)
	ld	c, (ix + 5 + \j)
	mlt	bc
	ld	a, (ix + 16 + \k)
	add	a, c
	ld	(ix + 16 + \k), a
	ld	a, (ix + 17 + \k)
	adc	a, b
	ld	(ix + 17 + \k), a
	.if \k < 4
	ld	a, (ix + 18 + \k)
	adc	a, 0
	ld	(ix + 18 + \k), a
	.endif
	.if \k < 3
	ld	a, (ix + 19 + \k)
	adc	a, 0
	ld	(ix + 19 + \k), a
	.endif
	.if \k < 2
	ld	a, (ix + 20 + \k)
	adc	a, 0
	ld	(ix + 20 + \k), a
	.endif
	.if \k < 1
	ld	a, (ix + 21 + \k)
	adc	a, 0
	ld	(ix + 21 + \k), a
	.endif
	.endm

_acc_rt_fmul:
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
	call	.funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl
	call	.funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	ld	a, (ix + 10)		; the sign is the two signs differing
	xor	a, (ix + 11)
	and	a, 0x80
	ld	(ix + 10), a

	ld	a, (ix + 8)		; either operand zero makes the product
	or	a, a			; zero, and there is no exponent for it
	jp	z, .fadd_zero
	ld	a, (ix + 9)
	or	a, a
	jp	z, .fadd_zero

	ld	(ix + 16), 0		; the product, in six bytes
	ld	(ix + 17), 0
	ld	(ix + 18), 0
	ld	(ix + 19), 0
	ld	(ix + 20), 0
	ld	(ix + 21), 0

	FMUL_AT 0, 0, 0
	FMUL_AT 0, 1, 1
	FMUL_AT 1, 0, 1
	FMUL_AT 0, 2, 2
	FMUL_AT 1, 1, 2
	FMUL_AT 2, 0, 2
	FMUL_AT 1, 2, 3
	FMUL_AT 2, 1, 3
	FMUL_AT 2, 2, 4

	; The exponent, which does not fit in a byte until the range has been
	; checked: two biased exponents add to as much as 508.
	ld	hl, 0
	ld	l, (ix + 8)
	ld	de, 0
	ld	e, (ix + 9)
	add	hl, de
	ld	de, 127			; one bias too many, having added two
	or	a, a
	sbc	hl, de

	bit	7, (ix + 21)		; already at bit 47: between 2 and 4,
	jr	z, .fmul_shift		; so one more exponent
	inc	hl
	jr	.fmul_exponent

.fmul_shift:
	sla	(ix + 16)		; below bit 47: one place up, and the
	rl	(ix + 17)		; exponent is the sum as it stands
	rl	(ix + 18)
	rl	(ix + 19)
	rl	(ix + 20)
	rl	(ix + 21)

.fmul_exponent:
	push	hl			; nothing left of the number
	pop	de
	ld	a, d
	or	a, a
	jp	nz, .fmul_range		; the high byte says out of a byte's range
	ld	a, e
	or	a, a
	jp	z, .fadd_zero
	cp	a, 255
	jp	nc, .fadd_overflow
	ld	(ix + 8), e

	; The significand is the top twenty-four bits; the twenty-four below
	; are what rounding reads. Half is exactly 0x800000 of them.
	ld	a, (ix + 19)
	ld	(ix + 1), a
	ld	a, (ix + 20)
	ld	(ix + 2), a
	ld	a, (ix + 21)
	ld	(ix + 3), a

	ld	a, (ix + 18)
	cp	a, 0x80
	jp	c, .fadd_pack		; below half
	jr	nz, .fmul_up		; above half
	ld	a, (ix + 16)		; exactly half only if nothing is left
	or	a, (ix + 17)		; below the halfway bit
	jr	nz, .fmul_up
	bit	0, (ix + 1)		; a true tie: to the even significand
	jp	z, .fadd_pack

.fmul_up:
	ld	a, (ix + 1)
	add	a, 1
	ld	(ix + 1), a
	ld	a, (ix + 2)
	adc	a, 0
	ld	(ix + 2), a
	ld	a, (ix + 3)
	adc	a, 0
	ld	(ix + 3), a
	jp	nc, .fadd_pack
	ld	(ix + 3), 0x80		; carried out: the next power of two
	inc	(ix + 8)
	jp	z, .fadd_overflow
	jp	.fadd_pack

.fmul_range:
	; The high byte is nonzero, so the exponent is either far too large or
	; has gone negative. Negative is a borrow out of the subtract above,
	; which leaves 0xff there.
	ld	a, d
	cp	a, 0xff
	jp	z, .fadd_zero
	jp	.fadd_overflow

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

	.global _acc_rt_fdiv

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
	call	.funpack
	ld	(ix + 8), b
	ld	(ix + 10), c

	push	ix
	pop	iy
	ld	bc, 4
	add	iy, bc
	ex	de, hl
	call	.funpack
	ex	de, hl
	ld	(ix + 9), b
	ld	(ix + 11), c

	ld	a, (ix + 10)		; the sign is the two signs differing
	xor	a, (ix + 11)
	and	a, 0x80
	ld	(ix + 10), a

	ld	a, (ix + 8)		; zero divided by anything is zero
	or	a, a
	jp	z, .fadd_zero
	ld	a, (ix + 9)		; and dividing by zero is undefined, so
	or	a, a			; the largest finite float will do
	jp	z, .fadd_overflow

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
	ld	(ix + 1), 0		; and the whole quotient comes from the
	ld	(ix + 2), 0		; loop, one exponent lower
	ld	(ix + 3), 0
	ld	b, 25			; twenty-four bits and the guard
	ld	c, 126
	jr	.fdiv_exponent

.fdiv_ge:
	ld	(ix + 16), hl		; what the leading 1 left behind
	ld	(ix + 1), 1		; and that 1, already in place
	ld	(ix + 2), 0
	ld	(ix + 3), 0
	ld	b, 24			; twenty-three more and the guard
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

	push	hl			; the high byte says whether a byte
	pop	de			; can hold it
	ld	a, d
	or	a, a
	jr	z, .fdiv_exp_byte
	pop	bc
	cp	a, 0xff			; negative, so far below the smallest
	jp	z, .fadd_zero
	jp	.fadd_overflow

.fdiv_exp_byte:
	ld	a, e
	or	a, a
	jr	nz, .fdiv_exp_high
	pop	bc
	jp	.fadd_zero
.fdiv_exp_high:
	cp	a, 255
	jr	c, .fdiv_exp_ok
	pop	bc
	jp	.fadd_overflow
.fdiv_exp_ok:
	ld	(ix + 8), e
	pop	bc			; the count back into b

	ld	hl, (ix + 16)		; the remainder, in a register for the
	ld	de, (ix + 5)		; whole loop, as is the divisor

.fdiv_loop:
	sla	(ix + 1)		; the quotient makes room for the bit
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
	inc	(ix + 1)		; the shift left bit 0 clear
.fdiv_next:
	djnz	.fdiv_loop

	; The last bit produced is the guard; it sits at the bottom of the
	; quotient and has to come out before the significand is packed.
	ld	a, (ix + 1)
	push	af
	srl	(ix + 3)
	rr	(ix + 2)
	rr	(ix + 1)
	pop	af
	bit	0, a
	jp	z, .fadd_pack		; below half

	; The guard is set: a remainder of nothing is a tie, anything else is
	; above half.
	ld	de, 0			; hl == 0 over all three bytes: `ld a, h`
	or	a, a			; would reach two of them and the third
	sbc	hl, de			; has no name, but a subtract of zero
	jr	nz, .fdiv_up		; sets Z from the whole width
	bit	0, (ix + 1)		; a true tie: to the even significand
	jp	z, .fadd_pack

.fdiv_up:
	ld	a, (ix + 1)
	add	a, 1
	ld	(ix + 1), a
	ld	a, (ix + 2)
	adc	a, 0
	ld	(ix + 2), a
	ld	a, (ix + 3)
	adc	a, 0
	ld	(ix + 3), a
	jp	nc, .fadd_pack
	ld	(ix + 3), 0x80		; carried out: the next power of two
	inc	(ix + 8)
	jp	z, .fadd_overflow
	jp	.fadd_pack
