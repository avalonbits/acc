;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lmul
	XDEF	acc_rt_labs_ix
	XDEF	acc_rt_labs_iy
	XDEF	acc_rt_lneg_iy
	XDEF	acc_rt_ludivmod_core

	.assume adl=1
	SEGMENT CODE

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


; Add the 16-bit product of left byte i and right byte j into the result at
; byte k, carrying up as far as byte 3. Four shapes rather than one with a
; count, because what the top byte drops is the point: at k = 3 the high half
; of the product is past the width and is not added at all.
	MACRO	LMUL_AT_0 i, j
	ld	b, (ix + i)
	ld	c, (ix + (4 + j))
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
	ENDMACRO

	MACRO	LMUL_AT_1 i, j
	ld	b, (ix + i)
	ld	c, (ix + (4 + j))
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
	ENDMACRO

	MACRO	LMUL_AT_2 i, j
	ld	b, (ix + i)
	ld	c, (ix + (4 + j))
	mlt	bc
	ld	a, (iy + 2)
	add	a, c
	ld	(iy + 2), a
	ld	a, (iy + 3)
	adc	a, b
	ld	(iy + 3), a
	ENDMACRO

	MACRO	LMUL_AT_3 i, j
	ld	b, (ix + i)
	ld	c, (ix + (4 + j))
	mlt	bc
	ld	a, (iy + 3)
	add	a, c
	ld	(iy + 3), a
	ENDMACRO

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


; The magnitude of the four bytes at iy, and the same at ix. Flags only; the
; pointers and every other register come back unchanged.
acc_rt_labs_iy:
	bit	7, (iy + 3)
	ret	z
acc_rt_lneg_iy:
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

acc_rt_labs_ix:
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
; overwritten by the remainder. Both unsigned. Every register but af comes
; back as it went in.
;
; Shift and subtract, a bit of quotient a turn, with all four numbers in
; registers -- the quotient in b:ix, the remainder in c:hl, the divisor in
; iyl:de and the count in iyh -- where it used to keep them in memory and
; step each a byte at a time: 5,700 cycles a division, which was three
; quarters of test/perf's crc.c, whose Adler-32 takes two remainders a byte.
; The alternate registers are left alone: MOS's use of them is its own.
;
; The quotient takes the dividend's place a bit at a time: each turn shifts
; the dividend's top bit out into the remainder and leaves a zero at the
; bottom, which is the quotient's bit when the divisor does not fit. The
; remainder never shifts a bit out of its top: before the kth turn it is
; less than the dividend's top k-1 bits, so below 2^31 even before the
; last.
acc_rt_ludivmod_core:
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	push	de			; where the remainder goes
	push	iy			; and the quotient

	; The dividend: b:ix, from its first byte that is not zero -- the
	; turns for the bytes above it would shift zeros through a zero
	; remainder and give zeros -- with c the turns that are left. A
	; dividend under 2^24 takes 24 turns, under 2^16 16: Adler-32's are
	; under 2^17. The bytes below the first one read are read from
	; before the dividend and cleared.
	ld	c, 32
	ld	a, (iy + 3)
	or	a, a
	jr	nz, .ldiv_32
	ld	c, 24
	ld	a, (iy + 2)
	or	a, a
	jr	nz, .ldiv_24
	ld	c, 16
	ld	a, (iy + 1)
	or	a, a
	jr	nz, .ldiv_16
	ld	c, 8
	ld	b, (iy + 0)
	ld	ix, 0
	jr	.ldiv_divisor
.ldiv_16:
	ld	b, a
	ld	ix, (iy - 2)
	ld	ixl, 0
	ld	ixh, 0
	jr	.ldiv_divisor
.ldiv_24:
	ld	b, a
	ld	ix, (iy - 1)
	ld	ixl, 0
	jr	.ldiv_divisor
.ldiv_32:
	ld	b, a
	ld	ix, (iy + 0)
.ldiv_divisor:
	ex	de, hl			; the divisor: iyl:de
	ld	de, (hl)
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	ld	iyl, a
	ld	a, c
	ld	iyh, a			; the count

	ld	a, iyl			; a zero divisor is undefined in C:
	or	a, a			; say zero for both rather than loop
	jr	nz, .ldiv_go
	sbc	hl, hl
	sbc	hl, de
	jr	nz, .ldiv_go
	ld	ix, 0
	ld	b, 0
	ld	c, 0
	jr	.ldiv_store

.ldiv_go:
	or	a, a
	sbc	hl, hl			; the remainder: c:hl = 0
	ld	c, l

.ldiv_loop:
	add	ix, ix			; {remainder:quotient} <<= 1
	rl	b
	adc	hl, hl
	rl	c			; which leaves carry clear
	sbc	hl, de
	ld	a, c
	sbc	a, iyl
	jr	c, .ldiv_back
	ld	c, a
	inc	ix			; the quotient's bit
	dec	iyh
	jr	nz, .ldiv_loop
	jr	.ldiv_store
.ldiv_back:
	add	hl, de			; c was never written
	dec	iyh
	jr	nz, .ldiv_loop

.ldiv_store:
	pop	iy			; the quotient where the dividend was
	ld	(iy + 0), ix
	ld	(iy + 3), b
	pop	iy			; the remainder where the divisor was
	ld	(iy + 0), hl
	ld	(iy + 3), c
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret
