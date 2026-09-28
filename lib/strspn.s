;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_strspn
	XDEF	_strcspn
	XDEF	_strpbrk

	.assume adl=1
	SEGMENT CODE

; size_t strspn(const char *s, const char *set) and strcspn: how many of s's
; first characters are all in the set, or all not in it; and strpbrk, where
; the first that is in it is. Each character of s is looked for in the set
; with cpir, over the set's length measured once and kept on the stack,
; since cpir wants it in BC every time. strspn walks while a character is
; found and strcspn while it is not, a loop each.
_strspn:
	ld	iy, 0
	add	iy, sp
	call	.span_setup
	jr	z, .span_count		; an empty set: none of s is in it
	push	hl
.spn_loop:
	ld	a, (de)
	or	a, a
	jr	z, .spn_end		; s's end
	ld	hl, (iy+6)
	pop	bc
	push	bc
	cpir
	jr	nz, .spn_end		; not in the set
	inc	de
	jr	.spn_loop
.spn_end:
	pop	bc
	jr	.span_count

_strcspn:
	ld	iy, 0
	add	iy, sp
	call	.cspan
.span_count:				; how far the walk got
	ex	de, hl
	ld	de, (iy+3)
	or	a, a
	sbc	hl, de
	ret

; char *strpbrk(const char *s, const char *set)
_strpbrk:
	ld	iy, 0
	add	iy, sp
	call	.cspan
	ex	de, hl
	ld	a, (hl)
	or	a, a
	ret	nz			; one in the set
	sbc	hl, hl			; s's end: none
	ret

; DE: the first of s in the set, or s's zero.
.cspan:
	call	.span_setup
	jr	nz, .csp_some
	ex	de, hl			; an empty set: all of s is not in it
	xor	a, a
	cpir				; BC is 0: to the zero
	dec	hl
	ex	de, hl
	ret
.csp_some:
	push	hl
.csp_loop:
	ld	a, (de)
	or	a, a
	jr	z, .csp_end		; s's end
	ld	hl, (iy+6)
	pop	bc
	push	bc
	cpir
	jr	z, .csp_end		; in the set
	inc	de
	jr	.csp_loop
.csp_end:
	pop	bc
	ret

; HL: the set's length, Z when it is 0, and BC 0 then. DE: s.
.span_setup:
	ld	hl, (iy+6)
	xor	a, a
	ld	bc, 0
	cpir
	sbc	hl, hl
	scf
	sbc	hl, bc			; the set's length
	ld	de, (iy+3)
	ld	bc, 0
	or	a, a
	sbc	hl, bc
	ret
