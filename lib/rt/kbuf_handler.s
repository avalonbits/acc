;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later WITH AdditionRef-acc-runtime-exception
;
; Part of acc's runtime, assembled by zap into rt.a: see lib/rt/README.md
; for what the runtime is, its calling convention, and the exception to the
; licence that lets a program carry it.
;

	XDEF	_acc_rt_kbuf_handler
	XREF	acc_rt_kbuf_end
	XREF	acc_rt_kbuf_ring
	XREF	acc_rt_kbuf_slots
	XREF	acc_rt_kbuf_start

	.assume adl=1
	SEGMENT CODE

; ================================================================ the keyboard
; kbuf's ring of key events, filled as MOS gets each key packet: see
; lib/keyboard.c, which starts it, empties it and ends it. The handler is
; what MOS calls, from its interrupt, with DE pointing at the packet's four
; bytes -- ascii, modifiers, virtual key, down -- and it has to be assembly,
; since nothing a C function is called with says where DE pointed.
;
; The ring is `slots` events of four bytes, `start` the next to read and
; `end` the next to write; `start == end` is empty, and a packet that would
; make `end` reach `start` is dropped, so it holds slots - 1 at most. Only
; the handler moves `end` and only C moves `start`, a byte at a time, so
; neither can see the other half done. slots of 0 is 256: the index wraps
; there on its own.
;
; A program carries it only when it calls kbuf_init.


; MOS saves nothing for a handler, and whatever this uses it gets back.
_acc_rt_kbuf_handler:
	push	af
	push	bc
	push	de
	push	hl
	ld	a, (acc_rt_kbuf_end)
	ld	bc, 0
	ld	c, a
	ld	hl, (acc_rt_kbuf_ring)	; the slot: ring + end * 4
	add	hl, bc
	add	hl, bc
	add	hl, bc
	add	hl, bc
	ex	de, hl
	ld	bc, 4
	ldir
	inc	a			; the next end, round the ring
	ld	hl, acc_rt_kbuf_slots
	cp	a, (hl)
	jr	nz, .kbuf_not_round
	xor	a, a
.kbuf_not_round:
	ld	hl, acc_rt_kbuf_start
	cp	a, (hl)
	jr	z, .kbuf_full		; full: this one is dropped
	ld	(acc_rt_kbuf_end), a
.kbuf_full:
	pop	hl
	pop	de
	pop	bc
	pop	af
	ret
