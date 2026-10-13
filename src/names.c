/*
 * Names: every identifier and every macro's name, interned once in an
 * arena and found again through a hash table, with the flags a name
 * carries -- a macro, a wide prefix, weak.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "lex_int.h"

/* ------------------------------------------------------------------ */
/* the name arena                                                      */

/* One block of characters holding every identifier in the program, each one
 * exactly once, NUL terminated. Identifiers are referred to by their offset
 * into it, so a name costs three bytes wherever it is mentioned. In front
 * of each name's text are four bytes: the file-scope symbol it stands for,
 * in the three next to the text (see name_global), and before those whether
 * it is a macro (see name_is_macro).
 *
 * Nothing is ever freed. A compiler runs once and exits; a free list would be
 * bytes spent to give memory back to a process that is about to end.
 *
 * Offset zero is never a name, so it can mean "none". */
char         *name_arena;       /* name_global reads it directly */

/* Open addressing over the arena: each slot is an offset, and a collision
 * walks forward. Sized as a power of two so the modulo is a mask, and grown by
 * doubling so the transient peak stays near 1.5x rather than 2x -- the
 * difference matters on a machine with no virtual memory. */
/* A slot is four bytes and not three, so that a slot's offset is a byte
 * pattern that can be assembled without arithmetic: see home, below. A
 * three-byte slot would make it a multiply, `call __imulu`. The probe runs for
 * every identifier in the program, which is what the compiler does most.
 *
 * An empty slot holds NAME_NONE. */
typedef union {
    NameRef       ref;
    unsigned char size[4];      /* four bytes on the host too, so the hash
                                 * test measures the table the Agon has */
} Bucket;

static Bucket  *buckets, *buckets_end;
static unsigned nbuckets, nnames;
static unsigned names_room;     /* names that fit before the table must grow */

/* Where a name's probe starts, as a byte offset into the table: a slot
 * number times the slot's size, masked to the table's size. It is assembled a byte at a
 * time, in memory, because each way of computing it in a register is a helper
 * call -- `high << 8` is `call __ishl`, masking a 24-bit value is `call
 * __iand`, and the slot times four is `call __ishl` again -- and those three
 * ran for every identifier in the program. Byte-wise they are an 8-bit `and`
 * and a store each, and the load that reads the offset back is one 24-bit
 * load. It has to go through memory: assembled in a local, the compiler
 * recognises the pattern and puts the shifts back.
 *
 * The bytes above the second stay zero, which caps the table at 64 KB: 16384
 * slots, and so 8191 distinct names in a program on the Agon. */
static unsigned char home[sizeof(unsigned)];
static unsigned char mask_lo, mask_hi;

#ifdef ACC_HASH_STATS
/* Counts probes so a test can tell a hash that spreads from one that does
 * not -- both give correct answers, and only one of them is fast. Compiled
 * only when the test asks for it: in the compiler this would be an increment
 * on the hottest loop there is. */
unsigned long name_probes, name_rehashes;
#define PROBE() (name_probes++)
#define REHASHED() (name_rehashes++)
#else
#define PROBE() ((void) 0)
#define REHASHED() ((void) 0)
#endif

/* On the Agon, src/hash.s -- only when agondev's clang is the compiler:
 * acc builds these sources for the Agon too, and takes no assembly, so it
 * keeps the C (test/selfbuild.sh). */
#if defined(AGONDEV) && defined(__clang__)
unsigned name_hash(const char *text, unsigned n);
#else
#include "hash.h"
#define name_hash name_hash_c
#endif

/* Returns the bucket a name's probe starts at. */
static inline __attribute__((always_inline))
Bucket *name_home(const char *text, unsigned n)
{
    /* The two bytes read out of memory, as the offset is put together:
     * a shift or a mask of a 24-bit value is a helper call here. */
    static unsigned hash;
    unsigned offset;

    hash = name_hash(text, n);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    offset = ((hash >> 8 & mask_hi) << 8) | (hash & mask_lo);
#else
    home[0] = ((unsigned char *) &hash)[0] & mask_lo;
    home[1] = ((unsigned char *) &hash)[1] & mask_hi;
    memcpy(&offset, home, sizeof offset);
#endif

    return (Bucket *) ((char *) buckets + offset);
}

/* Where names go: chunks that never move, so that the arena never has to
 * grow as one block.
 *
 * It was one block doubled by realloc, and on the Agon agondev's realloc
 * mallocs the new block, copies and frees the old: an arena of 36 KB --
 * what the headers of one of aed's files come to -- was a 64 KB block by
 * then, and asked for 96 KB at once to get there. Now each name goes in the
 * current chunk, and a name that does not fit starts a new one, so the
 * arena costs what is in it and the rest of a chunk.
 *
 * A name is still an offset from name_arena, which stays where it is: on
 * the Agon it is the first chunk, a static one, below every chunk malloc
 * hands out afterwards, so every offset is positive; on the host it is one
 * block reserved at the start, which the chunks are cut from in order.
 * Either way the first chunk has the lowest offsets, and the keywords --
 * interned first -- are the names below kw_limit, as they always were. */
#define NAME_CHUNK 1024

static char *chunk_at, *chunk_end;      /* the current chunk's free part */

#ifdef AGONDEV
static char name_first[NAME_CHUNK];
#else
#define NAME_RESERVE (64L * 1024 * 1024)
static char *reserve_at;
#endif

__attribute__((noinline))
static void names_chunk(size_t need)
{
    size_t size = need > NAME_CHUNK ? need : NAME_CHUNK;

#ifdef AGONDEV
    chunk_at = malloc(size);
    if (!chunk_at)
        acc_error("out of memory for names");
#else
    if (reserve_at + size > name_arena + NAME_RESERVE)
        acc_error("more names than the host build reserved room for");
    chunk_at = reserve_at;
    reserve_at += size;
#endif
    chunk_end = chunk_at + size;
}

static void buckets_rehash(unsigned newn)
{
    Bucket *old = buckets, *end = buckets_end, *from;
    unsigned mask = newn * sizeof *buckets - 1;

    if (newn * sizeof *buckets > 65536)
        acc_error("too many names: the limit is %u",
                  65536 / sizeof *buckets / 4 * 3 - 1);
    REHASHED();
    buckets = calloc(newn, sizeof *buckets);
    if (!buckets)
        acc_error("out of memory for the name table");
    buckets_end = buckets + newn;
    nbuckets = newn;
    names_room = newn / 4 * 3 - 1 - nnames;
    mask_lo = (unsigned char) (mask & ~(sizeof *buckets - 1));
    mask_hi = (unsigned char) (mask >> 8);
    for (from = old; from != end; from++) {
        NameRef ref = from->ref;
        Bucket *b;

        if (ref == NAME_NONE)
            continue;
        b = name_home(name_arena + ref, (unsigned) strlen(name_arena + ref));
        while (b->ref != NAME_NONE)
            if (++b == buckets_end)
                b = buckets;
        b->ref = ref;
    }
    free(old);
}

void name_init(int compiling)
{
#ifdef AGONDEV
    name_arena = name_first;
#else
    name_arena = malloc(NAME_RESERVE);
    if (!name_arena)
        acc_error("out of memory for names");
    reserve_at = name_arena + NAME_CHUNK;
#endif
    name_arena[0] = '\0';     /* so offset 0 is never a real name */
    chunk_at = name_arena + 1;
    chunk_end = name_arena + NAME_CHUNK;

    /* 4096 slots, which hold 3071 names before the table grows: an Agon
     * program's headers come to two or three thousand, and a rehash hashes
     * every name again -- starting at 1024, the two a program with
     * <agon/vdp.h> needed were 8% of its compile, and each held the old
     * table and the new at once. Sixteen KB, which a program that never
     * needed them can spare. A link or an archive reads no headers, and its
 * names are what its objects define and want: 1024, whose clearing is a
 * quarter of what 4096's was in a link of a hello world. */
    buckets_rehash(compiling ? 4096 : 1024);
}

/* The table that finds a name from its text, let go of: an object's
 * writer, which is all that follows a compile to one, reads names by their
 * offset and interns none. 16 KB of an Agon program's headers. Nothing may
 * intern a name after this. */
void name_table_free(void)
{
    free(buckets);
    buckets = buckets_end = NULL;
    nbuckets = 0;
    names_room = 0;
}

NameRef name_intern(const char *text, int len)
{
    Bucket *b = name_home(text, (unsigned) len);
    NameRef ref;

    PROBE();
    while ((ref = b->ref) != NAME_NONE) {
        /* Compare, then check the terminator, rather than measure first.
         * strlen walks the stored name to its end before the compare walks it
         * again, and it walked it even when the first character already said
         * the two were different. This runs 1.5 times per identifier in the
         * program.
         *
         * strncmp and not memcmp, which reads all len bytes of the stored
         * name whether or not it ends first -- off the end of the arena when
         * it is the last name there. strncmp stops at its terminator, so the
         * check after it only runs on a name at least len long. */
        if (strncmp(name_arena + ref, text, len) == 0
            && name_arena[ref + len] == '\0')
            return ref;
        if (++b == buckets_end)
            b = buckets;
        PROBE();
    }

    /* Kept under three quarters full. A linear probe walks further past
     * half, but a table of four-byte slots at half full for the headers of
     * an Agon program was 32 KB, and growing it held that and the 16 KB
     * before it at once; the compile measured no slower. Checked here, where
     * the name is new, rather than on every lookup: the growth moves every
     * name, so the probe starts again. */
    if (names_room == 0) {
        buckets_rehash(nbuckets * 2);

        return name_intern(text, len);
    }

    /* Not a macro and no file-scope symbol yet, then the text. */
    if ((size_t) (chunk_end - chunk_at) < 4 + (size_t) len + 1)
        names_chunk(4 + (size_t) len + 1);
    memset(chunk_at, 0, 4);
    ref = (NameRef) (chunk_at + 4 - name_arena);
    memcpy(name_arena + ref, text, len);
    name_arena[ref + len] = '\0';
    chunk_at += 4 + len + 1;
    b->ref = ref;
    nnames++;
    names_room--;

    return ref;
}

const char *name_text(NameRef ref)
{
    return name_arena + ref;
}
#define NAME_WEAK   4       /* #pragma weak, or a weak reference a link read */
#define NAME_STRONG 8       /* and a reference that was not weak, which ends it */

int name_weak(NameRef ref)
{
    return (name_is_macro(ref) & NAME_WEAK) != 0;
}

/* That the library a link is reading has no member defining the name: so
 * that the link asks it once, and not again on every pass it makes through
 * the library. Taken off each name when the link is done with it. */
#define NAME_MISSED 16

int name_missed(NameRef ref)
{
    return (name_is_macro(ref) & NAME_MISSED) != 0;
}

void name_set_missed(NameRef ref, int missed)
{
    if (missed)
        name_is_macro(ref) |= NAME_MISSED;
    else
        name_is_macro(ref) = (char) (name_is_macro(ref) & ~NAME_MISSED);
}

/* Weak is a name every reference to which is weak, whichever order the
 * references come in: once one that is not has been seen, it never is
 * again. `weak` 1 is a weak reference, 0 one that is not. */
void name_set_weak(NameRef ref, int weak)
{
    if (!weak)
        name_is_macro(ref) = (char) ((name_is_macro(ref) & ~NAME_WEAK)
                                     | NAME_STRONG);
    else if (!(name_is_macro(ref) & NAME_STRONG))
        name_is_macro(ref) |= NAME_WEAK;
}
