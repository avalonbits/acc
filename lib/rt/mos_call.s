;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_mos_call

	.assume adl=1
	SEGMENT CODE

;---------------------------------------------------------------- MOS calls
; int acc_rt_mos_call(AccMosRegs *r): a MOS call with every register it may
; want, and every register it may answer in, through one structure:
;
;	+0  HL   +3  DE   +6  BC   +9  IX   +12  IY   (three bytes each)
;	+15 A, the call's number going in and what MOS answers coming out
;	+16 the carry flag MOS left, 0 or 1
;
; Some calls take IX and IY as arguments and some answer in them, so both
; are loaded from the structure and stored back to it; the caller's own are
; kept on the stack meanwhile, IX being its frame. What comes back in HL is
; A, which is what most calls answer with.


_acc_rt_mos_call:
	push	ix
	push	iy
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+9)		; r
	push	hl			; kept, to store the answers through
	push	hl
	pop	iy
	ld	hl, (iy+0)
	ld	de, (iy+3)
	ld	bc, (iy+6)
	ld	ix, (iy+9)
	ld	a, (iy+15)
	ld	iy, (iy+12)
	rst.lil	$08
	ex	(sp), iy		; iy = r, and MOS's iy kept; flags untouched
	ld	(iy+16), 0		; nor does this touch them
	jr	nc, _rt_mos_nc
	ld	(iy+16), 1
_rt_mos_nc:
	ld	(iy+0), hl
	ld	(iy+3), de
	ld	(iy+6), bc
	ld	(iy+9), ix
	ld	(iy+15), a
	pop	hl			; MOS's iy
	ld	(iy+12), hl
	ld	hl, 0
	ld	l, a
	pop	iy
	pop	ix
	ret
