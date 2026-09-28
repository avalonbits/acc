;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_putch

	.assume adl=1
	SEGMENT CODE

; ================================================================ the machine
; What the eZ80 cannot do on its own but MOS can, which is everything to do
; with the world outside the program.
;
; Called from C and not by the code generator, so the argument is where acc
; puts one -- three bytes at (sp+3), past the return address -- rather than
; in a register. What comes back comes back in HL, as from any function acc
; compiles.


; int acc_rt_putch(int c): the low byte of c to the console, and c back.
_acc_rt_putch:
	ld	hl, 3
	add	hl, sp
	ld	a, (hl)
	ld	hl, 0
	ld	l, a
	push	hl
	rst.lil	$10
	pop	hl
	ret
