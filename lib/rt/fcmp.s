;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_fcmp
	XREF	_acc_rt_fkey
	XREF	acc_rt_fisnan

	.assume adl=1
	SEGMENT CODE

; How the float at (hl) stands to the one at (de), in a: 0 below, 1 equal, 2
; above, 3 unordered. Both are rewritten as keys on the way, which the caller
; can afford because it passes copies.
;
; Three outcomes would do for integers; floats need a fourth. A NaN is not
; less than, equal to or greater than anything, itself included, so `x < y`
; and `x >= y` are both false when either is one -- which no ordering can
; express, and which is why this returns a code rather than leaving flags for
; the caller's branch to read.
_acc_rt_fcmp:
	push	ix
	push	iy
	push	de
	push	hl

	call	acc_rt_fisnan
	jr	nz, .fcmp_unordered
	ex	de, hl
	call	acc_rt_fisnan
	ex	de, hl
	jr	nz, .fcmp_unordered

	call	_acc_rt_fkey
	ex	de, hl
	call	_acc_rt_fkey
	ex	de, hl

	push	hl
	pop	iy
	push	de
	pop	ix
	ld	a, (iy + 3)
	cp	a, (ix + 3)
	jr	nz, .fcmp_differ
	ld	a, (iy + 2)
	cp	a, (ix + 2)
	jr	nz, .fcmp_differ
	ld	a, (iy + 1)
	cp	a, (ix + 1)
	jr	nz, .fcmp_differ
	ld	a, (iy + 0)
	cp	a, (ix + 0)
	jr	nz, .fcmp_differ
	ld	a, 1
	jr	.fcmp_done

.fcmp_differ:
	jr	c, .fcmp_below
	ld	a, 2
	jr	.fcmp_done
.fcmp_below:
	xor	a, a
	jr	.fcmp_done
.fcmp_unordered:
	ld	a, 3

.fcmp_done:
	pop	hl
	pop	de
	pop	iy
	pop	ix
	ret
