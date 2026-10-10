;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_mulinv3

	.assume adl=1
	SEGMENT CODE

; hl = hl * 0xaaaaab, twenty-four bits, wrapping: the inverse of 3, which a
; pointer difference on three-byte elements -- ints and pointers -- is
; multiplied by to divide it by 3 exactly. A and the flags clobbered,
; everything else kept.
;
; 0xaaaaab is -0x555555, and 0x555555 is 5 * 17 * 257 * 65537 as far as
; twenty-four bits go: each of those is the value so far added to itself
; shifted up 2, 4, 8 and 16 places. The shifts by 8 and 16 move whole
; bytes, through the stack, where the top byte of HL has a place to be
; read from. About half the cycles of acc_rt_mul's general multiply.

_acc_rt_mulinv3:
	push	de
	push	hl
	pop	de			; de = x
	add	hl, hl
	add	hl, hl
	add	hl, de			; x * 5
	push	hl
	pop	de
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, de			; x * 0x55
	ex	de, hl
	push	de			; that shifted up a byte: H and L a place
	dec	sp			; up, read back from a byte lower
	pop	hl
	inc	sp
	ld	l, 0
	add	hl, de			; x * 0x5555
	ex	de, hl
	push	de			; and up two: L into the top byte
	dec	sp
	dec	sp
	pop	hl
	inc	sp
	inc	sp
	ld	h, 0
	ld	l, 0
	add	hl, de			; x * 0x555555
	ex	de, hl
	or	a, a
	sbc	hl, hl
	sbc	hl, de			; its negative, x * 0xaaaaab
	pop	de
	ret
