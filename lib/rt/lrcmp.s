;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_lrcmpu
	XDEF	_acc_rt_lrcmps

	.assume adl=1
	SEGMENT CODE

; The flags of comparing e:hl with a:bc, for a branch: carry where the left
; is the less, Z where they are equal. The top bytes first, unsigned -- or,
; signed, with their signs turned over, which orders them as signed bytes --
; and where those are equal, the low three, unsigned. Neither is changed.
_acc_rt_lrcmps:
	xor	a, 0x80
	push	de
	ld	d, a
	ld	a, e
	xor	a, 0x80
	jr	.lrcmp_top
_acc_rt_lrcmpu:
	push	de
	ld	d, a
	ld	a, e
.lrcmp_top:
	cp	a, d
	jr	nz, .lrcmp_done
	push	hl
	or	a, a
	sbc	hl, bc
	pop	hl			; pop leaves the flags alone
.lrcmp_done:
	pop	de
	ret
