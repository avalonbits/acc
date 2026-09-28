;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_setjmp

	.assume adl=1
	SEGMENT CODE

;---------------------------------------------------------------- setjmp
; setjmp and longjmp, which C cannot write: they are the stack pointer and
; the frame pointer, put back. Named as C names them rather than as acc_rt_
; helpers, since a program calls them itself.
;
; A jmp_buf is three words: where setjmp was called from, IX -- the one
; register a callee must give back, so the only one the caller can have
; anything in across the call -- and the stack pointer as it was at the
; call, pointing at the return address. Everything else a function holds
; across a call it holds in its frame, and the frame is where IX says.
;
; longjmp puts IX and the stack back, drops the return address as `ret`
; would have, and goes to it with the value -- or 1 for a 0, since C says
; setjmp may not appear to return 0 twice.


; int setjmp(jmp_buf env)
_setjmp:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)		; env
	ld	de, (iy+0)		; where to come back to
	ld	(hl), de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), ix
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), iy		; the stack, at the return address
	ld	hl, 0			; and 0, the first time
	ret
