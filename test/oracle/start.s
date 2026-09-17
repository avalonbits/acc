;
; The startup the oracle build uses.
;
; The reference answer for a test comes from compiling the same source with
; agondev and running it, so both builds have to report the same way: call
; main and hand the low byte of its result to IO port 0, which the emulator
; turns into its exit status.
;
; agondev's own crt0 is not used. It clears bss, runs initialisers and starts
; stdio, none of which a program that only computes needs, and it would put
; its own header at the front of the image.
;
	.assume adl=1
	.section .text,"ax",@progbits
	.global _start
	.extern _main

_start:
	jp	entry			; MOS enters here and this jumps the header
	.space	0x3c, 0			; the 60-byte name field
	.db	"MOS"			; at offset 0x40
	.db	0			; header version
	.db	1			; ADL, 24-bit addressing
entry:
	ld	ix, 0			; a frame for main to return through
	call	_main
	ld	a, l			; the low byte of the result
	out	(0), a			; which stops the emulator with that status
	ret
