;
; The startup the oracle build uses.
;
; The reference answer for a test comes from compiling the same source with
; agondev and running it, so both builds have to report the same way: call
; main and hand the low byte of its result to IO port 0, which the emulator
; turns into its exit status.
;
; agondev's own crt0 is not used. It runs initialisers and starts stdio,
; which a program that only computes does not need, and it would put its own
; header at the front of the image. What it does that such a program does
; need is clear bss: a global with no initial value is zero, C says, and
; without this it was whatever the memory last held -- the reference answer
; for a test of globals came out wrong, and a test of acc against it failed
; for a reason that had nothing to do with acc.
;
	.assume adl=1
	.section .text,"ax",@progbits
	.global _start
	.extern _main
	.extern __bss_start
	.extern _end

_start:
	jp	entry			; MOS enters here and this jumps the header
	.space	0x3c, 0			; the 60-byte name field
	.db	"MOS"			; at offset 0x40
	.db	0			; header version
	.db	1			; ADL, 24-bit addressing
entry:
	ld	hl, __bss_start		; bss is [__bss_start, _end), and zero
	ld	de, _end
clear:
	or	a, a
	sbc	hl, de			; at the end yet?
	jr	z, cleared
	add	hl, de			; back to where it was; Z is left alone
	ld	(hl), 0
	inc	hl
	jr	clear
cleared:
	ld	ix, 0			; a frame for main to return through
	call	_main
	ld	a, l			; the low byte of the result
	out	(0), a			; which stops the emulator with that status
	ret
