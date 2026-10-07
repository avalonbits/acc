;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_memset

	.assume adl=1
	SEGMENT CODE

;	hl = where, a = the byte, bc = how many
;	returns hl = where, which is what memset answers
; The tests for none are made in place, not called: a struct cleared with
; memset on every line of a program's input spends more in two calls than
; in the fill.
_acc_rt_memset:
	push	hl			; the answer
	ex	de, hl			; de = where
	or	a, a			; carry clear, and A kept
	sbc	hl, hl
	sbc	hl, bc			; zero when there are none
	jr	z, _rt_fill_done
	ld	(de), a			; the first one by hand, then copy it
	dec	bc			; along, which is how ldir fills
	or	a, a
	sbc	hl, hl
	sbc	hl, bc			; zero when that was the only one
	jr	z, _rt_fill_done
	push	de
	pop	hl
	inc	de
	ldir
_rt_fill_done:
	pop	hl
	ret
