; The start of a reference build: a test compiled and linked by agondev, to
; say what the right answer is on this machine. MOS's header, the bss
; cleared, main called, and its result printed as six hex digits -- the
; line acc's own startup prints -- before returning 0 to MOS whatever the
; result was. MOS 3 stops an autoexec.txt at the first command that fails,
; and one card runs every test in a row. See shim.s for exit and abort.
	.assume adl=1
	.section .text,"ax",@progbits
	.global _start
	.global __ref_finish
_start:
	jp	entry
	.fill	0x40 - 4, 1, 0
	.db	'M', 'O', 'S', 0, 1	; the header: an executable, in ADL mode
entry:
	ld	hl, __bss_start
	ld	de, _end
clear:
	or	a, a
	sbc	hl, de
	jr	z, cleared
	add	hl, de
	ld	(hl), 0
	inc	hl
	jr	clear
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
