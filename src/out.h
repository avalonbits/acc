/*
 * The output image and its relocations. See image.c and reloc.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_OUT_H
#define ACC_OUT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gen.h"

void out_open(const char *path, int header);
void out_close(void);
void out_free(void);                /* let it go without writing it */
void out_flush(void);               /* the image, into its file so far */
void out_abandon(void);             /* and taken out again, on an error */
extern int out_may_flush;           /* whether out_flush may: see src/image.c */
#define OUT_FLUSH_IN_PLACE 1        /* a link: the file is the output */
#define OUT_FLUSH_COPY     2        /* a compile: copied out at the end */
extern int out_flushed;             /* how much of the image is in it */
void out_resident(int at);          /* from `at` on, in memory again */
int  out_slot_get(int off);         /* a slot in the file, for cut_out */
void out_slot_put(int off, int value);
void out_cut_prepare(void);         /* before the file's addresses move */

/* An addition the image's file is to get: `value` added to the three bytes
 * at `at`. gen_finish hands its fixups over as these, in the order of
 * their slots, and the image applies them as it sweeps the file at the end
 * rather than keeping a patch for each. */
typedef struct {
    int value, at;
} OutAdd;
void out_add_later(const OutAdd *(*add_at)(int k), int n);
void out_add_later2(const OutAdd *(*add_at)(int k), int n);

/* And one value added to each of the n slots `slot_at` names, in order:
 * the bss's start, to the slots that hold an offset into it. */
void out_add_base_later(int (*slot_at)(int k), int n, int value);
void out_write_text(void *to);      /* the image, file and memory: a FILE */
void out_forget(void);          /* a function's cut runs, given back */
int  out_len(void);                 /* bytes written so far */

/* Where the image is loaded. Set before out_open and not after: every address
 * the compiler writes is absolute and is worked out from this. */
extern int out_base;

/* The three bytes at `at` are an address inside the image, and would have to
 * change if it were loaded somewhere else. Called where the slot is emitted
 * rather than where it is filled in, so the offsets come out in order and a
 * rewind can drop the ones it undoes.
 *
 * Inlined, and the table's three pointers visible for it, for the reason the
 * byte emitters are: see src/image.c. */
extern int *out_relocs, *out_reloc_put, *out_reloc_limit;
void out_reloc_grow(void);
void out_reloc_back(int at);
void out_reloc_merge(const int *add, int n, int first);

static inline __attribute__((always_inline))
void out_reloc(int at)
{
    if (out_reloc_put == out_reloc_limit)
        out_reloc_grow();
    at -= out_base;
    /* Unsigned, and reading a slot that is always there: an offset is never
     * negative, so the two compare the same either way, and a signed one is
     * a helper call on this target. The table starts with a sentinel so that
     * the first relocation has something behind it to compare with -- see
     * src/reloc.c. */
    if ((unsigned) out_reloc_put[-1] > (unsigned) at) {
        out_reloc_back(at);

        return;
    }
    *out_reloc_put++ = at;
}

int  out_nrelocs(void);
int  out_reloc_at(int i);

/* A run of bytes taken out of the image: see out_cut. Two passes take runs
 * out -- the one that leaves out the functions a file does not use, and the
 * one that shortens a jump whose target is near -- and both hand the runs to
 * out_cut_sum first, which works out what each one moves everything after it
 * by and keeps that beside them. */
typedef struct {
    int at, len;
} Cut;

void out_cut_sum(const Cut *cuts, int n);   /* before out_cut_moved or out_cut */
void out_cut(const Cut *cuts, int n, int first);
int  out_cut_moved(int a);          /* an address, wherever it is */
void out_cut_rewind(void);          /* and a run of them in rising order */
int  out_cut_next(int a);
void out_relocs_write(const char *path);

/* The output's write position and its end, and the growth that happens a
 * dozen times a compile. Visible so that the three emitters below can be
 * inlined: every byte the compiler emits goes through one of them, and as
 * calls each opened a frame on this target to do a compare and a store. */
extern unsigned char *out_put, *out_limit;
void out_grow(void);
extern unsigned out_rewinds;        /* see out_rewind */
extern int out_rewind_floor;        /* see out_rewind */
extern int out_rewound_to;          /* and ld_rr_ix */
extern void (*out_on_rewind)(int here); /* told of each rewind */
int  out_capacity(void);            /* the image's room, for test_out */
extern int out_start_cap;           /* and where it starts */

/* The first byte of the image, and the address the next one will have.
 *
 * out_here is asked wherever the compiler needs to know where it is -- every
 * jump, every function, every global, and every relocation recorded beside
 * one of those -- and as a call it opened a frame to do a subtraction and an
 * add. */
extern unsigned char *out_img;

static inline __attribute__((always_inline))
int out_here(void)
{
    return out_base + (int) (out_put - out_img);
}

/* The three bytes of a 24-bit value, lowest first, at `at`.
 *
 * Copied rather than shifted out. `value >> 8` and `value >> 16` are each a
 * call into the runtime on this target -- there is no instruction that shifts
 * a 24-bit register by more than one place -- and this runs for every call,
 * jump and constant the compiler emits. The value is already those three
 * bytes, lowest first, in memory, so copying them is the whole job. On a host
 * that keeps its bytes the other way round it is not, and the shifts are used
 * there instead. */
static inline __attribute__((always_inline))
void put24(unsigned char *at, int value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    at[0] = (unsigned char) value;
    at[1] = (unsigned char) (value >> 8);
    at[2] = (unsigned char) (value >> 16);
#else
    memcpy(at, &value, 3);
#endif
}

/* The 24-bit value put24 stored at `at`, for the same reason and in the same
 * way: one load, where assembling it from bytes would be shifts. Values are
 * non-negative. */
static inline __attribute__((always_inline))
int get24(const unsigned char *at)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return at[0] | (at[1] << 8) | (at[2] << 16);
#else
    int value = 0;

    memcpy(&value, at, 3);

    return value;
#endif
}

/* A name's file-scope symbol, or SYM_NONE: see sym.c, which keeps it in
 * the three bytes in front of the name's text, under its locals. */
int name_global(NameRef ref);

/* An opcode and the 24-bit operand that follows it, which is the shape of a
 * call, a jump and every load of a constant. Four bytes, one bounds check.
 *
 * The room left is compared unsigned, as in the two below: it is never
 * negative, and a signed compare is a helper call on this target, in front
 * of every byte the compiler emits. */
static inline __attribute__((always_inline))
void out_opcode24(int opcode, int value)
{
    if ((unsigned) (out_limit - out_put) < 4)
        out_grow();
    out_put[0] = (unsigned char) opcode;
    put24(out_put + 1, value);
    out_put += 4;
}

static inline __attribute__((always_inline)) void out_byte(int byte)
{
    if (out_put == out_limit)
        out_grow();
    *out_put++ = (unsigned char) byte;
}

static inline __attribute__((always_inline)) void out_byte2(int first, int second)
{
    if ((unsigned) (out_limit - out_put) < 2)
        out_grow();
    out_put[0] = (unsigned char) first;
    out_put[1] = (unsigned char) second;
    out_put += 2;
}

static inline __attribute__((always_inline)) void out_byte3(int first, int second, int third)
{
    if ((unsigned) (out_limit - out_put) < 3)
        out_grow();
    out_put[0] = (unsigned char) first;
    out_put[1] = (unsigned char) second;
    out_put[2] = (unsigned char) third;
    out_put += 3;
}
void out_word24(int v);
void out_rewind(int here);          /* forget what came after out_here() was here */
void out_seek(int here);            /* back to it, keeping what came after */
void out_copy(int at, unsigned char *to, int len);  /* bytes already written */
int  out_read24(int at);            /* and read one back */
void out_patch24(int at, int v);
void out_add24(int at, int delta);  /* added to what is there */
void out_patch8(int at, int v);
void out_patch16(int at, int v);

#endif
