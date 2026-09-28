;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_memcpy
	XDEF	_acc_rt_memmove
	XREF	acc_rt_bc_is_zero

	.assume adl=1
	SEGMENT CODE

; Copying and filling, which the eZ80 does in one instruction where C does
; it a byte at a time. acc calls these where a program says memcpy, memmove
; or memset: the library still has its own, for a program that takes their
; address, but nothing reaches them by name any more.
;
; ldir and lddr read the count as the full twenty-four bits of BC, so a count
; of nothing copies sixteen megabytes rather than nothing at all. The guard
; is here and not at the call, because here it is written once.
;
;	hl = source, de = destination, bc = how many
;	returns hl = the destination, which is what memcpy answers


_acc_rt_memcpy:
	push	de			; the answer
	call	acc_rt_bc_is_zero
	jr	z, _rt_copy_done
	ldir
_rt_copy_done:
	pop	hl
	ret

; The same, but right to left when the blocks overlap the wrong way round.
; Overlapping forwards is what ldir already does correctly.
_acc_rt_memmove:
	push	de
	call	acc_rt_bc_is_zero
	jr	z, _rt_copy_done
	push	hl			; source below destination: go backwards
	sbc	hl, de			; carry is clear, from bc_is_zero
	pop	hl
	jr	nc, _rt_move_up
	add	hl, bc			; both ends, one past the last byte
	dec	hl
	ex	de, hl
	add	hl, bc
	dec	hl
	ex	de, hl
	lddr
	pop	hl
	ret
_rt_move_up:
	ldir
	pop	hl
	ret
