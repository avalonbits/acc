;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into libc.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_strlen

	.assume adl=1
	SEGMENT CODE

;---------------------------------------------------------------- strings
; The string functions of <string.h> that walk a string, and memcmp: in C
; they are a loop of loads, tests and steps, five to twelve times slower
; than the block instructions -- cpir finds a byte, ldir copies, cpi
; compares and counts -- that do the same work here. Named as C names them,
; as setjmp is. The library has no C version of them.
;
; Called as C functions, arguments three bytes each from (sp+3), the answer
; in HL, IX kept. Bytes are compared as unsigned char, as C says; the
; answer is their difference.
;
; cpir with BC = 0 counts down from 2^24, so after finding the zero of a
; string of length n it leaves BC = -(n+1), and 0 - BC is n+1.


; size_t strlen(const char *s)
_strlen:
	ld	hl, 3
	add	hl, sp
	ld	hl, (hl)
	xor	a, a
	ld	bc, 0
	cpir				; carry is still clear, from the xor
	sbc	hl, hl
	scf
	sbc	hl, bc			; 0 - BC - 1: n
	ret
