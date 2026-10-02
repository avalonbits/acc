;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;
; The work of itoa, ltoa and ultoa, a member of its own so that a program's
; own of one of them does not clash with the library's.
;

	XDEF	acc_lib_toa

	.assume adl=1
	SEGMENT CODE

; itoa, ltoa and ultoa: an integer written out in a base, the Microsoft and
; DOS functions -- not C99's, and not agondev's either, but widely used. The
; digits of the value in the base, 2 to 36, letters past 9 lowercase, and a
; terminator, written to str and answering str; a base outside 2..36 writes
; the empty string. Only base 10 has a sign: in any other the bits are
; written as unsigned, so itoa(-1, s, 16) is "ffffff", an int being 24 bits.
;
; Each digit is one division of the whole value by the base, bit by bit,
; where C would take two of the runtime's long ones, a quotient and a
; remainder. The digits come least first, and are pushed until the last,
; over a zero that becomes the terminator, so that popping them writes them
; out in order.
;
; Each of the three is a member of its own (lib/itoa.s, ltoa.s, ultoa.s),
; and gives this its frame: iy placed so that str is at (iy+9) and the base
; at (iy+12) whichever called, itoa's arguments being a slot narrower than
; the long ones'.

; E:HL is the value; D's bit 7 says it is negative, if the base is 10.
acc_lib_toa:
	ld	a, (iy+13)		; the base: 2..36, all of it
	or	a, (iy+14)
	jr	nz, .toa_bad
	ld	a, (iy+12)
	cp	a, 37
	jr	nc, .toa_bad
	cp	a, 2
	jr	c, .toa_bad
	ld	c, a
	cp	a, 10
	jr	nz, .toa_digits
	bit	7, d
	jr	z, .toa_digits

	; Negative in base 10: a '-' and then the magnitude. For itoa the
	; value is extended to 32 bits first, D being 0xff and E 0, and for
	; ltoa D is E: either way E | D is the top byte to negate.
	ld	a, e
	or	a, d
	ld	e, a
	push	bc
	push	hl
	pop	bc
	xor	a, a
	sbc	hl, hl
	sbc	hl, bc
	sbc	a, e
	ld	e, a
	pop	bc
	push	hl
	ld	hl, (iy+9)
	ld	(hl), '-'
	inc	hl
	ld	(iy+9), hl		; the digits go after it
	pop	hl
	call	.toa_digits
	dec	hl			; str, the '-' included
	ret

.toa_bad:
	ld	hl, (iy+9)
	ld	(hl), 0
	ret

; The digits of E:HL in base C to (iy+9), answering it in HL.
.toa_digits:
	xor	a, a
	push	af			; the terminator, popped last
.toa_digit:
	xor	a, a			; the remainder, below the base
	ld	b, 32
.toa_bit:				; {A:E:HL} <<= 1, and subtract the base
	add	hl, hl			; from A where it fits, a quotient bit
	rl	e			; where the dividend's went
	rla
	cp	a, c
	jr	c, .toa_next
	sub	a, c
	inc	l
.toa_next:
	djnz	.toa_bit
	cp	a, 10
	jr	c, .toa_decimal
	add	a, 'a' - 10 - '0'
.toa_decimal:
	add	a, '0'
	push	af
	ld	a, e			; until the quotient is 0
	or	a, a
	jr	nz, .toa_digit
	ld	de, 0			; E is 0 already
	sbc	hl, de			; carry is clear, from the or
	jr	nz, .toa_digit
	ld	hl, (iy+9)
.toa_out:
	pop	af
	ld	(hl), a
	inc	hl
	or	a, a
	jr	nz, .toa_out
	ld	hl, (iy+9)
	ret
