;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	acc_rt_wcopy8

	.assume adl=1
	SEGMENT CODE

; (bc) copied to (hl), 8 bytes: the left operand of a wide operator put
; where its answer is to be, for the routines' entries that end in 3 --
; called with the left operand's address in BC as well -- to fall into the
; routine from. Keeps BC, DE and HL. An ldir: eight bytes a byte at a time
; through A, as wcopy4 copies four, would be twice the code for little.
acc_rt_wcopy8:
	push	hl
	push	de
	push	bc
	ex	de, hl			; DE where it goes
	pop	hl			; HL where it comes from
	push	hl
	ld	bc, 8
	ldir
	pop	bc
	pop	de
	pop	hl
	ret
