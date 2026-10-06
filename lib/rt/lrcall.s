;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	acc_rt_lrcall

	.assume adl=1
	SEGMENT CODE

; e:hl = e:hl op a:bc, for the long routines that work on longs in memory:
; the routine's address on the stack above the return, put there by the
; one that jumps here. Both operands go into eight bytes on the stack, the
; routine is called with hl at the left and de at the right, as those take
; them, and the answer comes back off it. Everything but a and the flags is
; kept, as the routines in registers keep it.
acc_rt_lrcall:
	push	iy
	push	ix
	push	de
	push	bc
	ld	ix, -8
	add	ix, sp
	ld	sp, ix
	ld	(ix + 0), hl
	ld	(ix + 3), e
	ld	(ix + 4), bc
	ld	(ix + 7), a
	ld	iy, (ix + 20)		; the routine: past the 8 and bc, de, ix, iy
	lea	hl, ix + 0
	lea	de, ix + 4
	call	.lrcall_iy
	ld	hl, (ix + 0)
	ld	a, (ix + 3)
	ld	ix, 8
	add	ix, sp
	ld	sp, ix
	pop	bc
	pop	de
	ld	e, a
	pop	ix
	pop	iy
	inc	sp			; the routine's address, dropped
	inc	sp
	inc	sp
	ret
.lrcall_iy:
	jp	(iy)
