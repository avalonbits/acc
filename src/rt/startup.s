;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; acc is free software; you can redistribute it and/or modify it under the
; terms of the GNU Lesser General Public License as published by the Free
; Software Foundation; either version 2.1 of the License, or (at your option)
; any later version.
;
; In addition to the permissions in the GNU Lesser General Public License,
; the author gives you unlimited permission to link the compiled version of
; this file with other programs, and to distribute those programs without any
; restriction coming from the use of this file. (The GNU Lesser General Public
; License restrictions do apply in other respects; for example, they cover
; modification of the file, and distribution when not linked into another
; program.)
;
; Note that people who make modified versions of this file are not obligated
; to grant this special exception for their modified versions; it is their
; choice whether to do so. The GNU Lesser General Public License gives
; permission to release a modified version without this exception; this
; exception also makes it possible to release a modified version which carries
; forward this exception.
;
; The exception is glibc's, from its startup code, and is here for the same
; reason: acc copies this code into every program it compiles, and a program
; does not take on acc's license by being compiled by it.
;
; The entry stub acc puts at the front of every image, and the routine that
; makes argc and argv out of what MOS passed.
;
; MOS lands on the first byte after the header, so this is what runs. It saves
; what MOS wants back, clears what starts at zero, turns the command line into
; arguments, calls main and then says what came back, in one of two ways:
;
;   printing   the result as six hex digits and returning to MOS. This is the
;              default, because it is the one that works on a real Agon and at
;              a command prompt.
;   reporting  it to IO port 0, which stops the emulator with the low byte as
;              its exit status. That is how the test suite reads an answer
;              without a C library, and acc emits it for -x.
;
; Six addresses in the stub are not known when it is written -- the two
; routines that are emitted at the end of the image, main itself, the two
; cells behind this stub that exit unwinds through, and the place in here that
; it unwinds to -- and are left as holes for gen.c to fill. The same goes
; for the table of pointers and the name in the argument routine.
;
; The two cells are written just before main is called, so that exit has a
; stack to go back to and somewhere to carry on from: it puts them into SP and
; the program counter, and the tail below then runs exactly as though main had
; returned. That is why the arguments are pushed before the stack is saved --
; the two pops after the call take them off either way.
;
; main runs on a stack of its own, at the top of the program's memory, and
; not on the one MOS called in on. MOS's is in its own RAM, above the
; program's, and its variables are below it: a program that went a few tens
; of kilobytes deep wrote over them, and the next interrupt took the machine
; down. The heap already stops short of the top of the program's memory for
; the stack to come down into -- see ACC_STACK_RESERVE -- and agondev's
; startup moves the stack there too. MOS's stack pointer is kept in a cell
; behind this stub and put back before anything is returned to MOS; exit
; unwinds to the tail that does it, as a return does.
;
; iy is saved here and not left to main's prologue for the same reason: exit
; unwinds past every epilogue that would have put it back. ix is not, because
; MOS does not ask for it -- the program that returns normally gives it back
; only because main's epilogue happens to.
;
; Assembled to get the bytes that gen.c embeds; see the comment there.
;
	.assume adl=1
	.section .text,"ax",@progbits
	.global _acc_startup_print
	.global _acc_startup_exit
	.global _acc_args

_acc_startup_exit:
	push	iy			; MOS wants it back as it left it
	push	hl			; and hands over its command line in it
	call	0			; [hole] clear what starts at zero
	pop	hl
	call	0			; [hole] argc and argv
	ld	(0), sp			; [hole] MOS's stack, given back at the end
	ld	sp, 0			; [hole] the program's own: the top of its memory
	push	hl			; argv
	push	de			; argc, which main reads first
	ld	(0), sp			; [hole] the stack main is called on
	ld	hl, 0			; [hole] where main returns to
	ld	(0), hl			; [hole] the cell that remembers it
	call	0			; [hole] main
	pop	bc			; the caller takes the arguments back
	pop	bc
	ld	sp, (0)			; [hole] MOS's stack again
	ld	a, l
	out	(0), a
	pop	iy
	ret

_acc_startup_print:
	push	iy
	push	hl
	call	0			; [hole] clear what starts at zero
	pop	hl
	call	0			; [hole] argc and argv
	ld	(0), sp			; [hole] MOS's stack, given back at the end
	ld	sp, 0			; [hole] the program's own: the top of its memory
	push	hl
	push	de
	ld	(0), sp			; [hole] the stack main is called on
	ld	hl, 0			; [hole] where main returns to
	ld	(0), hl			; [hole] the cell that remembers it
	call	0			; [hole] main
	pop	bc
	pop	bc
	ld	sp, (0)			; [hole] MOS's stack again
	push	hl			; the result, so its bytes can be read
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 2)		; most significant byte first
	call	hexbyte
	ld	a, (iy + 1)
	call	hexbyte
	ld	a, (iy + 0)
	call	hexbyte
	pop	hl
	ld	a, 13
	rst.lil	$10
	ld	a, 10
	rst.lil	$10
	pop	iy
	ret

hexbyte:
	push	af
	rra
	rra
	rra
	rra
	call	hexnib
	pop	af
hexnib:
	and	a, $0f
	add	a, '0'
	cp	a, '9' + 1
	jr	c, emit
	add	a, 'A' - '0' - 10
emit:
	rst.lil	$10
	ret

; hl holds what MOS passed: the command line past the name that was typed.
; Each word in it is given a zero of its own and a slot in the table, and the
; name -- which MOS does not pass -- is put in front as argv[0]. Answers with
; hl = argv and de = argc, which the stub pushes in that order.
;
; The sixteen-argument limit is agondev's, and is here for the same reason the
; walk is: a program that works under one startup has to work under the other.
_acc_args:
	ld	ix, 0			; [hole] the table of pointers
	ld	bc, 0			; [hole] the name this was built under
	ld	(ix + 0), bc		; argv[0]
	lea	ix, ix + 3
	call	args_spaces
	ld	c, 1			; argc
	ld	b, 16			; and the most there is room for
args_next:
	push	bc
	push	hl
	call	args_token		; c = its length, hl = past it
	ld	a, c
	pop	de			; where it started
	pop	bc
	or	a, a
	jr	z, args_done
	ld	(ix + 0), de
	push	hl
	pop	de			; where it ended, to be zeroed
	call	args_spaces		; hl = the next one
	xor	a, a
	ld	(de), a			; and this one ends here
	lea	ix, ix + 3
	inc	c
	ld	a, c
	cp	a, b
	jr	c, args_next
args_done:
	ld	de, 0
	ld	e, c
	ld	hl, 0			; [hole] the table of pointers
	ret

args_token:
	ld	c, 0
args_token_1:
	ld	a, (hl)
	or	a, a
	ret	z
	cp	a, 13
	ret	z
	cp	a, ' '
	ret	z
	inc	hl
	inc	c
	jr	args_token_1

args_spaces:
	ld	a, (hl)
	cp	a, ' '
	ret	nz
	inc	hl
	jr	args_spaces
