;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llmul
	XDEF	acc_rt_llabs_ix
	XDEF	acc_rt_llabs_iy
	XDEF	acc_rt_llneg_iy
	XDEF	acc_rt_llrem_move
	XDEF	acc_rt_lludivmod_core
	XREF	acc_rt_llneg_hl

	.assume adl=1
	SEGMENT CODE

; (hl) = (hl) * (de), eight bytes, wrapping.
;
; Schoolbook, a byte at a time, as the long multiply is: mlt gives the 16-bit
; product of two bytes, and byte i of the left operand times byte j of the
; right one lands at byte i + j. Only the 36 products with i + j below eight
; land inside the answer, so row i -- byte i times the whole right operand,
; added in from byte i up -- is 8 - i steps long, and whatever it carries out
; of the top byte is dropped. A row whose byte is zero adds nothing and is
; skipped, which is most of them for a value that would have fitted a long.
;
; Each step adds a product, the byte of the answer already there and the
; carry from the step below, and the sum fits sixteen bits: 255 * 255 + 255 +
; 255 is 65535. Its low byte is the answer's and its high byte the carry.
;
; The left operand is copied out first, to ix+0..7, because the answer is
; built where it was.
_acc_rt_llmul:
	push	ix
	push	iy
	push	bc
	push	de
	push	hl
	ld	ix, -8
	add	ix, sp
	ld	sp, ix

	push	hl
	pop	iy			; iy -> the answer
	push	de
	lea	de, ix + 0		; the left operand, copied
	ld	bc, 8
	ldir
	pop	de
	lea	hl, iy + 0		; and the answer, zero
	xor	a, a
	ld	b, 8
.llmul_clear:
	ld	(hl), a
	inc	hl
	djnz	.llmul_clear

	ld	c, 8			; row 0 is eight steps long
.llmul_row:
	ld	a, (ix + 0)		; this row's byte of the left operand
	or	a, a
	jr	z, .llmul_next
	push	bc
	push	de
	push	iy
	ld	h, c			; steps in this row
	ld	l, 0			; the carry into the first
.llmul_step:
	ld	a, (de)
	ld	c, a
	ld	b, (ix + 0)
	mlt	bc			; left byte * right byte
	ld	a, (iy + 0)		; + the answer's byte
	add	a, c
	ld	c, a
	ld	a, b
	adc	a, 0
	ld	b, a
	ld	a, c			; + the carry
	add	a, l
	ld	(iy + 0), a
	ld	a, b
	adc	a, 0
	ld	l, a			; the carry into the next
	inc	iy
	inc	de
	dec	h
	jr	nz, .llmul_step
	pop	iy
	pop	de
	pop	bc
.llmul_next:
	inc	ix			; the next byte of the left operand
	inc	iy			; lands a byte higher
	dec	c			; and has a step less to go
	jr	nz, .llmul_row

	ld	hl, 8			; the copy, dropped
	add	hl, sp
	ld	sp, hl
	pop	hl
	pop	de
	pop	bc
	pop	iy
	pop	ix
	ret

; iy -> the dividend, which becomes the quotient; de -> the divisor, which is
; overwritten by the remainder. Both unsigned.
;
; Shift and subtract, as the long core is, but with the remainder in
; registers rather than in memory: HL holds its low 24 bits, DE the next 24
; and BC the top 16, and a shift of all three is three add-with-carries. The
; quotient's low 24 bits are in IX, its next 24 on top of the stack and its
; top 16 in the frame. The divisor stays in the frame, where a trial compare
; reads it a limb at a time from the top: most of the time the top limbs
; differ and that settles it, and the subtract is done only when it fits.
;
; The remainder never needs a 65th bit, though it stays below a divisor that
; can be above 2^63: it is what is left of the part of the dividend shifted
; in so far, so it is never larger than that part, which after k rounds is
; k bits. Nothing falls out of the top of BC.
;
; Rounds that cannot subtract are not run. Leading zero bytes of the dividend
; would only shift zeros through a remainder that stays zero; and while the
; remainder is shorter than the divisor it cannot be as large, so a divisor
; of m bytes lets the dividend's top m - 1 bytes go straight into the
; remainder. What is left is eight rounds for each byte of the dividend past
; those: the rounds skipped are exactly shifts, done as a byte move.
;
; A divisor of one byte is the common case -- dividing by ten, or by a power
; of two -- and has a loop of its own. Its remainder fits in A, below 256,
; and needs a ninth bit, which the carry out of the rotate is. That leaves HL
; and DE for the quotient, which with IX holds all of it as 72 bits: the
; dividend sits a byte up in them, so what comes out of the top of DE is its
; top bit, and a round touches no memory at all.
;
; The frame, from iy:
;   iy+0..7    the divisor, and a zero at iy+8 so that its top limb reads
;              as 24 bits; then the remainder, when it is done
;   iy+10..18  the dividend moved up, top byte at iy+18, and the quotient
;              when it is done: at iy+11 from the long loop, iy+10 from the
;              byte one
;   iy+19..26  the remainder, from the dividend's top bytes, and a zero at
;              iy+27 for the same reason as iy+8
;   iy+28      the pushed iy: where the dividend is
;   iy+37      the pushed de: where the divisor is
acc_rt_lludivmod_core:
	push	bc
	push	de
	push	hl
	push	ix
	push	iy
	ld	hl, -28
	add	hl, sp
	ld	sp, hl
	push	hl
	pop	iy

	ex	de, hl			; the divisor, copied
	lea	de, iy + 0
	ld	bc, 8
	ldir
	xor	a, a			; and zeros from iy+8 to the top
	ld	b, 20
.lldiv_clear:
	ld	(de), a
	inc	de
	djnz	.lldiv_clear

	lea	hl, iy + 7		; the divisor's bytes, from the top
	ld	b, 8
.lldiv_dlen:
	ld	a, (hl)
	or	a, a
	jr	nz, .lldiv_dsize
	dec	hl
	djnz	.lldiv_dlen
	lea	hl, iy + 11		; zero, which C leaves undefined: say
	jp	.lldiv_store		; zero for both rather than loop
.lldiv_dsize:
	dec	b
	ld	c, b			; c = the bytes that go straight over

	ld	hl, (iy + 28)		; the dividend's bytes, from the top
	ld	de, 7
	add	hl, de
	ld	b, 8
.lldiv_nlen:
	ld	a, (hl)
	or	a, a
	jr	nz, .lldiv_move
	dec	hl
	djnz	.lldiv_nlen
	jr	.lldiv_rounds		; zero: nothing to move, and no rounds

.lldiv_move:				; its top byte to iy+18, and c more --
	push	bc			; or b more, when that is fewer: then
	ex	de, hl			; all of it is the remainder, from its
	lea	hl, iy + 18		; bottom byte up
	ld	a, c
	cp	a, b
	jr	c, .lldiv_up
	ld	a, b
.lldiv_up:
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl
	pop	bc
	push	bc
	ld	a, b
	ld	bc, 0
	ld	c, a
	lddr
	pop	bc

.lldiv_rounds:				; eight for each byte past those
	ld	a, b
	sub	a, c
	jr	nc, .lldiv_some
	xor	a, a			; all of it went over: none
.lldiv_some:
	add	a, a
	add	a, a
	add	a, a
	inc	c
	dec	c
	jp	z, .lldiv_byte_divisor

	ld	ix, (iy + 11)		; the quotient's low limb
	ld	hl, (iy + 14)		; and its middle one, kept on the stack
	push	hl
	ld	hl, (iy + 19)		; the remainder, as far as it has come
	ld	de, (iy + 22)
	ld	bc, (iy + 25)
	or	a, a
	jr	z, .lldiv_done

.lldiv_loop:
	add	ix, ix			; {remainder:quotient} <<= 1
	ex	(sp), hl
	adc	hl, hl
	ex	(sp), hl
	rl	(iy + 17)
	rl	(iy + 18)
	adc	hl, hl
	ex	de, hl
	adc	hl, hl
	ex	de, hl
	rl	c
	rl	b

	push	hl			; the top limbs: divisor - remainder
	ld	hl, (iy + 6)
	or	a, a
	sbc	hl, bc
	jr	c, .lldiv_fits_pop	; the remainder's is larger
	jr	nz, .lldiv_no_pop	; the divisor's is
	ld	hl, (iy + 3)		; equal: the middle ones, and the
	sbc	hl, de			; carry is clear
	jr	c, .lldiv_fits_pop
	jr	nz, .lldiv_no_pop
	pop	hl			; equal: the low ones, the other way
	push	bc			; round -- remainder - divisor, and
	ld	bc, (iy + 0)		; add it back, which leaves the carry
	sbc	hl, bc			; the borrow said
	add	hl, bc
	pop	bc
	jr	c, .lldiv_next
	jr	.lldiv_fits
.lldiv_no_pop:
	pop	hl
	jr	.lldiv_next
.lldiv_fits_pop:
	pop	hl
.lldiv_fits:
	inc	ix			; the bit the shift left empty
	push	bc			; remainder -= divisor
	ld	bc, (iy + 0)
	or	a, a
	sbc	hl, bc
	ex	de, hl
	ld	bc, (iy + 3)
	sbc	hl, bc
	ex	de, hl
	ex	(sp), hl		; the top limb, the low one kept
	ld	bc, (iy + 6)		; the zero at iy+8 keeps BC's top
	sbc	hl, bc			; byte zero: only c and b are
	ld	c, l			; taken back
	ld	b, h
	pop	hl
.lldiv_next:
	dec	a
	jr	nz, .lldiv_loop

.lldiv_done:
	ld	(iy + 0), hl		; the remainder, over the divisor
	ld	(iy + 3), de
	ld	(iy + 6), bc
	pop	hl			; the quotient
	ld	(iy + 14), hl
	ld	(iy + 11), ix
	lea	hl, iy + 11
	jr	.lldiv_store

.lldiv_byte_divisor:
	ld	b, a			; the rounds
	ld	c, (iy + 0)		; the divisor
	ld	ix, (iy + 10)		; the quotient, all 72 bits of it
	ld	hl, (iy + 13)
	ld	de, (iy + 16)
	xor	a, a			; the remainder
	or	a, b
	jr	z, .lldiv_byte_done
	xor	a, a
.lldiv_byte:
	add	ix, ix			; {remainder:quotient} <<= 1
	adc	hl, hl
	ex	de, hl
	adc	hl, hl
	ex	de, hl
	rla
	jr	c, .lldiv_byte_fits	; the ninth bit: it fits
	cp	a, c
	jr	c, .lldiv_byte_next
.lldiv_byte_fits:
	sub	a, c
	inc	ix
.lldiv_byte_next:
	djnz	.lldiv_byte

.lldiv_byte_done:
	ld	(iy + 10), ix		; the quotient, a byte down
	ld	(iy + 13), hl
	ld	(iy + 16), de
	lea	hl, iy + 0		; the remainder, over the divisor
	ld	(hl), a
	inc	hl
	xor	a, a
	ld	b, 7
.lldiv_byte_zero:
	ld	(hl), a
	inc	hl
	djnz	.lldiv_byte_zero
	lea	hl, iy + 10

.lldiv_store:				; hl -> the quotient
	ld	de, (iy + 28)
	ld	bc, 8
	ldir
	lea	hl, iy + 0
	ld	de, (iy + 37)
	ld	bc, 8
	ldir

	ld	hl, 28
	add	hl, sp
	ld	sp, hl
	pop	iy
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret

; The remainder the core left where the divisor was, moved to (iy).
acc_rt_llrem_move:
	push	bc
	push	de
	push	hl
	ex	de, hl
	lea	de, iy + 0
	ld	bc, 8
	ldir
	pop	hl
	pop	de
	pop	bc
	ret

acc_rt_llabs_iy:
	bit	7, (iy + 7)
	ret	z
acc_rt_llneg_iy:
	push	hl
	lea	hl, iy + 0
	call	acc_rt_llneg_hl
	pop	hl
	ret

acc_rt_llabs_ix:
	bit	7, (ix + 7)
	ret	z
	push	hl
	lea	hl, ix + 0
	call	acc_rt_llneg_hl
	pop	hl
	ret
