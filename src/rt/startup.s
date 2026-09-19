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
; The entry stub acc puts at the front of every image.
;
; MOS lands on the first byte after the header, so this is what runs. It calls
; main and then says what came back, in one of two ways:
;
;   printing   the result as six hex digits and returning to MOS. This is the
;              default, because it is the one that works on a real Agon and at
;              a command prompt.
;   reporting  it to IO port 0, which stops the emulator with the low byte as
;              its exit status. That is how the test suite reads an answer
;              without a C library, and acc emits it for -x.
;
; Assembled to get the bytes that gen.c embeds; see the comment there.
;
	.assume adl=1
	.section .text,"ax",@progbits
	.global _acc_startup_print
	.global _acc_startup_exit
	.extern _main

_acc_startup_exit:
	call	_main
	ld	a, l
	out	(0), a
	ret

_acc_startup_print:
	call	_main
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
