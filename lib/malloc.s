;
; Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
; SPDX-License-Identifier: LGPL-2.1-or-later
;
; Part of the C library, written in eZ80 assembly and assembled by zap into
; libc.a with the rest of it: see lib/strlen.s for why these are assembly.
;

	XDEF	_malloc
	XREF	_acc_heap
	XREF	_acc_free_list
	XREF	_acc_heap_begin
	XREF	_acc_free_take

	.assume adl=1
	SEGMENT CODE

; void *malloc(size_t n): lib/stdlib.c's heap, its blocks laid out as that
; has them -- next 0, prev 3, size 6, used 9, ten bytes; a free block's
; links in its first bytes, next 10 and prev 13 from the block -- first
; fit among the free blocks, and what is left of the one found a free
; block of its own in its place in the free list, where there is room for
; one. In C, through the first pass's code and three calls, a malloc was
; 183 cycles to agondev's 40, and ez80asm makes 13,564 of them; the room
; at the end of the heap is what nearly every one is split from.
;
; The two rare paths are lib/stdlib.c's: the heap's first block made, and
; a block taken whole out of the free list.
_malloc:
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy+3)		; n
	ld	de, 0
	or	a, a
	sbc	hl, de
	ret	z			; malloc(0): NULL, HL 0 already
	inc	hl
	inc	hl
	res	1, l			; (n + 2) & ~2, as rounded() has it
	ld	de, 6
	or	a, a
	sbc	hl, de
	jr	nc, .malloc_room
	or	a, a
	sbc	hl, hl			; under a free block's links: six
.malloc_room:
	add	hl, de
	push	hl			; n
	ld	hl, (_acc_heap)
	ld	de, 0
	or	a, a
	sbc	hl, de
	call	z, _acc_heap_begin	; the first malloc: the heap made
	pop	bc			; BC: n
	ld	iy, (_acc_free_list)
.malloc_walk:
	push	iy
	pop	hl
	ld	de, 0
	or	a, a
	sbc	hl, de
	ret	z			; none fits: NULL
	ld	hl, (iy+6)		; its size
	or	a, a
	sbc	hl, bc
	jr	nc, .malloc_fits
	ld	iy, (iy+10)		; the next free one
	jr	.malloc_walk

	; HL: what is over. Under n + 10 + 6 + 4 in all, as split() has it,
	; the block is taken whole.
.malloc_fits:
	ld	de, 20
	or	a, a
	sbc	hl, de
	jr	c, .malloc_whole
	push	ix
	lea	hl, iy+10
	add	hl, bc
	push	hl
	pop	ix			; IX: the rest, after n bytes
	ld	hl, (iy+0)
	ld	(ix+0), hl		; rest->next = b->next
	ld	(ix+3), iy		; rest->prev = b
	ld	hl, (iy+6)
	or	a, a
	sbc	hl, bc
	ld	de, 10
	or	a, a
	sbc	hl, de
	ld	(ix+6), hl		; rest->size = b->size - n - 10
	ld	(ix+9), 0		; rest->used = 0
	ld	hl, (ix+0)
	ld	de, 0
	or	a, a
	sbc	hl, de
	jr	z, .malloc_last
	inc	hl
	inc	hl
	inc	hl
	push	ix
	pop	de
	ld	(hl), de		; rest->next->prev = rest
.malloc_last:
	ld	(iy+0), ix		; b->next = rest
	ld	(iy+6), bc		; b->size = n
	; The rest in b's place in the free list: its links b's.
	ld	hl, (iy+13)		; before
	ld	de, (iy+10)		; after
	ld	(ix+13), hl
	ld	(ix+10), de
	push	de
	ld	de, 0
	or	a, a
	sbc	hl, de
	jr	z, .malloc_front
	ld	de, 10
	add	hl, de
	push	ix
	pop	de
	ld	(hl), de		; before's next = rest
	jr	.malloc_after
.malloc_front:
	ld	(_acc_free_list), ix
.malloc_after:
	pop	hl			; after
	ld	de, 0
	or	a, a
	sbc	hl, de
	jr	z, .malloc_put
	ld	de, 13
	add	hl, de
	push	ix
	pop	de
	ld	(hl), de		; after's prev = rest
.malloc_put:
	pop	ix
	jr	.malloc_used

.malloc_whole:
	push	iy
	call	_acc_free_take
	pop	iy
.malloc_used:
	ld	(iy+9), 1		; b->used = 1
	lea	hl, iy+10		; b + 1
	ret
