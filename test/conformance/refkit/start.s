; The start of a reference build: a test compiled and linked by agondev, to
; say what the right answer is on this machine. MOS's header, memory
; cleared, main called, and its result printed as six hex digits -- the
; line acc's own startup prints -- before returning 0 to MOS whatever the
; result was. MOS 3 stops an autoexec.txt at the first command that fails,
; and one card runs every test in a row. See shim.s for exit and abort.
;
; Memory is cleared from the bss to the top of the heap, which the link puts
; below 0xB0000, and not the bss alone: one card runs many programs, each in
; what the last left, and agondev's library answers differently in memory
; that is not the zeros a fresh machine has -- 27 of gcc's tests gave one
; answer on one import and another on the next.
	.assume adl=1
	.section .text,"ax",@progbits
	.global _start
	.global __ref_finish
_start:
	jp	entry
	.fill	0x40 - 4, 1, 0
	.db	'M', 'O', 'S', 0, 1	; the header: an executable, in ADL mode
entry:
	ld	hl, 0xB0000 - 1		; how many bytes past the first
	ld	de, __bss_start
	or	a, a
	sbc	hl, de
	push	hl
	pop	bc
	ex	de, hl			; the first zeroed, and ldir copies it on
	ld	(hl), 0
	push	hl
	pop	de
	inc	de
	ldir
cleared:
	ld	(__ref_sp), sp		; for exit, from however deep it is called,
	ld	ix, 0			; and after the clearing, which is where
	call	_main			; it is kept
; The result in HL, printed and dropped.
__ref_finish:
	ld	sp, (__ref_sp)
	push	hl
	ld	(__ref_hi), hl		; its top byte, through memory
	ld	a, (__ref_hi + 2)
	call	hex
	ld	a, h
	call	hex
	ld	a, l
	call	hex
	ld	a, 13
	rst.lil	10h
	ld	a, 10
	rst.lil	10h
	pop	hl
	ld	hl, 0			; MOS: it ran
	ret
hex:
	push	af
	rrca
	rrca
	rrca
	rrca
	call	nibble
	pop	af
nibble:
	and	a, 15
	add	a, '0'
	cp	a, '9' + 1
	jr	c, emit
	add	a, 'a' - '9' - 1
emit:
	rst.lil	10h
	ret

	.section .bss,"aw",@nobits
__ref_sp:
	.ds	3
__ref_hi:
	.ds	3
