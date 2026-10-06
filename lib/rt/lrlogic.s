;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrand
	XDEF	_acc_rt_lror
	XDEF	_acc_rt_lrxor

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl op a:bc, for &, | and ^. The top bytes are e and a, done
; first while a holds the right's; the low three are hl's and bc's, whose
; top bytes have no name, so both go on the stack and are worked a byte at a
; time there, iy reaching them, and hl comes back off it.
	MACRO	LR_LOGIC op
	push	iy
	push	bc
	push	hl
	ld	iy, 0
	add	iy, sp			; hl's bytes at iy+0, bc's at iy+3
	op	a, e
	ld	e, a
	ld	a, (iy + 0)
	op	a, (iy + 3)
	ld	(iy + 0), a
	ld	a, (iy + 1)
	op	a, (iy + 4)
	ld	(iy + 1), a
	ld	a, (iy + 2)
	op	a, (iy + 5)
	ld	(iy + 2), a
	pop	hl
	pop	bc
	pop	iy
	ret
	ENDMACRO

_acc_rt_lrand:
	LR_LOGIC and
_acc_rt_lror:
	LR_LOGIC or
_acc_rt_lrxor:
	LR_LOGIC xor
