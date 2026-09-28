;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_kbuf_state
	XDEF	acc_rt_kbuf_end
	XDEF	acc_rt_kbuf_ring
	XDEF	acc_rt_kbuf_slots
	XDEF	acc_rt_kbuf_start

	.assume adl=1
	SEGMENT CODE

; unsigned char *acc_rt_kbuf_state(void): where the ring's state is, for
; lib/keyboard.c to set up and read -- slots, start and end, a byte each,
; then the ring's address.
_acc_rt_kbuf_state:
	ld	hl, acc_rt_kbuf_slots
	ret

acc_rt_kbuf_slots:
	.byte	0
acc_rt_kbuf_start:
	.byte	0
acc_rt_kbuf_end:
	.byte	0
acc_rt_kbuf_ring:
	.byte	0, 0, 0
