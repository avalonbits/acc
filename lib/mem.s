;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_memcpy
	XDEF	_memmove
	XDEF	_memset
	XDEF	_memchr

	.assume adl=1
	SEGMENT CODE

;---------------------------------------------------------------- memory
; The memory half of <string.h>, with ldir, lddr and cpir. acc calls its
; runtime's own versions of these where a program names them, with the
; operands in registers; these are the ones a program reaches through a
; pointer, and the ones the library's C calls.
;
; ldir and its kin count BC down from 2^24 when it starts at zero, so a
; count of nothing is looked for first.
;
; C functions: arguments three bytes each from (sp+3), the answer in HL,
; IX kept.

; void *memcpy(void *to, const void *from, size_t n)
_memcpy:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when n is 0
	jr	z, .mem_to
	ld	de, (iy+3)
	ld	hl, (iy+6)
	ldir
.mem_to:
	ld	hl, (iy+3)
	ret

; void *memmove(void *to, const void *from, size_t n): forwards unless
; `to` is inside the n bytes from `from`, where forwards would copy over
; what it has still to read.
_memmove:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc
	jr	z, .mem_to
	ld	de, (iy+3)		; to
	ld	hl, (iy+6)		; from
	or	a, a
	sbc	hl, de
	jr	nc, .move_up		; from at or past to
	ld	hl, (iy+6)
	add	hl, bc
	or	a, a
	sbc	hl, de
	jr	c, .move_up		; from + n before to: apart
	jr	z, .move_up
	ld	hl, (iy+6)		; from the last bytes down
	add	hl, bc
	dec	hl
	ex	de, hl
	add	hl, bc
	dec	hl
	ex	de, hl
	lddr
	jr	.mem_to
.move_up:
	ld	hl, (iy+6)
	ldir
	jr	.mem_to

; void *memset(void *s, int c, size_t n): the first byte by hand, then
; ldir copies it along.
_memset:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc
	jr	z, .mem_to
	ld	hl, (iy+3)
	ld	a, (iy+6)
	ld	(hl), a
	dec	bc
	push	hl
	or	a, a
	sbc	hl, hl
	adc	hl, bc			; Z when that was the only one
	pop	hl
	jr	z, .mem_to
	push	hl
	pop	de
	inc	de
	ldir
	jr	.mem_to

; void *memchr(const void *s, int c, size_t n)
_memchr:
	ld	iy, 0
	add	iy, sp
	ld	bc, (iy+9)
	or	a, a
	sbc	hl, hl
	adc	hl, bc
	ret	z			; HL is 0: nothing to look in
	ld	hl, (iy+3)
	ld	a, (iy+6)
	cpir
	dec	hl			; cpir stepped past what it matched
	ret	z
	or	a, a
	sbc	hl, hl
	ret
