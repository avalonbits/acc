;
; The long routines that take registers, made callable from C for
; test/rtlong.sh: each t_... loads E:UHL and A:UBC from the arguments a C
; call leaves on the stack -- a long's low three bytes and then its top
; one, a slot each -- with D and IY set to what the routine must keep, and
; after it records BC, D, IY and the flags for check.c to look at. A long
; comes back in E:UHL, as acc and agondev answer one.
;

	XREF	_t_iy
	XREF	_t_bc
	XREF	_t_d
	XREF	_t_flags

	.assume adl=1
	SEGMENT CODE

; e:hl op a:bc, both longs.
	MACRO	T_BIN routine
	push	iy
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy + 6)
	ld	e, (iy + 9)
	ld	bc, (iy + 12)
	ld	a, (iy + 15)
	ld	d, 0x5a
	ld	iy, 0x123456
	call	routine
	call	t_after
	pop	iy
	ret
	ENDMACRO

; e:hl op a, a long and a count.
	MACRO	T_SHIFT routine
	push	iy
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy + 6)
	ld	e, (iy + 9)
	ld	a, (iy + 12)
	ld	bc, 0x654321
	ld	d, 0x5a
	ld	iy, 0x123456
	call	routine
	call	t_after
	pop	iy
	ret
	ENDMACRO

; op e:hl, a long alone.
	MACRO	T_ONE routine
	push	iy
	ld	iy, 0
	add	iy, sp
	ld	hl, (iy + 6)
	ld	e, (iy + 9)
	ld	a, 0xa5
	ld	bc, 0x654321
	ld	d, 0x5a
	ld	iy, 0x123456
	call	routine
	call	t_after
	pop	iy
	ret
	ENDMACRO

	XDEF	_t_add
	XREF	_acc_rt_lradd
_t_add:
	T_BIN	_acc_rt_lradd
	XDEF	_t_sub
	XREF	_acc_rt_lrsub
_t_sub:
	T_BIN	_acc_rt_lrsub
	XDEF	_t_and
	XREF	_acc_rt_lrand
_t_and:
	T_BIN	_acc_rt_lrand
	XDEF	_t_or
	XREF	_acc_rt_lror
_t_or:
	T_BIN	_acc_rt_lror
	XDEF	_t_xor
	XREF	_acc_rt_lrxor
_t_xor:
	T_BIN	_acc_rt_lrxor
	XDEF	_t_mul
	XREF	_acc_rt_lrmul
_t_mul:
	T_BIN	_acc_rt_lrmul
	XDEF	_t_divu
	XREF	_acc_rt_lrdivu
_t_divu:
	T_BIN	_acc_rt_lrdivu
	XDEF	_t_divs
	XREF	_acc_rt_lrdivs
_t_divs:
	T_BIN	_acc_rt_lrdivs
	XDEF	_t_remu
	XREF	_acc_rt_lrremu
_t_remu:
	T_BIN	_acc_rt_lrremu
	XDEF	_t_rems
	XREF	_acc_rt_lrrems
_t_rems:
	T_BIN	_acc_rt_lrrems
	XDEF	_t_cmpu
	XREF	_acc_rt_lrcmpu
_t_cmpu:
	T_BIN	_acc_rt_lrcmpu
	XDEF	_t_cmps
	XREF	_acc_rt_lrcmps
_t_cmps:
	T_BIN	_acc_rt_lrcmps
	XDEF	_t_shl
	XREF	_acc_rt_lrshl
_t_shl:
	T_SHIFT	_acc_rt_lrshl
	XDEF	_t_shru
	XREF	_acc_rt_lrshru
_t_shru:
	T_SHIFT	_acc_rt_lrshru
	XDEF	_t_shrs
	XREF	_acc_rt_lrshrs
_t_shrs:
	T_SHIFT	_acc_rt_lrshrs
	XDEF	_t_neg
	XREF	_acc_rt_lrneg
_t_neg:
	T_ONE	_acc_rt_lrneg
	XDEF	_t_not
	XREF	_acc_rt_lrnot
_t_not:
	T_ONE	_acc_rt_lrnot

; What the routine left: the flags as 1 for carry and 2 for zero, BC, D and
; IY, each where check.c reads it; E:UHL as it was.
t_after:
	push	af
	ld	(_t_iy), iy
	ld	(_t_bc), bc
	ld	a, d
	ld	(_t_d), a
	pop	af
	push	hl
	ld	hl, 0
	jr	nc, .t_nc
	inc	hl
.t_nc:
	jr	nz, .t_nz
	inc	hl
	inc	hl
.t_nz:
	ld	a, l
	ld	(_t_flags), a
	pop	hl
	ret
