/*
 * The output image: where it is held and how it grows, the bytes written
 * into it and read and patched back, and the file it goes to as it is made
 * and is copied out of at the end.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "out_int.h"

/* MOS loads a program at 0x040000 and enters it at 0x040045, past a 64-byte
 * header: a jump over itself, a 60-byte name field, then the signature and a
 * byte saying the program runs in ADL mode. There is no loader beyond that --
 * no relocation, no sections -- so what is written here is what runs. */
#define LOAD_ADDR    0x040000
#define HEADER_SIZE  0x45

/* Where the image will be loaded, which is 0x040000 for a program MOS runs
 * and settable with -b so that the same program can be built for somewhere
 * else. Nothing in the image is position-independent -- every call, jump and
 * address is absolute -- so moving it means knowing where all of those are,
 * which is what the relocation table below records. */
int out_base = LOAD_ADDR;

/* Held in memory and written at the end, because a call to a function defined
 * further down the file has to be patched once its address is known -- all
 * of it, but for a link: see out_flush. */
/* Held as a write pointer and the end of the buffer rather than as an index
 * and a capacity. Every byte the compiler emits comes through out_byte, and
 * `img[len++]` is an addition of two 24-bit values before the store, where a
 * pointer that walks is a store and an increment. The bound is the same
 * either way: one compare, against the end instead of against the size. */
unsigned char        *out_img;          /* the first byte of the image */
unsigned char        *out_put;          /* where the next byte goes */
unsigned char        *out_limit;        /* one past the last it may use */
static int            cap;
static const char    *out_path;

/* The image goes to a file as it is made, a function or an object at a
 * time, rather than being held whole: see out_flush. The first
 * `out_flushed` bytes are in the file, and memory holds the rest, from
 * out_img + out_flushed: out_img is where the first byte would be, so that
 * an address is found the same way whichever part it is in. What is written
 * to a byte in the file waits in `patches`, and is made when the file is
 * copied out or, for a link, in the file itself.
 *
 * A link's file is its output, patched where it stands at the end. A
 * compile's is `<output>~`, and its bytes are copied to the output at the
 * end: an object's text comes after tables not known until then, and
 * leaving out a file's unused functions takes bytes out of the middle of
 * what is in the file (see out_cut). */
int                   out_flushed;
static FILE          *spill;            /* the file, once it is written to */
static char          *spill_name;       /* and its name */
int                   out_may_flush;    /* OUT_FLUSH_*: see out_flush */

/* Each is PATCH_WIDE bytes: the offset, the value, and the kind. Walked
 * by a pointer, as the relocations are: an index into seven-byte entries
 * is a multiply, which is a call into the runtime on this target. */
#define PATCH_WIDE  7
#define PATCH_SET8  0
#define PATCH_SET24 1
#define PATCH_ADD24 2

/* Kept in blocks of a kilobyte, one after another, and not in one array:
 * a link of zap makes two thousand, and an array that doubled to hold them
 * wanted 42 KB at once at the end of a link, when the heap is fullest. */
#define PATCH_BLOCK (146 * PATCH_WIDE)

typedef struct PatchBlock {
    struct PatchBlock *next;
    unsigned char      bytes[PATCH_BLOCK];
} PatchBlock;

static PatchBlock    *patches, *patch_last;
static unsigned char *patch_put, *patch_limit;

/* The additions gen_finish hands over, in the order of their slots: see
 * out_add_later. */
static const OutAdd *(*later)(int k);
static int           nlater;

void out_add_later(const OutAdd *(*add_at)(int k), int n)
{
    later = add_at;
    nlater = n;
}

/* And the calls into the runtime gen_finish fills, a second list in the
 * order of its slots: see rt_fill_all. */
static const OutAdd *(*later2)(int k);
static int           nlater2;

void out_add_later2(const OutAdd *(*add_at)(int k), int n)
{
    later2 = add_at;
    nlater2 = n;
}

static int (*later_slot)(int k);
static int  nlater_slots, later_base;

void out_add_base_later(int (*slot_at)(int k), int n, int value)
{
    later_slot = slot_at;
    nlater_slots = n;
    later_base = value;
}

#ifdef ACC_TABLE_STATS
/* How often the file was written to, cut, and read back: test/spill.sh. */
int out_nflushes, out_nspill_cuts, out_nresidents;
#endif

/* What the file is read and written through, a piece at a time: each read
 * and write is a call through the whole of MOS's file layer, so a piece of
 * a kilobyte cost a compile that went to a file 5 to 9% where one of 8 KB
 * costs it 3. */
#define SLOTS_ROOM 8192

/* A piece to read the file through, of SLOTS_ROOM, or when the heap has no
 * block that big -- the end of a big compile, when it is in pieces itself
 * -- of a kilobyte: said in *room. */
unsigned char *piece(int *room)
{
    unsigned char *buf = malloc(SLOTS_ROOM);

    *room = SLOTS_ROOM;
    if (!buf) {
        buf = malloc(1024);
        *room = 1024;
    }
    if (!buf)
        acc_error("out of memory for the output");

    return buf;
}

/* Where the image lives.
 *
 * On the Agon, at the top of the heap, out of malloc's way. agondev's
 * realloc never extends a block: it mallocs the new one, copies, and frees
 * the old, so an image doubling from 32 KB held 96 KB at once -- the peak of
 * compiling one of zap's or vi's larger files -- and a link's image of a
 * hundred kilobytes could never grow to its size. At the top it grows
 * downwards by moving its bytes within what is then its own: growing costs
 * the new bytes and nothing more. malloc's break is kept below it by acc's
 * own sbrk, which is what malloc calls (the link says --wrap=_sbrk; see
 * Makefile.agon). A move changes out_img as realloc did.
 *
 * On the host, malloc and realloc: memory is not short there, and every
 * test whose program outgrows 4 KB moves the image, which is what shows up
 * code that keeps a pointer into it across a write. So too when acc builds
 * itself for the Agon (test/selfbuild.sh): that links acc's own library,
 * whose malloc is not libagon's. */
#if defined(AGONDEV) && defined(__clang__)     /* agondev's build, libagon's malloc */
extern char *_sbrkbase;                 /* libagon's break, set by crt0 */
extern char __heaptop[];                /* ___heaptop: see src/agon.ld */

static unsigned char *img_low;          /* the image's first byte, or null */

/* malloc's sbrk: libagon's, with the image's bottom as the top of the heap.
 * As libagon's does, it answers null when there is no room, and keeps the
 * break strictly below the top. */
void *_wrap__sbrk(int incr)
{
    char *was = _sbrkbase;
    char *top = img_low ? (char *) img_low : __heaptop;

    /* Unsigned: neither can be negative, and a signed compare is a call
     * to repair the flags after it. */
    if ((unsigned) incr >= (unsigned) (top - was))
        return NULL;
    _sbrkbase = was + incr;

    return was;
}

static unsigned char *img_take(int want)
{
    unsigned char *low = (unsigned char *) __heaptop - want;

    if (low <= (unsigned char *) _sbrkbase)
        return NULL;
    img_low = low;

    return low;
}

/* Twice as big, or failing that as big as there is room for, so long as
 * that is a kilobyte more. */
static unsigned char *img_grow(unsigned char *img, int used, int *capp)
{
    int want = *capp * 2;
    unsigned char *low = (unsigned char *) __heaptop - want;

    if (low <= (unsigned char *) _sbrkbase) {
        low = (unsigned char *) _sbrkbase + 1;
        want = (int) ((unsigned char *) __heaptop - low);
        if ((unsigned) want < (unsigned) *capp + 1024)
            return NULL;
    }
    memmove(low, img, (size_t) used);
    img_low = low;
    *capp = want;

    return low;
}

static void img_free(unsigned char *img)
{
    (void) img;
    img_low = NULL;
}
#else
static unsigned char *img_take(int want)
{
    return malloc((size_t) want);
}

static unsigned char *img_grow(unsigned char *img, int used, int *capp)
{
    (void) used;
    *capp *= 2;

    return realloc(img, (size_t) *capp);
}

static void img_free(unsigned char *img)
{
    free(img);
}
#endif

extern int out_start_cap;

static void hold(unsigned char *held, int used);

/* `header` is whether MOS's is wanted: a program has one and an object does
 * not, its bytes being something a linker will put after a header of its own
 * making. */
void out_open(const char *path, int header)
{
    static const unsigned char hdr[HEADER_SIZE - 4] = { 0 };
    const char *base, *scan;

    cap = out_start_cap;
    hold(img_take(cap), 0);
    out_reloc_grow();                   /* so the sentinel is there to read */
    out_path = path;
    if (!header)
        return;

    /* jp 0x040045, over the header */
    out_byte(0xc3);
    out_reloc(out_base + 1);
    out_word24(out_base + HEADER_SIZE);

    memcpy(out_put, hdr, sizeof hdr);
    out_put += sizeof hdr;

    /* The name MOS lists the program under, which is the file it is written
     * to. agondev has a separate step for this; there is no reason for one. */
    base = path;
    for (scan = path; *scan; scan++)
        if (*scan == '/' || *scan == '\\')
            base = scan + 1;
    {
        size_t name_len = strlen(base);

        if (name_len > 0x40 - 4 - 1)
            name_len = 0x40 - 4 - 1;
        memcpy(out_img + 4, base, name_len);
    }
    out_img[0x40] = 'M';
    out_img[0x41] = 'O';
    out_img[0x42] = 'S';
    out_img[0x43] = 0;        /* header version */
    out_img[0x44] = 1;        /* ADL, 24-bit addressing */
}

/* Kept out of the emitters in acc.h, which are inlined wherever an
 * instruction is written. Inline, its `cap *= 2` and its error string would
 * give every one of those places a frame slot to serve a path taken a dozen
 * times in a compile. Out of line, what is inlined is a compare and a store.
 *
 * The emitters used to be functions here, and an earlier note said inlining
 * them was slower. That was before the output became a walking pointer, and
 * it is no longer true: each call opened a frame to do a compare and a store,
 * and doing without the calls took big.c from 871 to 848 cycles a byte. */
/* Where the image starts: 16 KB on the Agon, which is most programs' whole
 * image.
 *
 * It grows by doubling, and on the Agon a growth holds both copies at once:
 * agondev's realloc never extends a block, it mallocs the new one, copies,
 * and frees the old. It used to start at 64 KB so that it would never grow,
 * and those 64 KB came out of every compile however small its output --
 * enough that no file including <agon/vdp.h> could be compiled on the
 * machine, and that 7 of aed's 23 did not fit. 16 KB still leaves an image
 * that outgrows it to double, and a thousand-function source of 53 KB still
 * compiles (test/cycles.sh); the headers and 20 of aed's files now do too
 * (test/headers.sh).
 *
 * On the host, 4 KB, so that the tests move the image often: that is what
 * shows up code keeping a pointer into it across a write. test/test_out.c
 * changes it to see it grow. */
#ifdef AGONDEV
int out_start_cap = 16384;
#else
int out_start_cap = 4096;
#endif

int out_capacity(void)
{
    return cap;
}

/* The image as `held`, of cap bytes, `used` of them written: past the
 * `out_flushed` bytes in the file. */
static void hold(unsigned char *held, int used)
{
    if (!held)
        acc_error("out of memory for the output");
    out_img = held - out_flushed;
    out_put = held + used;
    out_limit = held + cap;
}

void out_grow(void)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    /* One doubling is enough for any one write: cap starts at 4096 or more
     * and the largest is the four bytes of out_opcode24. */
    hold(img_grow(held, used, &cap), used);
}

/* The file the image goes to, opened the first time it is needed: the
 * output itself for a link, `<output>~` for a compile. */
static void spill_open(void)
{
    size_t n = strlen(out_path);

    spill_name = (char *) out_path;
    if (out_may_flush == OUT_FLUSH_COPY) {
        spill_name = malloc(n + 2);
        if (!spill_name)
            acc_error("out of memory for the output");
        memcpy(spill_name, out_path, n);
        spill_name[n] = '~';
        spill_name[n + 1] = '\0';
    }
    spill = fopen(spill_name, "w+b");
    if (!spill)
        acc_error("cannot write '%s'", spill_name);
}

/* `n` bytes at `off` in the file, read into `buf`, or with `write`
 * written from it. */
void spill_io(int off, unsigned char *buf, int n, int write)
{
    if (fseek(spill, off, SEEK_SET) != 0
        || (int) (write ? fwrite(buf, 1, (size_t) n, spill)
                        : fread(buf, 1, (size_t) n, spill)) != n)
        acc_error("short %s on '%s'", write ? "write" : "read", spill_name);
}

/* How much of the image memory holds before out_flush writes it to the
 * file. On the Agon 32 KB: a compile whose image is smaller -- most of
 * them, and every input of test/bench.sh -- never writes one and pays
 * nothing, and one that is bigger has a file of a hundred kilobytes and
 * more to go to. On the host 2 KB, so that the tests go to a file all the
 * time: they are what shows the file's bytes to be the image's. */
#ifdef AGONDEV
int out_flush_at = 32768;
#else
int out_flush_at = 2048;
#endif

/* What memory holds of the image, written to its file and let go of once
 * it is out_flush_at. Called where nothing written
 * so far will be written again but through out_patch and out_add24, or read
 * but through out_resident: between a link's objects, and at the end of
 * each function a compile writes -- a function's jumps, its constants and
 * its frame are all settled within it.
 *
 * The image starts again at its first size, and on the Agon what it grew to
 * goes back to malloc. */
void out_flush(void)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    if (!out_may_flush || !used
        || (out_may_flush == OUT_FLUSH_COPY
            && (unsigned) used < (unsigned) out_flush_at))
        return;
    if (!spill)
        spill_open();
    COUNT(out_nflushes);
    spill_io(out_flushed, held, used, 1);
    out_flushed += used;
    img_free(held);
    cap = out_start_cap;
    hold(img_take(cap), 0);

    /* A link cuts nothing and writes no table of its relocations, so the
     * ones in what went to the file are no one's any more: the table keeps
     * its sentinel and starts again. */
    if (out_may_flush == OUT_FLUSH_IN_PLACE)
        out_reloc_put = out_relocs + 1;
}

/* Before the file is read back or cut: nothing waiting to be written in
 * it, since a patch says where its bytes are now. A compile fills its
 * slots only at its end (gen_finish), after both, so none is waiting; one
 * that was would be written in the wrong place, and is refused. */
void out_cut_prepare(void)
{
    if (out_flushed && patches)
        acc_error("internal: the file is moved with patches waiting on it");
}

/* The image from `at` on, in memory again: what a global defined a second
 * time writes over (out_seek), or what a variable moved out of the bss
 * reads back (gen_bss_move). Both are rare, and both may reach bytes a
 * function ago; so the file's tail from `at` is read back in and taken off
 * it. */
void out_resident(int at)
{
    int off = at - out_base, need = out_flushed - off;
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    if ((unsigned) off >= (unsigned) out_flushed)
        return;
    COUNT(out_nresidents);
    out_cut_prepare();
    while ((unsigned) (cap - used) < (unsigned) need)
        hold(held = img_grow(held, used, &cap), used);
    memmove(held + need, held, (size_t) used);
    spill_io(off, held, need, 0);
    out_img += need;
    out_put += need;
    out_flushed = off;
}

/* The three bytes of a slot in the file, for the walk that moves every
 * address when a file's unused functions are left out (cut_out): read and
 * written through a kilobyte of the file held here, since the walk goes
 * through the slots in order. */
static unsigned char *slots;
static int            slots_at, slots_len, slots_dirty, slots_room;

static void slots_write_back(void)
{
    if (slots_dirty)
        spill_io(slots_at, slots, slots_len, 1);
    slots_dirty = 0;
}

static unsigned char *slot_bytes(int off)
{
    if (!slots_len
        || (unsigned) (off - slots_at) > (unsigned) (slots_len - 3)) {
        slots_write_back();
        if (!slots)
            slots = piece(&slots_room);
        slots_at = off;
        slots_len = (unsigned) (out_flushed - off) < (unsigned) slots_room
                    ? out_flushed - off : slots_room;
        spill_io(off, slots, slots_len, 0);
    }

    return slots + (off - slots_at);
}

int out_slot_get(int off)
{
    return get24(slot_bytes(off));
}

void out_slot_put(int off, int value)
{
    put24(slot_bytes(off), value);
    slots_dirty = 1;
}

/* Done with the slots: what changed written back, and the kilobyte let go. */
void slots_done(void)
{
    slots_write_back();
    free(slots);
    slots = NULL;
    slots_len = 0;
}





/* Two bytes and three bytes, which is what nearly every instruction acc emits
 * is made of: a prefix and an opcode, or those and a displacement.
 *
 * Written as repeated out_byte calls they cost a call and a bounds check
 * each. The check is the same one either way -- the buffer either has room
 * for the whole instruction or it does not -- and the call is pure overhead
 * on the path that runs once per byte of every program compiled. */


void out_word24(int value)
{
    /* One bounds check and three stores, rather than three calls that each
     * check. Every call instruction and every loaded constant emits one of
     * these, so it is a third of the output path. */
    if ((unsigned) (out_limit - out_put) < 3)
        out_grow();
    put24(out_put, value);
    out_put += 3;
}


/* How many times the image has been taken back. A mark something has left
 * on bytes it wrote -- see vcmp's -- is good only while this is what it was
 * when the mark was made: taken back past the mark and written again, the
 * image can come back to the very place the mark ends, with other bytes
 * before it. */
unsigned out_rewinds;

/* The lowest address the image has been taken back to since whoever reads
 * this last set it high: which marks a rewind reached, where out_rewinds
 * says only that there was one. */
int out_rewind_floor = INT_MAX;
int out_rewound_to = INT_MAX;       /* the same, for insn.c's ld_rr_ix */

/* Told of every rewind, if set: what the code generator keeps that a
 * rewind can take back. A pointer rather than a call, so that the image stands
 * on its own. */
void (*out_on_rewind)(int here);

void out_rewind(int here)
{
    if ((unsigned) (here - out_base) < (unsigned) out_flushed)
        acc_error("internal: a rewind to %06x, which is in the file", here);
    out_rewinds++;
    if (here < out_rewind_floor)
        out_rewind_floor = here;
    if ((unsigned) here < (unsigned) out_rewound_to)
        out_rewound_to = here;
    if (out_on_rewind)
        out_on_rewind(here);
    /* The slots recorded in what is being undone go with it. The table is
     * in increasing order, so the ones to drop are the last ones. */
    while (out_reloc_put > out_relocs + 1 && out_reloc_put[-1] >= here - out_base)
        out_reloc_put--;
    out_put = out_img + (here - out_base);
}

/* Back to an address already written, to write over it: a variable said twice
 * at file scope, whose value goes where the first mention reserved room for
 * it. Nothing is being undone -- everything compiled since stays, relocations
 * and all -- so this is not out_rewind, which forgets. */
void out_seek(int here)
{
    out_resident(here);
    out_put = out_img + (here - out_base);
}

/* Bytes already written, copied back out: an initialiser that turns out to
 * be given out of order has to take what it wrote in order and put it in the
 * buffer the walk builds. */
void out_copy(int at, unsigned char *to, int len)
{
    int off;

    if (len <= 0)
        return;                 /* nothing written yet, and `to` may be null */
    out_resident(at);
    off = at - out_base;
    if (off < out_flushed || off + len > OUT_LEN)
        acc_error("internal: a read at %06x is outside the image", at);
    memcpy(to, out_img + off, (size_t) len);
}

/* The three bytes at `at`, read back: what a relocation's slot was emitted
 * with, which for an address in a global's bytes is the amount to add to the
 * symbol's own address. */
int out_read24(int at)
{
    int off = at - out_base;

    out_resident(at);
    if (off < out_flushed || off + 3 > OUT_LEN)
        acc_error("internal: a read at %06x is outside the image", at);

    return get24(out_img + off);
}

/* A patch made: `value` written to the bytes at `at`, or added to them. */
static void patch_apply(unsigned char *at, int value, int kind)
{
    if (kind == PATCH_SET8)
        *at = (unsigned char) value;
    else
        put24(at, kind == PATCH_SET24 ? value : get24(at) + value);
}

/* A patch to the image at `at`: made, or for bytes in the file already,
 * kept for out_close. A slot is never split between the two: a link
 * flushes between objects, and no slot is split across two of them. */
static void patch(int at, int value, int kind)
{
    int off = at - out_base, width = kind == PATCH_SET8 ? 1 : 3;

    if ((unsigned) off >= (unsigned) OUT_LEN
        || (unsigned) (OUT_LEN - off) < (unsigned) width
        || ((unsigned) off < (unsigned) out_flushed
            && (unsigned) (off + width) > (unsigned) out_flushed))
        acc_error("internal: patch at %06x is outside the image", at);
    if ((unsigned) off >= (unsigned) out_flushed) {
        patch_apply(out_img + off, value, kind);

        return;
    }
    if (patch_put == patch_limit) {
        PatchBlock *b = malloc(sizeof *b);

        if (!b)
            acc_error("out of memory for the patches to the output");
        b->next = NULL;
        if (patch_last)
            patch_last->next = b;
        else
            patches = b;
        patch_last = b;
        patch_put = b->bytes;
        patch_limit = b->bytes + PATCH_BLOCK;
    }
    put24(patch_put, off);
    put24(patch_put + 3, value);
    patch_put[6] = (unsigned char) kind;
    patch_put += PATCH_WIDE;
}

/* One byte and two, for what an assembler's objects ask of a slot. */
void out_patch8(int at, int value)
{
    patch(at, value, PATCH_SET8);
}

void out_patch16(int at, int value)
{
    out_patch8(at, value & 0xff);
    out_patch8(at + 1, (value >> 8) & 0xff);
}

void out_patch24(int at, int value)
{
    patch(at, value, PATCH_SET24);
}

/* `delta` added to the three bytes at `at`: an address filled in on top of
 * what its slot was emitted with, which may be in the file by then. */
void out_add24(int at, int delta)
{
    patch(at, delta, PATCH_ADD24);
}

/* The image and its table let go of, without writing anything: what the
 * object writer wants, having written the same bytes itself. */
static void patches_free(void)
{
    while (patches) {
        PatchBlock *next = patches->next;

        free(patches);
        patches = next;
    }
    patch_last = NULL;
    patch_put = patch_limit = NULL;
}

/* The file's bytes from 0 to out_flushed, with the patches waiting on them
 * made: written to `to`, or with `to` null back where they are, a piece at a
 * time through `buf`, of `room`. Each patch is made in the order it was
 * made, in the piece it starts in, short of the piece's last two bytes;
 * those are carried to the front of the next piece -- with what a patch
 * that started before them wrote in them -- so a slot is never split. */
static void sweep(FILE *to, unsigned char *buf, int room)
{
    int from = 0, have = 0, k = 0, k2 = 0, s = 0;

    while (from != out_flushed) {
        int n = out_flushed - from, stop;
        const PatchBlock *b;

        if ((unsigned) n > (unsigned) room)
            n = room;
        spill_io(from + have, buf + have, n - have, 0);
        stop = from + n == out_flushed ? out_flushed : from + n - 2;
        for (b = patches; b; b = b->next) {
            const unsigned char *p = b->bytes;
            const unsigned char *end = b == patch_last ? patch_put
                                                       : p + PATCH_BLOCK;

            for (; p != end; p += PATCH_WIDE)
                if ((unsigned) (get24(p) - from) < (unsigned) (stop - from))
                    patch_apply(buf + (get24(p) - from), get24(p + 3), p[6]);
        }
        /* And the additions, which are in the order of their slots. */
        for (; k != nlater; k++) {
            const OutAdd *add = later(k);

            if ((unsigned) (add->at - out_base - from) >= (unsigned) (stop - from))
                break;
            patch_apply(buf + (add->at - out_base - from), add->value,
                        PATCH_ADD24);
        }
        for (; k2 != nlater2; k2++) {
            const OutAdd *add = later2(k2);

            if ((unsigned) (add->at - out_base - from) >= (unsigned) (stop - from))
                break;
            patch_apply(buf + (add->at - out_base - from), add->value,
                        PATCH_ADD24);
        }
        for (; s != nlater_slots; s++) {
            int at = later_slot(s) - out_base - from;

            if ((unsigned) at >= (unsigned) (stop - from))
                break;
            patch_apply(buf + at, later_base, PATCH_ADD24);
        }
        if (!to)
            spill_io(from, buf, stop - from, 1);
        else if ((int) fwrite(buf, 1, (size_t) (stop - from), to) != stop - from)
            acc_error("short write on '%s'", out_path);
        have = from + n - stop;
        memmove(buf, buf + (stop - from), (size_t) have);
        from = stop;
    }
}

/* The image, written to `to`: from the file and then from memory, or all
 * from memory when none of it went to a file. */
void out_write_text(void *to)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);

    if (spill) {
        int room;
        unsigned char *buf = piece(&room);

        sweep((FILE *) to, buf, room);
        free(buf);
    }
    if (used && (int) fwrite(held, 1, (size_t) used, (FILE *) to) != used)
        acc_error("short write on '%s'", out_path);
}

/* A compile's file, which is copied out and no longer wanted. */
static void spill_remove(void)
{
    if (!spill)
        return;
    fclose(spill);
    spill = NULL;
    remove(spill_name);
    if (spill_name != out_path)
        free(spill_name);
    spill_name = NULL;
}

void out_free(void)
{
    if (out_may_flush == OUT_FLUSH_COPY)
        spill_remove();
    img_free(out_img + out_flushed);
    out_img = NULL;
    out_flushed = 0;
    patches_free();
    later = NULL;
    nlater = 0;
    later_slot = NULL;
    nlater_slots = 0;
    nlater2 = 0;
    free(out_relocs);
    out_relocs = out_reloc_put = out_reloc_limit = NULL;
}

#ifdef ACC_TABLE_STATS
/* How many patches and additions wait for out_close: see
 * test/linkstream.sh. */
int out_npatches(void)
{
    const PatchBlock *b;
    int n = 0;

    for (b = patches; b; b = b->next)
        n += (int) ((b == patch_last ? patch_put : b->bytes + PATCH_BLOCK)
                    - b->bytes) / PATCH_WIDE;

    return n + nlater + nlater2 + nlater_slots; /* the additions wait on it too */
}

/* How many of those are additions, and not patches: test/linkfixups.sh. */
int out_nadds(void)
{
    return nlater + nlater2 + nlater_slots;
}
#endif

void out_close(void)
{
    unsigned char *held = out_img + out_flushed;
    int used = (int) (out_put - held);
    FILE *file;

    /* A link's file is the output: the rest written after it, and the
     * patches made where they stand. */
    if (spill && out_may_flush == OUT_FLUSH_IN_PLACE) {
        spill_io(out_flushed, held, used, 1);
        sweep(NULL, held, cap);
        fclose(spill);
        spill = NULL;
        out_free();

        return;
    }
    file = fopen(out_path, "wb");
    if (!file)
        acc_error("cannot write '%s'", out_path);
    out_write_text(file);
    fclose(file);
    out_free();
}

/* A compile or a link that fails leaves no program: what it wrote is not
 * one. */
void out_abandon(void)
{
    if (!spill)
        return;
    fclose(spill);
    spill = NULL;
    remove(spill_name);
}

#ifdef OPT_ACC
/* This file's marks -- where a sequence just emitted ended, and the
 * out_rewinds it was made at -- copied out, or back, for genlog.c, which
 * compiles a function again from where it began and needs every mark as it
 * stood then. Returns how many bytes they take. */
#ifndef STATE_VAR               /* as gen_int.h has it */
#define STATE_VAR(var) do {                                               \
        if (restore)                                                    \
            memcpy(&(var), buf + at, sizeof (var));                     \
        else                                                            \
            memcpy(buf + at, &(var), sizeof (var));                     \
        at += sizeof (var);                                             \
    } while (0)
#endif
size_t image_marks(unsigned char *buf, int restore);

size_t image_marks(unsigned char *buf, int restore)
{
    size_t at = 0;

    STATE_VAR(out_rewinds);
    STATE_VAR(out_rewind_floor);
    STATE_VAR(out_rewound_to);

    return at;
}
#endif
