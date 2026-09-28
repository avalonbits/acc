/*
 * The end of a file and the start of a program: calls and addresses still
 * waiting on a name, the variables that start at zero, argc and argv, the
 * slots a link fills, gen_finish, and the entry stub.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "rt_helpers.h"
#include "gen_int.h"

/* ------------------------------------------------------------------ */
/* functions                                                           */

Fixup   **fixup_blocks;
int       nfixups;
static int nfixup_blocks, fixup_blocks_cap;

/* A new block of `bytes` at the end of the list of them at *dir, of *n
 * blocks with room for *cap: what the fixups and the bss's slots are kept
 * in (fixup_at, bss_fixup). */
static void *block_new(void ***dir, int *n, int *cap, size_t bytes)
{
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *dir = realloc(*dir, (size_t) *cap * sizeof **dir);
        if (!*dir)
            acc_error("out of memory for forward calls");
    }
    (*dir)[*n] = malloc(bytes);
    if (!(*dir)[*n])
        acc_error("out of memory for forward calls");

    return (*dir)[(*n)++];
}

/* Where the next goes, while nfixups is what it was when it was worked
 * out: a rollback or a compaction changes nfixups, and it is found again. */
static Fixup *fixup_put;
static int    fixup_put_at = -1;

Fixup *fixup_at(int i)
{
    unsigned b = (unsigned) i >> 8;

    return b < (unsigned) nfixup_blocks ? fixup_blocks[b] + (unsigned char) i
                                        : NULL;
}

/* A slot to fill with `fn`'s address once it has one. */
void fixup_add(int fn, int at)
{
    want(fn);
    /* A block for this one when the last is full, kept from before when a
     * rollback left it behind. */
    if (!(unsigned char) nfixups && ((unsigned) nfixups >> 8) == (unsigned) nfixup_blocks)
        block_new((void ***) &fixup_blocks, &nfixup_blocks, &fixup_blocks_cap,
                  FIXUP_BLOCK * sizeof **fixup_blocks);
    out_reloc(at);
    if (fixup_put_at != nfixups || !(unsigned char) nfixups)
        fixup_put = fixup_at(nfixups);
    fixup_put->fn = fn;
    fixup_put->at = at;
    fixup_put++;
    fixup_put_at = ++nfixups;
}

/* The uses of `sym` so far, filled in now that it has an address rather
 * than at the end of the file: a block's static, used in its own
 * initialiser, is dropped with the rest of the block's names when its
 * function ends, and gen_finish would find nothing there. The relocation
 * each slot needs was recorded when the use was. */
void gen_settle(int sym)
{
    int i, kept = 0;
    Fixup *f = fixup_at(0), *k = f;

    for (i = 0; i != nfixups; i++, f = FIXUP_STEP(f, i)) {
        if (f->fn != sym) {
            *k = *f;
            kept++;
            k = FIXUP_STEP(k, kept);
            continue;
        }
        out_add24(f->at, sym_at(sym)->val);
    }
    nfixups = kept;
}

/* Calls this file cannot resolve, when it is being compiled to an object:
 * the function is defined somewhere else, and where the call has to point is
 * the linker's to work out. The slot keeps the zero it was emitted with,
 * which is the addend, and the relocation names the symbol.
 *
 * Kept apart from the relocation table rather than in it, because image.c and reloc.c do
 * not know about symbols and should not have to. The object writer puts the
 * two together. */
int gen_objects;                /* -c: compiling to an object */

typedef struct {
    int at, fn;
} ExternFix;

static ExternFix *externs, *externs_put, *externs_limit;
static ExternFix *externs_helpers_end;  /* the first run: see gen_finish */
static int        nexterns;

/* The table as `bytes` of room, what is in it kept. Counted in bytes and
 * walked by a pointer, as the relocations are: an index into six-byte
 * entries is a multiply, which is a call into the runtime at every site. */
static void externs_room(size_t bytes)
{
    size_t used = (size_t) ((char *) externs_put - (char *) externs);

    externs = realloc(externs, bytes);
    if (!externs)
        acc_error("out of memory for the calls out of this file");
    externs_put = (ExternFix *) (void *) ((char *) externs + used);
    externs_limit = (ExternFix *) (void *) ((char *) externs + bytes);
}

/* Room for `n` in all, at once. What goes in at the end of a compile to an
 * object -- one for each call and address still waiting on a name, and one
 * for each call to a helper -- is counted before it is added, and one block
 * the right size is taken: doubling towards it held the table twice over,
 * which on the Agon was what an Agon program's largest file ran out on. */
static void extern_reserve(int n)
{
    size_t bytes = (size_t) n * sizeof *externs;

    if (bytes > (size_t) ((char *) externs_limit - (char *) externs))
        externs_room(bytes);
}

static void extern_add(int at, int fn)
{
    if (externs_put == externs_limit) {
        size_t used = (size_t) ((char *) externs_put - (char *) externs);

        externs_room(used ? used + used : 16 * sizeof *externs);
    }
    externs_put->at = at;
    externs_put->fn = fn;
    externs_put++;
    nexterns++;
}

int gen_nexterns(void)
{
    return nexterns;
}

/* The entries from `from` to `to` in the order of their slots. They nearly
 * are already -- they were recorded as the code was written -- but an
 * initialiser with designators writes a global's bytes out of order, and
 * the addresses in them with it. So an insertion sort, which costs a
 * compare an entry when nothing is out of place, and needs no room. */
static void externs_sort(ExternFix *from, ExternFix *to)
{
    ExternFix *e;

    for (e = from; e != to; e++) {
        ExternFix x = *e, *p = e;

        while (p != from && (unsigned) p[-1].at > (unsigned) x.at) {
            *p = p[-1];
            p--;
        }
        *p = x;
    }
}

/* How many of the calls out are the helpers', which come first: the two
 * runs are each in the order of their slots (see externs_sort), and the
 * object writer walks them that way. */
int gen_externs_helpers(void)
{
    return (int) ((char *) externs_helpers_end - (char *) externs)
           / (int) sizeof *externs;
}

int gen_extern_at(int i)
{
    return externs[i].at;
}

int gen_extern_sym(int i)
{
    return externs[i].fn;
}

/* ------------------------------------------------------------------ */
/* what starts at zero                                                 */

/* A variable C says starts at zero takes no room in the file. It is given an
 * address past the image's last byte instead, and the program clears what is
 * there before main runs. `int big[1000];` was three thousand bytes of zeros
 * to read off the card; now it is three thousand bytes of nothing.
 *
 * Where the bss starts is not known until the whole image has been written,
 * but where in it each variable goes is known as it is declared. So a use of
 * one is its offset, which folds like any other constant -- `a[3]` is one
 * load with the nine already in it -- and the slot is written down for the
 * start of the bss to be added to it at the end. One number, the same for
 * all of them, which is why these are a list of places and not fixups
 * against a symbol.
 *
 * That is also what lets a block's `static` live here. It has no name
 * outside its function and its symbol is dropped at the end of it, so there
 * would be nothing to hang a symbol on -- and it needs none.
 *
 * File-scope ones are written down as symbols as well, because another
 * object may want to name them. */
typedef struct {
    int sym, at;
} BssSym;

static BssSym *bss_syms;
static int     nbss_syms, bss_syms_cap;
static int     bss_len;
static int     bss_init_hole = -1;      /* the stub's call to the clearing */
static int     args_hole = -1;          /* and the one to the arguments */
static int     bss_extra;               /* room past it that is not cleared */
static const char *exec_name;           /* what is being built, for argv[0] */
static int     bss_top;                 /* the first byte past everything */

/* Slots holding an offset into the bss, which want the start of it added.
 *
 * The same shape as the calls into the runtime blob: a list of places,
 * settled in one pass once there is an address to settle them with. Not
 * fixups against a symbol, because there is no symbol -- every one of them
 * wants the same one number, and a block's `static` has no name outside the
 * function it is in to hang a symbol on. */
int      **bss_blocks;
int        nbss_fixups;
static int nbss_blocks, bss_blocks_cap;
static int *bss_put, bss_put_at = -1;  /* as fixup_put */

int *bss_fixup(int i)
{
    unsigned b = (unsigned) i >> 8;

    return b < (unsigned) nbss_blocks ? bss_blocks[b] + (unsigned char) i
                                      : NULL;
}

/* Kept in order of where they are, as the relocation table is and for the
 * same reason: the object writer walks the two together to say which of the
 * slots it is writing down want the bss rather than the image. They nearly
 * always arrive in order -- a slot is recorded where it is emitted -- and a
 * global's initial values are the exception, since a designated one is
 * written out of order. */
void gen_bss_fixup(int at)
{
    int i;

    if (!(unsigned char) nbss_fixups
        && ((unsigned) nbss_fixups >> 8) == (unsigned) nbss_blocks)
        block_new((void ***) &bss_blocks, &nbss_blocks, &bss_blocks_cap,
                  256 * sizeof **bss_blocks);
    out_reloc(at);
    if (bss_put_at != nbss_fixups || !(unsigned char) nbss_fixups)
        bss_put = bss_fixup(nbss_fixups);
    /* In order, nearly always: the slot goes on the end. */
    if (!nbss_fixups
        || (unsigned) ((unsigned char) nbss_fixups ? bss_put[-1]
                                                   : *bss_fixup(nbss_fixups - 1))
           <= (unsigned) at) {
        *bss_put++ = at;
        bss_put_at = ++nbss_fixups;

        return;
    }
    for (i = nbss_fixups; i && (unsigned) *bss_fixup(i - 1) > (unsigned) at; i--)
        *bss_fixup(i) = *bss_fixup(i - 1);
    *bss_fixup(i) = at;
    nbss_fixups++;
}

int gen_nbss_fixups(void)
{
    return nbss_fixups;
}

int gen_bss_fixup_at(int i)
{
    return *bss_fixup(i);
}

/* Room in the bss, and where in it. The linker asks for a whole object's
 * worth at once and hands out the pieces itself. */
/* The largest alignment anything in the bss asked for, as a log2: where
 * the bss starts is rounded up to it. An assembly object may ask for a
 * page-aligned buffer; nothing acc compiles does. */
static int bss_align;
static int bss_start;                   /* where bss_emit put it */

int gen_bss_reserve_aligned(int bytes, int align)
{
    int step = 1 << align;

    if (align > bss_align)
        bss_align = align;
    bss_len = (bss_len + step - 1) & ~(step - 1);

    return gen_bss_reserve(bytes);
}

int gen_bss_reserve(int bytes)
{
    int at = bss_len;

    if (bytes < 0 || bss_len + bytes < bss_len)
        acc_error("internal: %d bytes of bss", bytes);
    bss_len += bytes;

    return at;
}

void gen_bss_symbol(int sym, int at)
{
    if (nbss_syms == bss_syms_cap) {
        bss_syms_cap = bss_syms_cap ? bss_syms_cap * 2 : 16;
        bss_syms = realloc(bss_syms, (size_t) bss_syms_cap * sizeof *bss_syms);
        if (!bss_syms)
            acc_error("out of memory for the variables that start at zero");
    }
    bss_syms[nbss_syms].sym = sym;
    bss_syms[nbss_syms].at = at;
    nbss_syms++;
}

/* Where in the bss a symbol is, or -1 when it is not there. Asked by the
 * object writer, of every symbol it exports, so that the object can say
 * which of them have room in the file and which want it cleared. */
int gen_bss_offset(int sym)
{
    int i;

    for (i = 0; i != nbss_syms; i++)
        if (bss_syms[i].sym == sym)
            return bss_syms[i].at;

    return -1;
}

int gen_bss_len(void)
{
    return bss_len;
}

/* Whether anything has already been compiled that reaches into [at, at +
 * bytes] of the bss.
 *
 * Asked when a variable that was given room there turns out, further down
 * the file, to have a value after all: its bytes then go in the file and the
 * room here is abandoned, which is fine -- unless something in between was
 * compiled to look in the room. The range is closed at both ends on purpose:
 * the address one past the end of an array is a real thing to have taken,
 * and it is not worth being clever about whose it is. */
/* A variable that was in the bss, at `at` for `bytes`, and has been given
 * room in the image at `to` instead -- `int n;`, used, and then `int n =
 * 30;`. Every slot written so far with an offset into its bytes, or just
 * past them, is an address of it: each becomes that address in the image,
 * relocated as the image's are -- which is what a slot not on the bss's
 * list is -- and is no longer one the bss's start is added to. */
void gen_bss_move(int at, int bytes, int to)
{
    int i, kept = 0;

    for (i = 0; i != nbss_fixups; i++) {
        int slot = *bss_fixup(i), was = out_read24(slot);

        if (was < at || was > at + bytes) {
            *bss_fixup(kept++) = slot;
            continue;
        }
        out_patch24(slot, to + was - at);       /* relocated already: see */
    }                                           /* gen_bss_fixup */
    nbss_fixups = kept;
}

/* And that it is no longer there, so that the addresses handed out at the
 * end leave it alone. What it was given stays reserved: a hole in an area
 * that is all zeros anyway. */
void gen_bss_forget(int sym)
{
    int i;

    for (i = 0; i != nbss_syms; i++)
        if (bss_syms[i].sym == sym) {
            bss_syms[i] = bss_syms[--nbss_syms];

            return;
        }
}

/* The routine that clears the bss, and the addresses of everything in it.
 *
 * Last of all, so that what follows the image's last byte is the bss itself
 * -- which is the whole point: the file stops, and the zeros do not have to
 * be in it. The routine is in the file, just before them.
 *
 * It has three shapes, and which one follows from how much there is to
 * clear, so its own length is known before it is written and with it where
 * the bss starts. One byte is the shape that has to be told apart: `ld (hl),
 * 0` has already cleared it, and an ldir of the nothing left over would be
 * an ldir of bc = 0, which on this chip is not nothing but sixteen
 * megabytes -- it cleared the machine out from under the program. */
#define BSS_INIT_ONE  7                 /* ld hl, base / ld (hl), 0 / ret */
#define BSS_INIT_LEN  17                /* and an ldir along the rest */

static void bss_emit(void)
{
    int base, i;

    if (bss_init_hole < 0)
        return;                         /* no entry stub: nothing calls it */

    out_patch24(bss_init_hole, out_here());
    if (!bss_len) {
        out_byte(0xc9);                 /* ret */
        base = out_here();
    } else {
        /* It starts past the routine that clears it, rounded up to the most
         * any of it asked to be aligned to. The bytes between are outside
         * the image: the bss is room, and nothing is written into it. */
        base = out_here() + (bss_len == 1 ? BSS_INIT_ONE : BSS_INIT_LEN);
        base = (base + (1 << bss_align) - 1) & ~((1 << bss_align) - 1);
        out_byte(0x21);                         /* ld hl, base */
        out_reloc(out_here());
        out_word24(base);
        out_byte2(0x36, 0x00);                  /* ld (hl), 0 */
        if (bss_len > 1) {
            out_byte(0x11);                     /* ld de, base + 1 */
            out_reloc(out_here());
            out_word24(base + 1);
            out_byte(0x01);                     /* ld bc, bss_len - 1 */
            out_word24(bss_len - 1);
            out_byte2(0xed, 0xb0);              /* ldir */
        }
        out_byte(0xc9);                         /* ret */

        for (i = 0; i != nbss_syms; i++) {
            sym_at(bss_syms[i].sym)->val = base + bss_syms[i].at;
            sym_set_flags(bss_syms[i].sym, SYMF_DEFINED);
        }
    }

    if (base - out_base + bss_len + bss_extra > ACC_RAM_BYTES)
        acc_error("the program and what it leaves at zero come to %d bytes, "
                  "and the Agon has %d for both",
                  base - out_base + bss_len + bss_extra, ACC_RAM_BYTES);

    /* Every slot that holds an offset into it, which is where the folding
     * went: `a[3]` put nine there, and this makes it an address. The table
     * the argument routine keeps is one of them, and it is there whether the
     * program left anything at zero or not -- so this runs even when there
     * was nothing to clear. */
    /* Those in the image's file are the first so many, the list being in
     * the order of the slots: the image adds the start to them as it sweeps
     * the file, rather than keeping a patch for each. */
    {
        int *p = bss_fixup(0);

        for (i = 0; i != nbss_fixups
                    && (unsigned) (*p - out_base) < (unsigned) out_flushed;
             i++, p = BSS_STEP(p, i))
            ;
        if (i)
            out_add_base_later(gen_bss_fixup_at, i, base);
        for (; i != nbss_fixups; i++, p = BSS_STEP(p, i))
            out_add24(*p, base);
    }

    bss_top = base + bss_len + bss_extra;
    bss_start = base;
}

/* argc and argv, out of what MOS passed.
 *
 * MOS enters a program with HL pointing at the rest of the command line --
 * everything after the name that was typed, which is why the name is not in
 * it and has to come from somewhere else. The routine below walks that text,
 * writes a zero over each separator so that every word is a string of its
 * own, and fills a table of pointers to them. It is the same walk agondev's
 * startup makes, down to the sixteen-argument limit, because a program that
 * works under one has to work under the other.
 *
 * It is written whether main asks for it or not. Whether it does is not a
 * question the link can answer -- main arrives from an object, and an object
 * says what its symbols are called and not what they take -- and a program
 * that works compiled in one piece and not linked from two would be a worse
 * thing than the ninety bytes. A main that takes no parameters is handed two
 * values it never reads.
 *
 * The bytes come from src/rt/startup.s, as the stub's do. */
#define ARGV_MAX  16            /* as agondev's startup has it */

static const unsigned char args_code[] = {
    0xdd, 0x21, 0x00, 0x00, 0x00,               /* ld ix, argv */
    0x01, 0x00, 0x00, 0x00,                     /* ld bc, the name */
    0xdd, 0x0f, 0x00,                           /* ld (ix+0), bc */
    0xed, 0x32, 0x03,                           /* lea ix, ix+3 */
    0xcd, 0x00, 0x00, 0x00,                     /* call spaces */
    0x0e, 0x01, 0x06, ARGV_MAX,                 /* ld c, 1 / ld b, 16 */
    0xc5, 0xe5,                                 /* next: push bc / push hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call token */
    0x79, 0xd1, 0xc1,                           /* ld a, c / pop de / pop bc */
    0xb7, 0x28, 0x13,                           /* or a, a / jr z, done */
    0xdd, 0x1f, 0x00,                           /* ld (ix+0), de */
    0xe5, 0xd1,                                 /* push hl / pop de */
    0xcd, 0x00, 0x00, 0x00,                     /* call spaces */
    0xaf, 0x12,                                 /* xor a, a / ld (de), a */
    0xed, 0x32, 0x03,                           /* lea ix, ix+3 */
    0x0c, 0x79, 0xb8, 0x38, 0xe1,               /* inc c / cp b / jr c, next */
    0x11, 0x00, 0x00, 0x00, 0x59,               /* done: ld de, 0 / ld e, c */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, argv */
    0xc9,
    0x0e, 0x00,                                 /* token: ld c, 0 */
    0x7e, 0xb7, 0xc8,                           /* ld a, (hl) / ret z */
    0xfe, 0x0d, 0xc8,                           /* cp 13 / ret z */
    0xfe, 0x20, 0xc8,                           /* cp ' ' / ret z */
    0x23, 0x0c, 0x18, 0xf3,                     /* inc hl / inc c / jr */
    0x7e, 0xfe, 0x20, 0xc0,                     /* spaces: ld a,(hl) / ret */
    0x23, 0x18, 0xf9                            /* inc hl / jr */
};

/* Its holes, as offsets from its first byte: the table of pointers, which is
 * in the bss and so wants the bss added; the name, which is in the image;
 * and the table again, where it is answered with. */
#define ARGS_ARGV_AT    0x02
#define ARGS_NAME_AT    0x06
#define ARGS_ARGV2_AT   0x3c

static const struct { int at, to; } args_calls[] = {
    { 0x10, 0x4f },                             /* spaces */
    { 0x1a, 0x40 },                             /* token */
    { 0x29, 0x4f }                              /* spaces */
};

/* The name argv[0] is given. MOS does not pass one -- what was typed is not
 * in the text it hands over -- so this is the name of the file being built,
 * which is the nearest thing to the truth that is known here. agondev puts a
 * fixed string there instead. */
static const char *base_name(const char *path)
{
    const char *at = path, *p;

    for (p = path; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            at = p + 1;

    return at;
}

static void args_emit(void)
{
    const char *name;
    int base, argv_at, name_at, i;

    if (args_hole < 0)
        return;                         /* no entry stub: nothing calls it */

    out_patch24(args_hole, out_here());

    /* The table goes past everything that is cleared rather than among it.
     * Nothing reads a slot the routine has not written -- argc says how many
     * there are -- so zeroing forty-eight bytes at every start would be work
     * for no one, and leaving them out keeps a program whose own bss is one
     * byte a program whose bss is one byte. */
    argv_at = bss_len;
    bss_extra = ARGV_MAX * ACC_INT_SIZE;
    base = out_here();
    for (i = 0; i < (int) sizeof args_code; i++)
        out_byte(args_code[i]);

    name = base_name(exec_name ? exec_name : "");
    name_at = out_here();
    for (i = 0; name[i]; i++)
        out_byte((unsigned char) name[i]);
    out_byte(0);

    out_patch24(base + ARGS_NAME_AT, name_at);
    out_reloc(base + ARGS_NAME_AT);
    out_patch24(base + ARGS_ARGV_AT, argv_at);
    gen_bss_fixup(base + ARGS_ARGV_AT);
    out_patch24(base + ARGS_ARGV2_AT, argv_at);
    gen_bss_fixup(base + ARGS_ARGV2_AT);

    for (i = 0; i < (int) (sizeof args_calls / sizeof *args_calls); i++) {
        out_patch24(base + args_calls[i].at, base + args_calls[i].to);
        out_reloc(base + args_calls[i].at);
    }
}

/* Two names the link answers for, because only it knows them: where the
 * program's own memory ends and where the machine's does. What is between
 * them is nobody's, which is what makes it a heap -- and the stack comes
 * down from above it, so the top is kept clear of where the stack will be.
 *
 * A program that never asks for them costs nothing for their being here:
 * they are only ever looked for among the names nothing has defined.
 *
 * The stub asks for a third, the top of the program's memory, where main's
 * stack starts. That one is known by its symbol rather than its name. */
static int stack_top_sym = -1;   /* made by gen_finish, for the stub */
static int stack_top_at;         /* where in the stub it goes */

static int link_given(int sym)
{
    const char *name;

    if (sym == stack_top_sym)
        return out_base + ACC_RAM_BYTES;
    name = name_text(sym_at(sym)->name);

    if (strcmp(name, "acc_heap_start") == 0)
        return bss_top;
    if (strcmp(name, "acc_heap_end") == 0)
        return out_base + ACC_RAM_BYTES - ACC_STACK_RESERVE;

    return 0;
}

/* A symbol the fixups are waiting on that nothing here has given an address
 * to. A function has one once its body has been read -- asked of the flag
 * and not of the address, because an object puts its first function at
 * offset zero. A variable has one once room has been reserved for it, which
 * a declaration that only says extern does not do: that leaves -1 behind. */
/* A slot of any kind, filled with a value that is known: see the note on
 * relocation kinds in src/obj.c. A PCREL8's value is where it jumps to. */
void gen_slot(int at, int kind, long value)
{
    long d;

    switch (kind) {
    case REL_ABS24:  out_patch24(at, (int) value);                  break;
    case REL_LOW8:   out_patch8(at, (int) (value & 0xff));          break;
    case REL_HIGH8:  out_patch8(at, (int) ((value >> 8) & 0xff));   break;
    case REL_UPPER8: out_patch8(at, (int) ((value >> 16) & 0xff));  break;
    case REL_ABS16:  out_patch16(at, (int) (value & 0xffff));       break;
    default:
        d = value - (at + 1);
        if (d < -128 || d > 127)
            acc_error("a relative jump at %06x reaches %ld bytes, and one "
                      "can reach 127 forward and 128 back", at, d);
        out_patch8(at, (int) (d & 0xff));
        break;
    }
}

/* Slots that want a symbol's address, or the bss's, and are not the three
 * bytes gen_data_fixup fills: the kinds an assembly object has. Filled once
 * everything has an address, at the end of gen_finish. */
typedef struct {
    int  sym, at, kind;
    long addend;
} LateFixup;

static LateFixup *late;
static int        nlate, late_cap;

void gen_late_fixup(int sym, int at, int kind, long addend)
{
    if (nlate == late_cap) {
        late_cap = late_cap ? late_cap * 2 : 16;
        late = realloc(late, (size_t) late_cap * sizeof *late);
        if (!late)
            acc_error("out of memory for the link's slots");
    }
    late[nlate].sym = sym;
    late[nlate].at = at;
    late[nlate].kind = kind;
    late[nlate].addend = addend;
    nlate++;
}

int gen_nlate(void)
{
    return nlate;
}

int gen_late_sym(int i)
{
    return late[i].sym;
}

static void late_fill(int bss_base)
{
    int i;

    for (i = 0; i != nlate; i++) {
        long target;

        if (late[i].sym < 0) {
            target = bss_base;
        } else {
            Sym *s = sym_at(late[i].sym);

            if (no_address(late[i].sym) && !name_weak(s->name))
                acc_error("'%s' is used but never defined", name_text(s->name));
            target = no_address(late[i].sym) ? 0 : s->val;
        }
        gen_slot(late[i].at, late[i].kind, target + late[i].addend);
    }
}

int no_address(int sym)
{
    return sym_at(sym)->kind == SYM_FUNC
           ? !(sym_flags(sym) & SYMF_DEFINED)
           : sym_at(sym)->val < 0;
}

/* The calls and addresses still waiting on something, which is how a link
 * knows what to go looking for in a library. */
int gen_nfixups(void)
{
    return nfixups;
}

int gen_fixup_sym(int i)
{
    return fixup_at(i)->fn;
}

int gen_no_address(int sym)
{
    return no_address(sym);
}

/* The k-th addition gen_finish left for the image's file, in the place
 * of a fixup already read: see there. */
static const OutAdd *add_at(int k)
{
    return (const OutAdd *) (const void *) fixup_at(k);
}

void gen_finish(void)
{
    OutAdd   add;
    int      nadds = 0, last = 0, i;
    Fixup   *f, *k;
    RtFixup *r, *rend;

#ifndef ACC_NODROP
    drop_unused_statics();
#endif

    /* A helper wanted by name, which is how one arrives from an object: that
     * object used it and did not carry the blob. Claimed before anything is
     * laid down, so that the blob knows how much of itself to be. */
    /* The loops below walk the fixups by pointer: indexed, each use was a
     * multiply by the entry's width, and a call in between meant it was
     * worked out again. Nothing in them adds a fixup, so the table stays
     * where it is. */
    if (!gen_objects)
        for (i = 0, f = fixup_at(0); i != nfixups; i++, f = FIXUP_STEP(f, i)) {
            int which;

            if (!no_address(f->fn))
                continue;
            which = rt_which(name_text(sym_at(f->fn)->name));
            if (which < 0)
                continue;
            rt_syms[which] = f->fn;
            rt_wanted(which);
        }

    /* Compiling to an object, a call to a helper is a call to a name, and
     * the blob stays here: one copy of it goes into the program that is
     * linked, rather than one into every object that multiplies. */
    if (gen_objects) {
        extern_reserve(nexterns + nrt_fixups + nfixups);
        for (r = rt_fixups, rend = rt_fixups + nrt_fixups; r != rend; r++)
            extern_add(r->at, rt_symbol(r->which));
        externs_helpers_end = externs_put;
        externs_sort(externs, externs_put);
    } else {
        rt_emit_used();
        args_emit();
        bss_emit();

        /* The stub's stack top, a name made here rather than with the
         * stub: made there, it was interned ahead of every name in the
         * source and moved all of them, and the macro table hashes by
         * where a name is -- which cost text.c 13,000 cycles in collisions
         * for nothing. Made after the helpers are claimed, it is not
         * compared against every helper's name either. */
        stack_top_sym = sym_push(name_intern("acc_stack_top", 13),
                                 SYM_GLOBAL, -1);
        sym_set_flags(stack_top_sym, SYMF_DECLARED | SYMF_EXTERN);
        fixup_add(stack_top_sym, stack_top_at);

        /* And the ones the link itself answers for, now that there is an
         * answer: everything else has been laid down. */
        for (i = 0, f = fixup_at(0); i != nfixups; i++, f = FIXUP_STEP(f, i)) {
            int sym = f->fn, at;

            if (!no_address(sym) || (at = link_given(sym)) == 0)
                continue;
            sym_at(sym)->val = at;
            sym_set_flags(sym, SYMF_DEFINED);
        }
    }

    k = fixup_at(0);
    for (i = 0, f = k; i != nfixups; i++, f = FIXUP_STEP(f, i)) {
        Sym *fn = sym_at(f->fn);


        if (no_address(f->fn)) {
            if (gen_objects) {
                extern_add(f->at, f->fn);
                continue;
            }
        }

        /* A weak reference nothing else wanted is at zero, and so is what
         * reads it as a pointer: see do_pragma. */
        if (no_address(f->fn) && name_weak(fn->name)) {
            fn->val = 0;
            if (fn->kind == SYM_FUNC)
                sym_set_flags(f->fn, SYMF_DEFINED);
        }
        if (no_address(f->fn)) {
            if (fn->kind == SYM_FUNC)
                acc_error("'%s' is called but never defined",
                          name_text(fn->name));
            acc_error("'%s' is declared and used but never given room, so "
                      "another file has to define it -- which needs the "
                      "pieces linked together", name_text(fn->name));
        }

        /* The address is added to whatever the slot was emitted with, which
         * is nothing for a call or a register load and is the amount added
         * for an address in a global's bytes: `int *p = &g + 1` puts the one
         * there, because where g is was not known when the bytes were
         * written.
         *
         * A slot in memory is filled now. One in the image's file becomes
         * the addition the file is to get, written over this fixup's own
         * place or one before it, which have been read: so the fixups are
         * the list of them, and there is no patch for each as well. They
         * come in the order of their slots, as the code was written; one
         * that does not -- the stack's top, in the stub at the image's start,
         * asked for above -- is a patch of its own instead. */
        if ((unsigned) (f->at - out_base) >= (unsigned) out_flushed
            || (unsigned) f->at < (unsigned) last) {
            out_add24(f->at, fn->val);
            continue;
        }
        add.value = fn->val;
        add.at = last = f->at;
        memcpy((void *) k, &add, sizeof add);
        nadds++;
        k = FIXUP_STEP(k, nadds);
    }
    if (nadds)
        out_add_later(add_at, nadds);
    if (gen_objects)
        externs_sort(externs_helpers_end, externs_put);
    late_fill(bss_start);
}

/* The first thing in the image, because MOS enters at its first byte.
 *
 * The bytes come from src/rt/startup.s, assembled and copied in rather than
 * hand-encoded, so that the source of truth is assembly anyone can read and
 * reassemble. Three versions:
 *
 *   return  calls main and returns its result to MOS in hl, printing
 *           nothing, as agondev's startup does. The default: a program
 *           leaves the screen as it found it.
 *   print   calls main, writes the result as six hex digits and returns to
 *           MOS: -p, for reading an answer at a command prompt or on a real
 *           Agon.
 *   exit    calls main and hands the low byte to IO port 0, which stops the
 *           emulator with that byte as its exit status: -x, which is how the
 *           tests read an answer with no C library and nothing to print with.
 */
/* IY is saved and put back around the whole of the program, in every stub.
 *
 * MOS wants it as it left it: a program that returns having changed it takes
 * the machine down, and takes it down after the program has run and printed
 * its answer, which is as confusing a way to find this out as there is. It
 * cost a morning. The backend uses IY as its own scratch -- see the note on
 * the value stack -- so the program will have changed it, and the stub is
 * the one place that can put it back whatever the program did.
 *
 * In the exit stub too, though that one stops the machine rather than
 * returning: the contract is that a program acc compiles gives IY back, and
 * a contract with a hole in it for the mode the tests use is worse than no
 * contract. */
static const unsigned char startup_exit[] = {
    0xfd, 0xe5,                                 /* push iy */
    0xe5,                                       /* push hl: MOS's line */
    0xcd, 0x00, 0x00, 0x00,                     /* call the clearing */
    0xe1,                                       /* pop hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call the arguments */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (mos_sp), sp */
    0x31, 0x00, 0x00, 0x00,                     /* ld sp, the top */
    0xe5, 0xd5,                                 /* push hl / push de */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (exit_sp), sp */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, after main */
    0x22, 0x00, 0x00, 0x00,                     /* ld (exit_pc), hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call main */
    0xc1, 0xc1,                                 /* pop bc / pop bc */
    0xe5,                                       /* push hl: the status */
    0x2a, 0x00, 0x00, 0x00,                     /* ld hl, (exit hook) */
    0xcd, 0x00, 0x00, 0x00,                     /* call jp (hl) below */
    0xe1,                                       /* pop hl */
    0xed, 0x7b, 0x00, 0x00, 0x00,               /* ld sp, (mos_sp) */
    0x7d, 0xd3, 0x00,                           /* ld a, l / out (0), a */
    0xfd, 0xe1,                                 /* pop iy */
    0xc9,
    0xe9,                                       /* jp (hl) */
    0xc9                                        /* the hook if none */
};

static const unsigned char startup_return[] = {
    0xfd, 0xe5,                                 /* push iy */
    0xe5,                                       /* push hl: MOS's line */
    0xcd, 0x00, 0x00, 0x00,                     /* call the clearing */
    0xe1,                                       /* pop hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call the arguments */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (mos_sp), sp */
    0x31, 0x00, 0x00, 0x00,                     /* ld sp, the top */
    0xe5, 0xd5,                                 /* push hl / push de */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (exit_sp), sp */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, after main */
    0x22, 0x00, 0x00, 0x00,                     /* ld (exit_pc), hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call main */
    0xc1, 0xc1,                                 /* pop bc / pop bc */
    0xe5,                                       /* push hl: the status */
    0x2a, 0x00, 0x00, 0x00,                     /* ld hl, (exit hook) */
    0xcd, 0x00, 0x00, 0x00,                     /* call jp (hl) below */
    0xe1,                                       /* pop hl */
    0xed, 0x7b, 0x00, 0x00, 0x00,               /* ld sp, (mos_sp) */
    0xfd, 0xe1,                                 /* pop iy */
    0xc9,                                       /* ret, the status in hl */
    0xe9,                                       /* jp (hl) */
    0xc9                                        /* the hook if none */
};

static const unsigned char startup_print[] = {
    0xfd, 0xe5,                                 /* push iy */
    0xe5,                                       /* push hl: MOS's line */
    0xcd, 0x00, 0x00, 0x00,                     /* call the clearing */
    0xe1,                                       /* pop hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call the arguments */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (mos_sp), sp */
    0x31, 0x00, 0x00, 0x00,                     /* ld sp, the top */
    0xe5, 0xd5,                                 /* push hl / push de */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (exit_sp), sp */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, after main */
    0x22, 0x00, 0x00, 0x00,                     /* ld (exit_pc), hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call main */
    0xc1, 0xc1,                                 /* pop bc / pop bc */
    0xe5,                                       /* push hl: the status */
    0x2a, 0x00, 0x00, 0x00,                     /* ld hl, (exit hook) */
    0xcd, 0x00, 0x00, 0x00,                     /* call jp (hl) below */
    0xe1,                                       /* pop hl */
    0xed, 0x7b, 0x00, 0x00, 0x00,               /* ld sp, (mos_sp) */
    0xe5,                                       /* push hl */
    0xfd, 0x21, 0x00, 0x00, 0x00, 0xfd, 0x39,   /* ld iy, 0 / add iy, sp */
    0xfd, 0x7e, 0x02, 0xcd, 0x00, 0x00, 0x00,   /* ld a, (iy+2) / hexbyte */
    0xfd, 0x7e, 0x01, 0xcd, 0x00, 0x00, 0x00,
    0xfd, 0x7e, 0x00, 0xcd, 0x00, 0x00, 0x00,
    0xe1,                                       /* pop hl */
    0x3e, 0x0d, 0x5b, 0xd7,                     /* ld a, 13 / rst.lil 0x10 */
    0x3e, 0x0a, 0x5b, 0xd7,                     /* ld a, 10 / rst.lil 0x10 */
    0xfd, 0xe1,                                 /* pop iy */
    0xc9,
    0xf5, 0x1f, 0x1f, 0x1f, 0x1f,               /* hexbyte: push af / rra x4 */
    0xcd, 0x00, 0x00, 0x00, 0xf1,               /* call hexnib / pop af */
    0xe6, 0x0f, 0xc6, 0x30, 0xfe, 0x3a,         /* hexnib: the digit */
    0x38, 0x02, 0xc6, 0x07, 0x5b, 0xd7, 0xc9,
    0xe9,                                       /* jp (hl) */
    0xc9                                        /* the hook if none */
};

/* The three holes in either stub, as offsets from its first byte: the call
 * into the routine that clears what starts at zero, the one that turns MOS's
 * command line into argc and argv, and the one into main. */
#define STUB_CLEAR_AT   4
#define STUB_ARGS_AT    9
/* Where exit comes back to, and where the stack was when main was called:
 * see gen_exit. The stub writes both before calling main, so that exit has
 * somewhere to go and a stack to go there on. */
#define STUB_SP_AT     25
#define STUB_PC_AT     29
#define STUB_PCSTORE_AT 33
#define STUB_MAIN_AT   37
#define STUB_AFTER_MAIN 40
/* The exit hook: the load of its cell, and the call to the `jp (hl)` that
 * each stub has as its last two bytes but one. */
#define STUB_HOOK_AT   44
#define STUB_HOOK_CALL_AT 48

/* The stack main runs on, which is not MOS's: see src/rt/startup.s. The stub
 * keeps MOS's stack pointer in a cell behind itself, moves to the top of the
 * program's memory, and puts MOS's back before it returns. */
#define STUB_MOS_SP_AT      14
#define STUB_TOP_AT         18
#define STUB_MOS_SP_BACK_AT 54

/* Where the print stub calls within itself, as offsets from its first byte.
 * They are absolute calls, so they have to be filled in once the stub's
 * address is known. */
static const struct { int at, to; } print_calls[] = {
    { 0x45, 0x62 }, { 0x4c, 0x62 }, { 0x53, 0x62 },   /* hexbyte */
    { 0x68, 0x6c }                                    /* hexnib */
};

/* The two cells the stub writes. See gen_startup and gen_exit.
 *
 * They are known by name, because a file compiled to an object has never
 * seen the stub: the link lays that down, and until it does there is no
 * saying where the cells are. So the object leaves a name behind and the
 * link answers it -- which is what a call to a helper does, for the same
 * reason.
 *
 * The names are acc's own. A program that spells one of them means this. */
static const char *const exit_cell_name[3] = {
    "__acc_exit_sp", "__acc_exit_pc", "__acc_exit_hook"
};

static int exit_cell_syms[3] = { SYM_NONE, SYM_NONE, SYM_NONE };

int exit_cell(int which)
{
    if (exit_cell_syms[which] == SYM_NONE) {
        const char *name = exit_cell_name[which];
        int sym = sym_push(name_intern(name, (int) strlen(name)),
                           SYM_GLOBAL, -1);

        /* extern, so that the pass over the globals that gives a tentative
         * definition its room in the bss leaves these alone: gen_startup
         * puts them in the image instead, and a file compiled to an object
         * wants them left for the link to find. */
        sym_set_flags(sym, SYMF_DECLARED | SYMF_EXTERN);
        exit_cell_syms[which] = sym;
    }

    return exit_cell_syms[which];
}

void gen_startup(int ending, const char *program)
{
    int m = sym_push(name_intern("main", 4), SYM_FUNC, 0);
    const unsigned char *stub = ending == END_EXIT ? startup_exit
                                : ending == END_PRINT ? startup_print
                                : startup_return;
    int n = ending == END_EXIT ? (int) sizeof startup_exit
            : ending == END_PRINT ? (int) sizeof startup_print
            : (int) sizeof startup_return;
    int base;
    int i;

    exec_name = program;

    base = out_here();
    for (i = 0; i != n; i++)
        out_byte(stub[i]);

    /* The two routines that are written at the end, once there is an address
     * and a length for what they work on: clearing what starts at zero, and
     * making argc and argv out of what MOS passed. Both calls are here
     * whether they turn out to have anything to do or not -- four bytes and
     * a `ret` each, against working out how to not have made the call. */
    out_reloc(base + STUB_CLEAR_AT);
    bss_init_hole = base + STUB_CLEAR_AT;
    out_reloc(base + STUB_ARGS_AT);
    args_hole = base + STUB_ARGS_AT;

    /* Where exit unwinds to, and the stack it unwinds onto. Two cells
     * written by the stub just before it calls main: the stack pointer
     * then, and the address of the instruction that main returns to. exit
     * puts the second in the program counter with the first in the stack
     * pointer, and the tail below runs as though main had returned. See
     * gen_exit.
     *
     * They live here, in the image behind the stub, and not in the bss.
     * Six bytes of RAM either way, and here they cost nothing to clear:
     * the stub writes both before anything reads either, so zero is not a
     * value they ever have to start at. A program with nothing else at
     * zero then still has nothing there, and the routine that clears it is
     * still a bare `ret`.
     *
     * The stub writes them whether the program calls exit or not, which is
     * nineteen bytes a program. Working out whether it does would mean
     * knowing before the file is read. */
    for (i = 0; i < 2 * ACC_INT_SIZE; i++)
        out_byte(0);
    sym_at(exit_cell(0))->val = out_here() - 2 * ACC_INT_SIZE;
    sym_at(exit_cell(1))->val = out_here() - ACC_INT_SIZE;

    /* And the exit hook: what the stub calls on the way out, whether main
     * returned or exit sent it there -- see src/rt/startup.s. It starts at
     * the `ret` that ends the stub, and the library's atexit and fopen point
     * it at the routine that runs atexit's functions and closes the files.
     * A name, like the other two, so that the library can reach it. */
    out_reloc(out_here());
    out_word24(base + n - 1);
    sym_at(exit_cell(2))->val = out_here() - ACC_INT_SIZE;
    out_reloc(base + STUB_HOOK_AT);
    out_patch24(base + STUB_HOOK_AT, out_here() - ACC_INT_SIZE);
    out_reloc(base + STUB_HOOK_CALL_AT);
    out_patch24(base + STUB_HOOK_CALL_AT, base + n - 2);

    /* And a third, for MOS's stack pointer while main runs on its own at the
     * top of the program's memory -- which is where the heap already leaves
     * room for it. This one only the stub reads, so it needs no name. */
    {
        int mos_sp = out_here();

        for (i = 0; i != ACC_INT_SIZE; i++)
            out_byte(0);
        out_reloc(base + STUB_MOS_SP_AT);
        out_patch24(base + STUB_MOS_SP_AT, mos_sp);
        out_reloc(base + STUB_MOS_SP_BACK_AT);
        out_patch24(base + STUB_MOS_SP_BACK_AT, mos_sp);
        /* The top of memory moves with where the image is loaded, so it is
         * relocated -- but not by the pass that shortens jumps, which moves
         * every address in the image along with the code, and this one is
         * past the image: written straight away it came out 71 bytes low,
         * that being how far the jumps had shrunk. So it is a name the link
         * answers, after everything is laid down, as the heap's end is. */
        stack_top_at = base + STUB_TOP_AT;     /* see gen_finish */
    }
    sym_set_flags(exit_cell(0), SYMF_DEFINED);
    sym_set_flags(exit_cell(1), SYMF_DEFINED);
    sym_set_flags(exit_cell(2), SYMF_DEFINED);
    fixup_add(exit_cell(0), base + STUB_SP_AT);
    fixup_add(exit_cell(1), base + STUB_PCSTORE_AT);
    out_reloc(base + STUB_PC_AT);
    out_patch24(base + STUB_PC_AT, base + STUB_AFTER_MAIN);

    fixup_add(m, base + STUB_MAIN_AT);

    if (ending == END_PRINT)
        for (i = 0; i < (int) (sizeof print_calls / sizeof *print_calls); i++) {
            out_reloc(base + print_calls[i].at);
            out_patch24(base + print_calls[i].at, base + print_calls[i].to);
        }
}
