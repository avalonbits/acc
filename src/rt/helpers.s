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
