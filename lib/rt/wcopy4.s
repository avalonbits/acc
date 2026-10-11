;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	acc_rt_wcopy4

	.assume adl=1
	SEGMENT CODE

; (bc) copied to (hl), 4 bytes: the left operand of a wide operator put
; where its answer is to be, for the routines' entries that end in 3 --
; called with the left operand's address in BC as well -- to fall into the
; routine from. Keeps BC, DE and HL; A is the routines' to change anyway.
; A byte at a time through A, BC and HL stepped back after rather than
; pushed and popped: half the cycles of an ldir with what it needs saved.
acc_rt_wcopy4:
	ld	a, (bc)
	ld	(hl), a
	inc	bc
	inc	hl
	ld	a, (bc)
	ld	(hl), a
	inc	bc
	inc	hl
	ld	a, (bc)
	ld	(hl), a
	inc	bc
	inc	hl
	ld	a, (bc)
	ld	(hl), a
	dec	bc
	dec	bc
	dec	bc
	dec	hl
	dec	hl
	dec	hl
	ret
