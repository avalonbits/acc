;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
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
; A jmp_buf is four words: where setjmp was called from, IX -- the one
; register a callee must give back, so the only one the caller can have
; anything in across the call -- the stack pointer as it was at the call,
; pointing at the return address, and IY. Everything else a function holds
; across a call it holds in its frame, and the frame is where IX says; IY
; is the one local acc keeps in a register, which a call to setjmp does not
; save around it (see keeps_iy in src/func.c), so it is kept here, and
; setjmp itself leaves IY as it found it.
;
; longjmp puts IX, the stack and IY back, drops the return address as `ret`
; would have, and goes to it with the value -- or 1 for a 0, since C says
; setjmp may not appear to return 0 twice.


; int setjmp(jmp_buf env)
_setjmp:
	pop	de			; where to come back to
	pop	hl			; env
	push	hl
	push	de
	ld	(hl), de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), ix
	inc	hl
	inc	hl
	inc	hl
	ex	de, hl
	ld	hl, 0
	add	hl, sp			; the stack, at the return address
	ex	de, hl
	ld	(hl), de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), iy
	ld	hl, 0			; and 0, the first time
	ret
