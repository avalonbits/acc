;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_ftoll
	XREF	acc_rt_ftol_entry
	XREF	acc_rt_llneg_hl

	.assume adl=1
	SEGMENT CODE

; (hl): a float in the first four bytes in, eight bytes of integer out,
; truncated towards zero. Below 2^31 that is ftol's answer, sign extended.
; From there up the significand is shifted into place -- which is how an
; unsigned long long above 2^63 comes out right too, so one routine serves
; both. Too large for either is undefined, and is left to wrap.
_acc_rt_ftoll:
	push	iy
	push	bc
	push	hl
	pop	iy
	push	hl
	ld	a, (iy + 3)		; the biased exponent
	and	a, 0x7f
	add	a, a
	ld	b, a
	ld	a, (iy + 2)
	rlca
	and	a, 1
	add	a, b
	ld	b, a
	cp	a, 158			; 127 + 31
	jr	nc, .ftoll_big

	call	acc_rt_ftol_entry
	ld	a, (iy + 3)
	rla
	sbc	a, a
	ld	(iy + 4), a
	ld	(iy + 5), a
	ld	(iy + 6), a
	ld	(iy + 7), a
	jr	.ftoll_out

.ftoll_big:
	ld	c, (iy + 3)		; the sign, in bit 7
	set	7, (iy + 2)		; the leading 1, at bit 23
	xor	a, a
	ld	(iy + 3), a
	ld	(iy + 4), a
	ld	(iy + 5), a
	ld	(iy + 6), a
	ld	(iy + 7), a
	ld	a, b
	sub	a, 150			; the significand goes e - 150 bits
	ld	b, a			; up: at least 8, so never zero
.ftoll_shl:
	sla	(iy + 0)
	rl	(iy + 1)
	rl	(iy + 2)
	rl	(iy + 3)
	rl	(iy + 4)
	rl	(iy + 5)
	rl	(iy + 6)
	rl	(iy + 7)
	djnz	.ftoll_shl
	bit	7, c
	call	nz, acc_rt_llneg_hl

.ftoll_out:
	pop	hl
	pop	bc
	pop	iy
	ret
