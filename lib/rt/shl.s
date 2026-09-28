;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_shl

	.assume adl=1
	SEGMENT CODE

; ---------------------------------------------------------------- shifts
; hl = hl shifted by c places. C leaves a shift of the width or more
; undefined; the count is masked to five bits so the loop terminates, and a
; count of 24 or more then shifts every bit out, which is a defensible answer.

_acc_rt_shl:
	ld	a, c
	and	a, 31
	ret	z
.shl_loop:
	add	hl, hl
	dec	a
	jr	nz, .shl_loop
	ret
