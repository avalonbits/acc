;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrshru
	XDEF	_acc_rt_lrshrs

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl >> a, a taken as 0 to 31: unsigned, a 0 into the top bit, and
; signed, the sign. A bit goes down from e into hl's top byte, which has no
; name: hl is shifted on the stack, iy reaching it. Whole bytes first, moved
; down one place each with what fills from the top in e -- a shift by 16,
; zap's and ez80asm's, was sixteen turns of the loop, 400 cycles -- and the
; bits left a bit at a time. A and the flags clobbered.
_acc_rt_lrshru:
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.lrshru_bytes:
	cp	a, 8
	jr	c, .lrshru_bits
	sub	a, 8
	push	af
	ld	a, (iy + 1)
	ld	(iy + 0), a
	ld	a, (iy + 2)
	ld	(iy + 1), a
	ld	(iy + 2), e
	ld	e, 0
	pop	af
	jr	.lrshru_bytes
.lrshru_bits:
	or	a, a
	jr	z, .lrshru_done
.lrshru_loop:
	srl	e
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .lrshru_loop
.lrshru_done:
	pop	hl
	pop	iy
	ret

_acc_rt_lrshrs:
	and	a, 31
	ret	z
	push	iy
	push	hl
	ld	iy, 0
	add	iy, sp
.lrshrs_bytes:
	cp	a, 8
	jr	c, .lrshrs_bits
	sub	a, 8
	push	af
	ld	a, (iy + 1)
	ld	(iy + 0), a
	ld	a, (iy + 2)
	ld	(iy + 1), a
	ld	(iy + 2), e
	ld	a, e
	rla
	sbc	a, a
	ld	e, a			; the sign, in every bit
	pop	af
	jr	.lrshrs_bytes
.lrshrs_bits:
	or	a, a
	jr	z, .lrshrs_done
.lrshrs_loop:
	sra	e
	rr	(iy + 2)
	rr	(iy + 1)
	rr	(iy + 0)
	dec	a
	jr	nz, .lrshrs_loop
.lrshrs_done:
	pop	hl
	pop	iy
	ret
