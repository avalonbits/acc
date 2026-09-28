;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_longjmp

	.assume adl=1
	SEGMENT CODE

; void longjmp(jmp_buf env, int val)
_longjmp:
	pop	bc			; our own return address, not wanted
	pop	hl			; env
	pop	de			; val
	ld	bc, (hl)		; where setjmp was called from
	inc	hl
	inc	hl
	inc	hl
	ld	ix, (hl)
	inc	hl
	inc	hl
	inc	hl
	push	de			; val, while DE takes the stack
	ld	de, (hl)
	inc	hl
	inc	hl
	inc	hl
	ld	iy, (hl)
	pop	hl			; val
	ex	de, hl
	ld	sp, hl
	inc	sp			; past the return address, as ret leaves it
	inc	sp
	inc	sp
	ex	de, hl			; val
	ld	de, 0
	or	a, a
	sbc	hl, de
	jr	nz, _rt_jmp_go
	inc	hl
_rt_jmp_go:
	push	bc
	ret
