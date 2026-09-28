;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lcmpeq
	XDEF	acc_rt_lcmpeq_n

	.assume adl=1
	SEGMENT CODE

; Z set if the four bytes at (hl) equal those at (de). Neither is changed.
; Compared a byte at a time and stopped at the first difference, rather than
; subtracted: a subtract's Z would describe only the byte it was done on.
_acc_rt_lcmpeq:
	push	bc
	push	de
	push	hl
	ld	b, 4
acc_rt_lcmpeq_n:
.lcmpeq_loop:
	ld	a, (de)
	cp	a, (hl)
	jr	nz, .lcmpeq_differ
	inc	hl
	inc	de
	djnz	.lcmpeq_loop
	xor	a, a			; every byte matched: Z
	jr	.lcmpeq_done
.lcmpeq_differ:
	ld	a, 1
	or	a, a			; NZ, whatever the bytes were
.lcmpeq_done:
	pop	hl			; pop leaves the flags alone
	pop	de
	pop	bc
	ret
