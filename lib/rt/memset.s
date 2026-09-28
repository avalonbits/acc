;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_memset
	XREF	acc_rt_bc_is_zero

	.assume adl=1
	SEGMENT CODE

;	hl = where, a = the byte, bc = how many
;	returns hl = where, which is what memset answers
_acc_rt_memset:
	push	hl
	call	acc_rt_bc_is_zero
	jr	z, _rt_fill_done
	ld	(hl), a			; the first one by hand, then copy it
	dec	bc			; along, which is how ldir fills
	call	acc_rt_bc_is_zero
	jr	z, _rt_fill_done
	push	hl
	pop	de
	inc	de
	ldir
_rt_fill_done:
	pop	hl
	ret
