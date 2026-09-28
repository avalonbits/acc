;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lsub
	XDEF	acc_rt_lsub_n

	.assume adl=1
	SEGMENT CODE

; Only (hl) has an `sbc a, (rr)` form, so the right-hand byte goes through C
; on the way. Neither `ld c, a` nor `ld a, (hl)` touches the carry, so the
; borrow still chains from one byte to the next.
_acc_rt_lsub:
	push	bc
	push	de
	push	hl
	ld	b, 4
acc_rt_lsub_n:
	or	a, a
.lsub_loop:
	ld	a, (de)
	ld	c, a
	ld	a, (hl)
	sbc	a, c
	ld	(hl), a
	inc	hl
	inc	de
	djnz	.lsub_loop
	pop	hl
	pop	de
	pop	bc
	ret
