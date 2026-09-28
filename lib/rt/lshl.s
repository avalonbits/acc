;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lshl

	.assume adl=1
	SEGMENT CODE

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
