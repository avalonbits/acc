;
; The marks a file's bytes leave, folded in on the Agon.
;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Only the Agon build assembles this: the host build has the C loop in
; lex.c, which this does exactly, and test/target.sh holds the two builds'
; objects -- which record the marks -- to the same bytes.
;
; Every byte of every file acc reads is folded in here. agondev's clang kept
; both sums, the pointer and the count in the frame, and made the loop 93
; cycles a byte, a quarter of a compile; and the pointer form of the C it
; compiled wrongly, reading from the second byte. By hand it is eight
; instructions and every value lives in a register:
;
;   IY  the next byte           DE  sum         HL  weighted
;   BC  the byte, from C        A   bytes left in this run of 256
;
; The count is taken in runs of 256 so that A can count them: the first run
; is n's low byte, 0 meaning 256, and every other run is 256. The number of
; runs, (n + 255) >> 8, is kept in the frame. The sums are three bytes, as
; the registers are, which is the mask the C applies.
;
;   void marks_fold(unsigned *sump, unsigned *weightedp, const char *bytes,
;                   int n)

	.assume adl=1
	.section .text,"ax",@progbits
	.global _marks_fold

_marks_fold:
	push ix
	ld ix, 0
	add ix, sp
	push iy

	ld hl, (ix+15)			; n
	ld de, 255
	add hl, de
	push hl				; n + 255, at (ix-6)
	ld hl, 0
	ld l, (ix-5)
	ld h, (ix-4)			; the runs: (n + 255) >> 8
	ld (ix-6), hl
	ld a, l
	or a, h
	jr z, done			; n is 0

	ld iy, (ix+12)
	ld hl, (ix+6)
	ld de, (hl)			; sum
	ld hl, (ix+9)
	ld hl, (hl)			; weighted
	ld bc, 0
	ld a, (ix+15)			; the first run

byte:
	ld c, (iy+0)
	inc iy
	ex de, hl
	add hl, bc			; sum += the byte
	ex de, hl
	add hl, de			; weighted += sum
	dec a
	jr nz, byte

	push hl
	ld hl, (ix-6)
	dec hl
	ld (ix-6), hl
	ld a, l
	or a, h				; the runs never reach the top byte
	pop hl
	ld a, 0				; the next run is 256
	jr nz, byte

	ex de, hl			; weighted to DE, sum to HL
	push hl
	ld hl, (ix+9)
	ld (hl), de
	pop de
	ld hl, (ix+6)
	ld (hl), de

done:
	ld iy, (ix-3)
	ld sp, ix
	pop ix
	ret
