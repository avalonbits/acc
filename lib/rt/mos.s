;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_mos

	.assume adl=1
	SEGMENT CODE

; A MOS call: the number in A, and HL, DE and BC as that call wants them.
;
; Four ways out, because MOS answers in whichever register suits what was
; asked -- an error code in A, a count of bytes in DE, a pointer in HL, the
; system variables in IX -- and a C function answers in HL.
;
; The arguments are where acc leaves them, which is where agondev leaves them
; too: three bytes each, the first at (ix+6) once ix is the stack pointer and
; the saved ix and the return address are behind it. IX is saved and put back
; because MOS is free with it, which is what libagon does here as well.

_acc_rt_mos:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ix+9)
	ld	de, (ix+12)
	ld	bc, (ix+15)
	ld	a, (ix+6)
	rst.lil	$08
	ld	hl, 0
	ld	l, a
	pop	ix
	ret
