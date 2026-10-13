;
; The hash of a name, on the Agon.
;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Only the Agon build assembles this: the host build has the same hash in C,
; in src/hash.h, and test/hash.sh holds this to it over thousands of names on
; the emulator. Every identifier the lexer reads comes through here, and
; agondev's clang kept the low lane and the count in the frame and made the
; loop fifty cycles a character, a twentieth of a compile. By hand every value
; lives in a register:
;
;   DE  the next character      HL  the table, its page, L the index
;   C   low: the Pearson lane   B   high: doubled and the character added
;   IYL characters left in this run of 256, IYH the runs
;
; The table is aligned to 256 bytes, so that an index into it is a load into
; L. The count is taken in runs of 256, as marks.s takes it: the first run is
; n's low byte, 0 meaning 256, and every other run is 256. A name is read
; from the lexer's window of 16 KB, so the runs fit IYH.
;
;   unsigned name_hash(const char *text, unsigned n)
;
; answers high, mixed through the table, in L, and low in H: the two bytes
; name_home masks into a bucket's offset.

	.assume adl=1
	.section .text,"ax",@progbits
	.global _name_hash

; The permutation of 0..255 in src/hash.h, page aligned: first in the
; section, so that the alignment pads in front of it and not after the code.
	.balign 256
pearson:
	.byte 203, 49, 73, 37, 250, 58, 130, 246, 54, 244, 89, 107, 253, 227, 167, 57
	.byte 3, 231, 230, 80, 9, 235, 197, 113, 251, 78, 22, 47, 193, 238, 176, 132
	.byte 109, 217, 225, 190, 53, 236, 72, 120, 75, 242, 39, 205, 70, 20, 64, 5
	.byte 166, 69, 15, 86, 100, 212, 177, 232, 41, 60, 23, 171, 8, 108, 125, 82
	.byte 219, 165, 26, 28, 180, 25, 218, 74, 209, 133, 102, 51, 207, 118, 92, 237
	.byte 153, 18, 211, 81, 36, 127, 43, 175, 85, 52, 189, 131, 183, 17, 95, 192
	.byte 213, 88, 16, 1, 110, 99, 119, 240, 186, 172, 170, 228, 137, 162, 158, 145
	.byte 255, 214, 199, 65, 157, 0, 160, 245, 91, 35, 181, 10, 59, 243, 194, 123
	.byte 134, 79, 249, 115, 178, 34, 101, 144, 248, 239, 84, 141, 67, 185, 215, 140
	.byte 184, 198, 149, 173, 187, 45, 223, 210, 117, 38, 161, 252, 50, 229, 216, 139
	.byte 247, 98, 96, 90, 14, 2, 168, 196, 42, 30, 56, 87, 201, 24, 142, 4
	.byte 33, 126, 148, 71, 55, 159, 147, 13, 29, 234, 31, 179, 77, 150, 233, 114
	.byte 200, 155, 104, 66, 224, 83, 6, 93, 12, 19, 7, 48, 122, 182, 220, 254
	.byte 76, 152, 121, 46, 202, 206, 11, 124, 191, 32, 103, 208, 135, 106, 226, 105
	.byte 204, 116, 163, 138, 68, 111, 44, 63, 62, 136, 156, 143, 188, 128, 154, 146
	.byte 169, 112, 151, 195, 221, 61, 129, 94, 27, 164, 222, 40, 241, 97, 174, 21

_name_hash:
	push iy
	ld iy, 0
	add iy, sp
	ld de, (iy+6)			; text
	ld hl, (iy+9)			; n
	ld c, l				; low starts as n's low byte
	ld b, 0				; high
	ld a, l
	or a, h
	jr z, mix			; no characters

	push bc
	ld bc, 255
	add hl, bc
	pop bc
	ld a, h
	ld iyh, a			; the runs: (n + 255) >> 8
	ld a, c
	ld iyl, a			; the first run
	ld hl, pearson

char:
	ld a, (de)			; the character
	inc de
	ld l, a
	ld a, b
	add a, a
	add a, l
	ld b, a				; high = high + high + c
	ld a, l
	xor a, c
	ld l, a
	ld c, (hl)			; low = pearson[low ^ c]
	dec iyl
	jr nz, char
	dec iyh				; and IYL is 0: the next run is 256
	jr nz, char

mix:
	ld hl, pearson
	ld l, b
	ld a, (hl)
	xor a, c			; high = pearson[high] ^ low
	ld hl, 0
	ld l, a
	ld h, c
	pop iy
	ret
