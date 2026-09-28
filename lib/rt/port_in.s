;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_port_in

	.assume adl=1
	SEGMENT CODE

; int acc_rt_port_in(int port) and void acc_rt_port_out(int port, int value):
; the eZ80's own I/O space, where the GPIO ports and the timers are, which
; C has no way to reach. The port is the whole of BC, as IN r,(C) takes it.


_acc_rt_port_in:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+3)
	in	a, (c)
	ld	hl, 0
	ld	l, a
	ret
