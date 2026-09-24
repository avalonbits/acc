; exit and abort for a reference build: the status goes where main's result
; would have, and start.s prints it and returns to MOS.
	.assume adl=1
	.section .text,"ax",@progbits
	.global _exit
	.global _abort
_exit:
	pop	de			; the return address, not wanted
	pop	hl			; the status
	jp	__ref_finish
_abort:
	ld	hl, 134
	jp	__ref_finish
