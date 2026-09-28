;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_llneg
	XDEF	acc_rt_llneg_hl

	.assume adl=1
	SEGMENT CODE

; (hl) = -(hl): 0 minus each byte, the borrow chaining up. Neither `ld a, 0`
; nor `inc hl` nor djnz touches the carry.
_acc_rt_llneg:
acc_rt_llneg_hl:
	push	bc
	push	hl
	ld	b, 8
	or	a, a
.llneg_loop:
	ld	a, 0
	sbc	a, (hl)
	ld	(hl), a
	inc	hl
	djnz	.llneg_loop
	pop	hl
	pop	bc
	ret
