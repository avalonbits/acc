/*
 * opt-acc's peephole (src/peep.c) on machine code written here: each rule
 * shown taking out what it should, and leaving what it must -- a byte moved
 * and moved back, a widening nothing reads, a move through the stack to a
 * register that holds that already, a jump to the next instruction, a frame
 * slot loaded again after a store of it; and not an address the link fills
 * in, not what a jump lands on, not IY across a call, not what a call into
 * the runtime reads.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "gen_int.h"

static int failures, checks;

static void is(const char *name, long got, long want)
{
    checks++;
    if (got == want) {
        fprintf(stderr, "  ok   %-50s %ld\n", name, got);
    } else {
        fprintf(stderr, "  FAIL %-50s got %ld, want %ld\n", name, got, want);
        failures++;
    }
}

/* What peep.c reads of the compiler, held here: the image, and lists that
 * are empty but where a test fills them. */
#define BASE 0x1000

unsigned char *out_img, *out_put;
int out_base = BASE, out_flushed;
static int relocs[8];
int *out_relocs = relocs, *out_reloc_put = relocs + 1;
int nfixups, nrt_fixups, nbss_fixups, npool_sites;
RtFixup *rt_fixups;
int *pool_site_at;
static RtFixup rts[4];

static Fixup fixups[4];

Fixup *fixup_at(int i)
{
    return &fixups[i];
}

int *bss_fixup(int i)
{
    (void) i;

    return NULL;
}

int out_cut_moved(int a)
{
    return a;
}

/* The runs peep_function cuts, kept for a test to look at. */
static Cut cut_runs[16];
static int ncut_runs;

void relax_cut_code(Cut *cuts, int ncuts, const Mark *from, int fn_from,
                    int fn_to)
{
    (void) from;
    (void) fn_from;
    (void) fn_to;
    ncut_runs = ncuts < 16 ? ncuts : 16;
    memcpy(cut_runs, cuts, (size_t) ncut_runs * sizeof *cuts);
}

void acc_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "  FAIL acc_error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static unsigned char code[256];
static Mark start;

/* The code, from BASE, read and its rules run: whether it was taken. */
static int run(const unsigned char *bytes, int n)
{
    memcpy(code, bytes, (size_t) n);
    out_img = code;
    out_put = code + n;

    return peep_analyse(&start, BASE, BASE + n);
}

static int gone(int offset)
{
    return peep_gone(BASE + offset);
}

static int trimmed(int offset)
{
    return peep_trimmed(BASE + offset);
}

int main(void)
{
    /* ld a, (hl); ld l, a; rlc l; sbc hl, hl; ld l, a; ld a, l; ld (de), a;
     * ld hl, 0; ret -- a byte widened, and only the byte stored: the
     * second ld a, l puts back what A holds, and the widening, then, is
     * read by nothing. */
    static const unsigned char widen[] = {
        0x7e, 0x6f, 0xcb, 0x05, 0xed, 0x62, 0x6f, 0x7d, 0x12,
        0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; jr +0; ret */
    static const unsigned char jr_next[] = { 0x00, 0x18, 0x00, 0xc9 };
    /* nop; push hl; pop bc; ex de, hl; ld hl, (hl); ex de, hl; push bc;
     * pop hl; ld (hl), a; ld bc, 0; ret -- HL kept in BC and brought back
     * when HL holds it still. */
    static const unsigned char moved[] = {
        0x00, 0xe5, 0xc1, 0xeb, 0xed, 0x27, 0xeb, 0xc5, 0xe1, 0x77,
        0x01, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; ld hl, x; ld (hl), 1; ld hl, x; ld (hl), 1; ret */
    static const unsigned char twice[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0x36, 0x01,
        0x21, 0x00, 0x00, 0x00, 0x36, 0x01, 0xc9
    };
    /* nop; jr +0; L: ld a, 1; ld a, 2; ret -- a dead load jumped to */
    static const unsigned char landed[] = {
        0x00, 0x18, 0x00, 0x3e, 0x01, 0x3e, 0x02, 0xc9
    };
    /* nop; ld iy, 5; call f; ld a, (iy+0); ld iy, 0; ret */
    static const unsigned char iy_kept[] = {
        0x00, 0xfd, 0x21, 0x05, 0x00, 0x00, 0xcd, 0x00, 0x00, 0x00,
        0xfd, 0x7e, 0x00, 0xfd, 0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; ld a, 5; call f; ld a, 1; ret */
    static const unsigned char before_call[] = {
        0x00, 0x3e, 0x05, 0xcd, 0x00, 0x00, 0x00, 0x3e, 0x01, 0xc9
    };
    /* nop; ld (ix-3), hl; ld hl, (ix-3); ld (de), a; ret */
    static const unsigned char reload[] = {
        0x00, 0xdd, 0x2f, 0xfd, 0xdd, 0x27, 0xfd, 0x12, 0xc9
    };
    /* nop; ld (ix-3), hl; ld (hl), a; ld hl, (ix-3); ret -- a store
     * between, which may be into the frame */
    static const unsigned char reload_after_store[] = {
        0x00, 0xdd, 0x2f, 0xfd, 0x77, 0xdd, 0x27, 0xfd, 0xc9
    };
    /* nop; jr +2; two bytes of data -- reti, were they read as code; ret */
    static const unsigned char data[] = { 0x00, 0x18, 0x02, 0xed, 0x4d, 0xc9 };
    /* nop; lea iy, ix+0; lea hl, ix+0; lea hl, iy+0; ld (hl), a; ret --
     * IY made IX, so HL holds IY already */
    static const unsigned char lea[] = {
        0x00, 0xed, 0x55, 0x00, 0xed, 0x22, 0x00, 0xed, 0x23, 0x00, 0x77, 0xc9
    };
    /* nop; ld hl, 5; or a, a; sbc hl, hl; ret -- HL made from the carry
     * alone, so the 5 is never read; the same of A by sbc a, a and xor a */
    static const unsigned char by_carry[] = {
        0x00, 0x21, 0x05, 0x00, 0x00, 0xb7, 0xed, 0x62, 0xc9
    };
    static const unsigned char a_by_carry[] = { 0x00, 0x3e, 0x05, 0x9f, 0xc9 };
    static const unsigned char a_zero[] = { 0x00, 0x3e, 0x05, 0xaf, 0xc9 };
    /* nop; push af; ld a, (iy+8); and a, (ix-34); pop de; or a, d;
     * ld de, 0; ret -- A kept in D while the other byte is made */
    static const unsigned char park_a[] = {
        0x00, 0xf5, 0xfd, 0x7e, 0x08, 0xdd, 0xa6, 0xde, 0xd1, 0xb2,
        0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* the same, but what is between reads D: A kept in E, then moved */
    static const unsigned char park_a_d_read[] = {
        0x00, 0xf5, 0x7a, 0xd1, 0xb2, 0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* and with every byte register read between -- add a, d ... add a, l
     * -- no room */
    static const unsigned char park_a_no_room[] = {
        0x00, 0xf5, 0x82, 0x83, 0x80, 0x81, 0x84, 0x85, 0xd1, 0xb2,
        0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; push af; ld b, 1; add a, c; add a, d; add a, e; add a, l;
     * add a, h; pop hl; or a, h; ld hl, 0; ret -- B written between, and
     * read after: not a place to keep A */
    static const unsigned char park_a_b_written[] = {
        0x00, 0xf5, 0x06, 0x01, 0x81, 0x82, 0x83, 0x85, 0x84, 0xe1, 0xb4,
        0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; push hl; ld hl, (ix+6); ld e, (hl); ld (hl), e; pop hl;
     * ld (hl), a; ld de, 0; ret -- E written between: DE is not free */
    static const unsigned char saved_de_written[] = {
        0x00, 0xe5, 0xdd, 0x27, 0x06, 0x5e, 0x73, 0xe1, 0x77,
        0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; push hl; <a load of HL>; ex de, hl; pop hl; ld (hl), e; ret --
     * the value wanted in DE, HL kept: the load made into DE */
    static const unsigned char de_nn_ind[] = {
        0x00, 0xe5, 0x2a, 0x34, 0x12, 0x00, 0xeb, 0xe1, 0x73, 0xc9
    };
    static const unsigned char de_nn[] = {
        0x00, 0xe5, 0x21, 0x34, 0x12, 0x00, 0xeb, 0xe1, 0x73, 0xc9
    };
    static const unsigned char de_ix[] = {
        0x00, 0xe5, 0xdd, 0x27, 0xfa, 0xeb, 0xe1, 0x73, 0xc9
    };
    static const unsigned char de_lea[] = {
        0x00, 0xe5, 0xed, 0x23, 0x05, 0xeb, 0xe1, 0x73, 0xc9
    };
    /* nop; ld hl, 7; push hl; ld hl, 6; ex de, hl; pop hl; add hl, de;
     * ld de, 29; add hl, de; ret -- case 020's: the ld de, 6 made is read */
    static const unsigned char de_read_after[] = {
        0x00, 0x21, 0x07, 0x00, 0x00, 0xe5, 0x21, 0x06, 0x00, 0x00, 0xeb,
        0xe1, 0x19, 0x11, 0x1d, 0x00, 0x00, 0x19, 0xc9
    };
    /* the same, but the pop is to BC: HL is not what comes back */
    static const unsigned char de_pop_bc[] = {
        0x00, 0xe5, 0x21, 0x34, 0x12, 0x00, 0xeb, 0xc1, 0x73, 0xc9
    };
    /* the same, but no ex de, hl: the value stays in HL */
    static const unsigned char de_no_ex[] = {
        0x00, 0xe5, 0x21, 0x34, 0x12, 0x00, 0x7d, 0xe1, 0x73, 0xc9
    };
    /* nop; push bc; pop hl; ld a, (hl); ld hl, 0; ret -- HL only the
     * address: ld a, (bc) */
    static const unsigned char via_bc[] = {
        0x00, 0xc5, 0xe1, 0x7e, 0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* the same, HL read after (by the ret) */
    static const unsigned char via_bc_hl_read[] = {
        0x00, 0xc5, 0xe1, 0x7e, 0xc9
    };
    /* nop; push de; pop hl; ld (hl), a; ld hl, 0; ret: ld (de), a */
    static const unsigned char via_de_store[] = {
        0x00, 0xd5, 0xe1, 0x77, 0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; lea hl, iy+5; ld a, (hl); ld hl, 0; ret: ld a, (iy+5) */
    static const unsigned char via_iy[] = {
        0x00, 0xed, 0x23, 0x05, 0x7e, 0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; lea hl, ix-3; ld (hl), e; ld hl, 0; ret: ld (ix-3), e */
    static const unsigned char via_ix_store[] = {
        0x00, 0xed, 0x22, 0xfd, 0x73, 0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; lea hl, iy+0; ld (hl), l; ld hl, 0; ret -- L, the address's own
     * byte: kept */
    static const unsigned char via_iy_l[] = {
        0x00, 0xed, 0x23, 0x00, 0x75, 0x21, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; ld hl, 0; add hl, bc; ld (hl), a; ret: push bc; pop hl */
    static const unsigned char zero_add[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0x09, 0x77, 0xc9
    };
    /* the same, the flags read after -- jr c past a nop: add hl, bc clears
     * the carry */
    static const unsigned char zero_add_flags[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0x09, 0x38, 0x01, 0x00, 0x77, 0xc9
    };
    /* nop; ld hl, 0; push hl; call f; ret: or a; sbc hl, hl */
    static const unsigned char zero_push[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0xe5, 0xcd, 0x00, 0x00, 0x00, 0xc9
    };
    /* the same, the flags read after the load: jr z past a nop */
    static const unsigned char zero_flags[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0x28, 0x01, 0x00, 0xe5, 0xc9
    };
    /* nop; ld hl, x the link fills in; push hl; ret: kept */
    static const unsigned char zero_linked[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0xe5, 0xc9
    };
    /* nop; ld hl, 0; push hl; pop de; ld de, 5; ld hl, 7; ret -- the load
     * made shorter while push hl reads it, then push hl / pop de taken out
     * (DE loaded again), and the load dead: cut whole, its trimmed byte
     * with it */
    static const unsigned char trim_then_dead[] = {
        0x00, 0x21, 0x00, 0x00, 0x00, 0xe5, 0xd1, 0x11, 0x05, 0x00, 0x00,
        0x21, 0x07, 0x00, 0x00, 0xc9
    };
    /* Tails: nop; jr z, B; jr A; L: ret; A: ... jp L; B: ... jp L -- the
     * jumps to L backward, so that neither is a jump to the next. */
    static const unsigned char tail_same[] = {
        0x00, 0x28, 0x0b, 0x18, 0x01, 0xc9, 0x3e, 0x01, 0x77, 0x23, 0xc3, 0x05,
        0x10, 0x00, 0x3e, 0x01, 0x77, 0x23, 0xc3, 0x05, 0x10, 0x00
    };
    static const unsigned char tail_one_byte[] = {
        0x00, 0x28, 0x09, 0x18, 0x01, 0xc9, 0x77, 0x23, 0xc3, 0x05, 0x10, 0x00,
        0x77, 0x23, 0xc3, 0x05, 0x10, 0x00
    };
    static const unsigned char tail_part[] = {
        0x00, 0x28, 0x0b, 0x18, 0x01, 0xc9, 0x3e, 0x01, 0x77, 0x23, 0xc3, 0x05,
        0x10, 0x00, 0x3e, 0x02, 0x77, 0x23, 0xc3, 0x05, 0x10, 0x00
    };
    static const unsigned char tail_calls[] = {
        0x00, 0x28, 0x0b, 0x18, 0x01, 0xc9, 0xcd, 0x00, 0x00, 0x00, 0xc3, 0x05,
        0x10, 0x00, 0xcd, 0x00, 0x00, 0x00, 0xc3, 0x05, 0x10, 0x00
    };
    /* the same as tail_same, 130 bytes between the two -- nop; jp z, B;
     * jr A; L: ret; ... -- so that no jr reaches */
    static const unsigned char tail_far[] = {
        0x00, 0xca, 0x92, 0x10, 0x00, 0x18, 0x01, 0xc9, 0x3e, 0x01, 0x77, 0x23,
        0xc3, 0x07, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x3e, 0x01, 0x77, 0x23, 0xc3, 0x07, 0x10, 0x00
    };
    /* A: ld a, 2; call f; jp L -- B: ld a, 3; call f; jp L: the calls not
     * jumped to, so the jr goes in the second's last two bytes */
    static const unsigned char tail_calls_after[] = {
        0x00, 0x28, 0x0d, 0x18, 0x01, 0xc9, 0x3e, 0x02, 0xcd, 0x00, 0x00, 0x00,
        0xc3, 0x05, 0x10, 0x00, 0x3e, 0x03, 0xcd, 0x00, 0x00, 0x00, 0xc3, 0x05,
        0x10, 0x00
    };
    /* tail_same with a jump into the second tail's inc hl as well: the run
     * taken out starts there, where the jr then is */
    static const unsigned char tail_into[] = {
        0x00, 0x28, 0x0d, 0x20, 0x0e, 0x18, 0x01, 0xc9, 0x3e, 0x01, 0x77, 0x23,
        0xc3, 0x07, 0x10, 0x00, 0x3e, 0x01, 0x77, 0x23, 0xc3, 0x07, 0x10, 0x00
    };
    /* nop; jr z, B; jr nz, C; jr A; A: tail; jp L; B: tail; jp L; C: ld a, 5;
     * L: jr 1 -- the two tails inside the loop L closes: a jr more on
     * every trip, so kept */
    static const unsigned char tail_looped[] = {
        0x00, 0x28, 0x0c, 0x20, 0x12, 0x18, 0x00, 0x3e, 0x01, 0x77, 0x23, 0xc3,
        0x19, 0x10, 0x00, 0x3e, 0x01, 0x77, 0x23, 0xc3, 0x19, 0x10, 0x00, 0x3e,
        0x05, 0x18, 0xe6
    };
    /* nop; jr -10: a jump to before the function -- left, not read */
    static const unsigned char jump_out[] = { 0x00, 0x18, 0xf6 };
    /* nop; push bc; ld a, b; pop bc; ret -- BC not written between */
    static const unsigned char saved_unwritten[] = {
        0x00, 0xc5, 0x78, 0xc1, 0xc9
    };
    /* nop; push hl; ld hl, (ix+6); ld (hl), a; pop hl; ld (hl), a;
     * ld de, 0; ret -- HL kept in DE across the code that uses HL */
    static const unsigned char saved_in_de[] = {
        0x00, 0xe5, 0xdd, 0x27, 0x06, 0x77, 0xe1, 0x77,
        0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* the same, but HL read between before it is written */
    static const unsigned char saved_read[] = {
        0x00, 0xe5, 0x23, 0x77, 0xe1, 0x77, 0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* the same, but DE read after: nop; push hl; ld hl, (ix+6);
     * ld (hl), a; pop hl; ld a, e; ret */
    static const unsigned char saved_de_read[] = {
        0x00, 0xe5, 0xdd, 0x27, 0x06, 0x77, 0xe1, 0x7b, 0xc9
    };
    /* nop; push hl; ld hl, (ix+6); pop de; add hl, de; ld de, 0; ret --
     * HL kept in DE by ex de, hl */
    static const unsigned char park_hl[] = {
        0x00, 0xe5, 0xdd, 0x27, 0x06, 0xd1, 0x19, 0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* the same, but what is between reads HL first: inc hl */
    static const unsigned char park_hl_read[] = {
        0x00, 0xe5, 0x23, 0xd1, 0x19, 0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; push af; ld a, 5; pop de; ld a, e; ret -- E, the flags, read */
    static const unsigned char park_a_e_read[] = {
        0x00, 0xf5, 0x3e, 0x05, 0xd1, 0x7b, 0xc9
    };
    /* nop; push hl; ld l, 5; pop de; add hl, de; ld de, 0; ret -- only L
     * written between, and H read after */
    static const unsigned char park_hl_part[] = {
        0x00, 0xe5, 0x2e, 0x05, 0xd1, 0x19, 0x11, 0x00, 0x00, 0x00, 0xc9
    };
    /* nop; reti */
    static const unsigned char unknown[] = { 0x00, 0xed, 0x4d };

    is("widening: read", run(widen, sizeof widen), 1);
    is("widening: ld a, (hl) kept", gone(0), 0);
    is("widening: ld l, a gone", gone(1), 1);
    is("widening: rlc l gone", gone(2), 1);
    is("widening: sbc hl, hl gone", gone(4), 1);
    is("widening: second ld l, a gone", gone(6), 1);
    is("widening: ld a, l (A holds it) gone", gone(7), 1);
    is("widening: ld (de), a kept", gone(8), 0);

    run(by_carry, sizeof by_carry);
    is("HL loaded before sbc hl, hl: gone", gone(1), 1);
    run(a_by_carry, sizeof a_by_carry);
    is("A loaded before sbc a, a: gone", gone(1), 1);
    run(a_zero, sizeof a_zero);
    is("A loaded before xor a, a: gone", gone(1), 1);

    run(jr_next, sizeof jr_next);
    is("a jump to the next instruction gone", gone(1), 1);

    run(moved, sizeof moved);
    is("push bc; pop hl, HL holding it: push gone", gone(7), 1);
    is("push bc; pop hl, HL holding it: pop gone", gone(8), 1);
    is("push hl; pop bc, BC then unread: push gone", gone(1), 1);
    is("push hl; pop bc, BC then unread: pop gone", gone(2), 1);
    is("the load between kept", gone(4), 0);

    run(twice, sizeof twice);
    is("ld hl, nn loaded again: gone", gone(7), 1);
    relocs[1] = 2;                  /* both operands the link's */
    relocs[2] = 8;
    out_reloc_put = relocs + 3;
    run(twice, sizeof twice);
    is("ld hl, nn the link fills in: kept", gone(7), 0);
    out_reloc_put = relocs + 1;

    run(landed, sizeof landed);
    is("a dead load jumped to: kept", gone(3), 0);
    is("the jump to it gone", gone(1), 1);
    is("the load after it, read: kept", gone(5), 0);

    run(iy_kept, sizeof iy_kept);
    is("IY loaded before a call, read after: kept", gone(1), 0);

    run(before_call, sizeof before_call);
    is("A loaded before a call to C, unread: gone", gone(1), 1);
    rts[0].at = BASE + 4;           /* the call's operand: the runtime */
    rt_fixups = rts;
    nrt_fixups = 1;
    run(before_call, sizeof before_call);
    is("A loaded before a call into the runtime: kept", gone(1), 0);
    nrt_fixups = 0;

    /* A routine of the runtime reads and changes what its code does
     * (rt_regs): acc_rt_mul reads HL and BC and keeps DE, so DE loaded
     * before it and not read after is dead -- and read after, is the
     * value it was; acc_rt_memcpy reads DE. Each after a nop: ld de, 5 /
     * call / [ex de, hl] / ret, in a function answering in HL. */
    {
        static const unsigned char de_call[] = {
            0x00, 0x11, 0x05, 0x00, 0x00, 0xcd, 0x00, 0x00, 0x00, 0xc9
        };
        static const unsigned char de_call_read[] = {
            0x00, 0x11, 0x05, 0x00, 0x00, 0xcd, 0x00, 0x00, 0x00, 0xeb, 0xc9
        };

        peep_answer = PEEP_PAIR;
        rts[0].at = BASE + 6;
        rt_fixups = rts;
        nrt_fixups = 1;
        rts[0].which = RT_MUL;
        run(de_call, sizeof de_call);
        is("DE before acc_rt_mul, unread after: gone", gone(1), 1);
        run(de_call_read, sizeof de_call_read);
        is("DE before acc_rt_mul, which keeps it, read after: kept", gone(1), 0);
        rts[0].which = RT_MEMCPY;
        run(de_call, sizeof de_call);
        is("DE before acc_rt_memcpy, which reads it: kept", gone(1), 0);
        rts[0].which = 0;
        nrt_fixups = 0;
        peep_answer = PEEP_ANY;
    }

    run(reload, sizeof reload);
    is("a frame slot loaded after its store: gone", gone(4), 1);
    is("the store kept", gone(1), 0);
    run(reload_after_store, sizeof reload_after_store);
    is("loaded after a store through HL: kept", gone(5), 0);

    is("data jumped over: read, not taken for code", run(data, sizeof data), 1);

    run(park_a, sizeof park_a);
    is("push af ... pop de: made ld d, a", code[1], 0x57);
    is("push af ... pop de: pop gone", gone(8), 1);
    run(park_a_d_read, sizeof park_a_d_read);
    is("push af ... pop de, D read between: ld e, a", code[1], 0x5f);
    is("push af ... pop de, D read between: ld d, e", code[3], 0x53);
    run(park_a_no_room, sizeof park_a_no_room);
    is("push af ... pop de, every byte read between: kept", code[1], 0xf5);
    run(park_a_b_written, sizeof park_a_b_written);
    is("push af ... pop hl, B written between: kept", code[1], 0xf5);
    run(saved_de_written, sizeof saved_de_written);
    is("push hl ... pop hl, E written between: kept", code[1], 0xe5);
    run(de_nn_ind, sizeof de_nn_ind);
    is("ld hl, (nn) into DE: ld de, (nn) over the push",
       code[1] << 8 | code[2], 0xed5b);
    is("ld hl, (nn) into DE: nn where it was", code[3] | code[4] << 8, 0x1234);
    is("ld hl, (nn) into DE: ex gone", gone(6), 1);
    is("ld hl, (nn) into DE: pop gone", gone(7), 1);
    run(de_nn, sizeof de_nn);
    is("ld hl, nn into DE: ld de, nn", code[2], 0x11);
    is("ld hl, nn into DE: push gone", gone(1), 1);
    run(de_ix, sizeof de_ix);
    is("ld hl, (ix+d) into DE: ld de, (ix+d)", code[3], 0x17);
    run(de_lea, sizeof de_lea);
    is("lea hl, iy+d into DE: lea de, iy+d", code[3], 0x13);
    run(de_read_after, sizeof de_read_after);
    is("ld hl, nn into DE, then read: ld de, nn", code[6], 0x11);
    is("ld hl, nn into DE, then read: kept", gone(6), 0);
    run(de_pop_bc, sizeof de_pop_bc);
    is("the same popped into BC: kept", code[2], 0x21);
    run(de_no_ex, sizeof de_no_ex);
    is("the same with no ex de, hl: kept", code[2], 0x21);
    run(via_bc, sizeof via_bc);
    is("push bc; pop hl; ld a, (hl): ld a, (bc)", code[3], 0x0a);
    is("push bc; pop hl; ld a, (hl): push gone", gone(1), 1);
    is("push bc; pop hl; ld a, (hl): pop gone", gone(2), 1);
    run(via_bc_hl_read, sizeof via_bc_hl_read);
    is("the same, HL read after: kept", code[3], 0x7e);
    run(via_de_store, sizeof via_de_store);
    is("push de; pop hl; ld (hl), a: ld (de), a", code[3], 0x12);
    run(via_iy, sizeof via_iy);
    is("lea hl, iy+5; ld a, (hl): ld a, (iy+5)",
       code[1] << 16 | code[2] << 8 | code[3], 0xfd7e05);
    is("lea hl, iy+5; ld a, (hl): the load gone", gone(4), 1);
    run(via_ix_store, sizeof via_ix_store);
    is("lea hl, ix-3; ld (hl), e: ld (ix-3), e",
       code[1] << 16 | code[2] << 8 | code[3], 0xdd73fd);
    run(via_iy_l, sizeof via_iy_l);
    is("lea hl, iy+0; ld (hl), l: kept", code[1] << 8 | code[2], 0xed23);
    run(zero_add, sizeof zero_add);
    is("ld hl, 0; add hl, bc: push bc", code[1], 0xc5);
    is("ld hl, 0; add hl, bc: pop hl", code[5], 0xe1);
    is("ld hl, 0; add hl, bc: the load's tail trimmed", trimmed(1), 3);
    run(zero_add_flags, sizeof zero_add_flags);
    is("the same, the carry read: kept", code[1], 0x21);
    run(zero_push, sizeof zero_push);
    is("ld hl, 0 alone: or a; sbc hl, hl",
       code[1] << 16 | code[2] << 8 | code[3], 0xb7ed62);
    is("ld hl, 0 alone: one byte trimmed", trimmed(1), 1);
    run(zero_flags, sizeof zero_flags);
    is("ld hl, 0, the flags read after: kept", code[1], 0x21);
    relocs[1] = 2;
    out_reloc_put = relocs + 2;
    run(zero_linked, sizeof zero_linked);
    is("ld hl, nn the link fills in: kept", code[1], 0x21);
    out_reloc_put = relocs + 1;
    memcpy(code, trim_then_dead, sizeof trim_then_dead);
    out_img = code;
    out_put = code + sizeof trim_then_dead;
    setenv("OPTACC_PEEP", "1", 1);
    peep_function(&start, BASE);
    is("a trimmed load then dead: one run cut", ncut_runs, 1);
    is("a trimmed load then dead: from it", ncut_runs ? cut_runs[0].at - BASE : -1, 1);
    is("a trimmed load then dead: six bytes, its trim too",
       ncut_runs ? cut_runs[0].len : -1, 6);
    unsetenv("OPTACC_PEEP");
    run(tail_same, sizeof tail_same);
    is("two tails the same: the second a jr", code[14], 0x18);
    is("two tails the same: its ld (hl), a cut", gone(16), 1);
    is("two tails the same: its inc hl cut", gone(17), 1);
    is("two tails the same: its jp cut", gone(18), 1);
    is("two tails the same: the first kept", gone(6) | gone(8) | gone(10), 0);
    run(tail_one_byte, sizeof tail_one_byte);
    is("a tail of one-byte instructions: jr over the first", code[12], 0x18);
    is("a tail of one-byte instructions: the next's byte taken", gone(13), 1);
    run(tail_part, sizeof tail_part);
    is("tails the same after ld a, n: jr at ld (hl), a", code[16], 0x18);
    is("tails the same after ld a, n: ld a, 2 kept", code[14], 0x3e);
    fixups[0].at = BASE + 7;
    fixups[0].fn = 5;
    fixups[1].at = BASE + 15;
    fixups[1].fn = 5;
    nfixups = 2;
    run(tail_calls, sizeof tail_calls);
    is("a tail's call jumped to: no jr where the link writes",
       code[14] == 0xcd && code[16] == 0x00, 1);
    fixups[0].at = BASE + 9;
    fixups[1].at = BASE + 19;
    run(tail_calls_after, sizeof tail_calls_after);
    is("tails calling the same function: the jr in the call's last two",
       code[20], 0x18);
    memcpy(code, tail_calls_after, sizeof tail_calls_after);
    out_img = code;
    out_put = code + sizeof tail_calls_after;
    setenv("OPTACC_PEEP", "1", 1);
    peep_function(&start, BASE);
    unsetenv("OPTACC_PEEP");
    is("and the call's head cut, its operand's slot in it",
       ncut_runs && cut_runs[0].at - BASE == 18 && cut_runs[0].len == 2, 1);
    fixups[0].at = BASE + 7;
    fixups[1].at = BASE + 15;
    fixups[1].fn = 6;
    fixups[0].at = BASE + 9;
    fixups[1].at = BASE + 19;
    run(tail_calls_after, sizeof tail_calls_after);
    is("tails calling different ones: kept", code[18] == 0xcd && code[20] == 0x00, 1);
    nfixups = 0;
    run(tail_far, sizeof tail_far);
    is("tails too far apart for a jr: kept", code[146], 0x3e);
    run(tail_into, sizeof tail_into);
    is("a tail jumped into: the jr where it lands", code[19], 0x18);
    is("a tail jumped into: what is before it kept", gone(16) | gone(18), 0);
    is("a jump to before the function: left", run(jump_out, sizeof jump_out), 0);
    run(tail_looped, sizeof tail_looped);
    is("tails in a loop: kept", code[15], 0x3e);
    run(saved_unwritten, sizeof saved_unwritten);
    is("push bc ... pop bc, BC not written: push gone", gone(1), 1);
    is("push bc ... pop bc, BC not written: pop gone", gone(3), 1);
    run(saved_in_de, sizeof saved_in_de);
    is("push hl ... pop hl, DE free: ex de, hl", code[1], 0xeb);
    is("push hl ... pop hl, DE free: ex de, hl back", code[6], 0xeb);
    run(saved_read, sizeof saved_read);
    is("push hl ... pop hl, HL read first: kept", code[1], 0xe5);
    run(saved_de_read, sizeof saved_de_read);
    is("push hl ... pop hl, DE read after: kept", code[1], 0xe5);
    run(park_a_e_read, sizeof park_a_e_read);
    is("push af ... pop de, E read after: kept", code[1], 0xf5);
    run(park_hl_part, sizeof park_hl_part);
    is("push hl ... pop de, HL partly written: kept", code[1], 0xe5);
    run(park_hl, sizeof park_hl);
    is("push hl ... pop de: made ex de, hl", code[1], 0xeb);
    is("push hl ... pop de: pop gone", gone(5), 1);
    run(park_hl_read, sizeof park_hl_read);
    is("push hl ... pop de, HL read between: kept", code[1], 0xe5);

    run(lea, sizeof lea);
    is("lea hl, iy+0 with HL holding IY: gone", gone(7), 1);
    is("an instruction not known: left", run(unknown, sizeof unknown), 0);

    /* Each after a nop: what a function begins with is something jumped
     * to, which no rule touches.
     *
     * A member read through a pointer in the frame, in a function whose
     * answer is in HL: ld hl, (ix+6) / ld de, 3 / add hl, de / ld hl, (hl)
     * / ret made ld iy, (ix+6) / ld hl, (iy+3) / ret -- DE and IY no
     * return reads, the constant a displacement. */
    {
        static const unsigned char member[] = {
            0x00, 0xdd, 0x27, 0x06, 0x11, 0x03, 0x00, 0x00, 0x19, 0xed, 0x27, 0xc9
        };
        static const unsigned char member_e[] = {      /* ld (hl), e */
            0x00, 0xdd, 0x27, 0x06, 0x11, 0x03, 0x00, 0x00, 0x19, 0x73, 0xc9
        };
        static const unsigned char member_far[] = {    /* past a displacement */
            0x00, 0xdd, 0x27, 0x06, 0x11, 0x80, 0x00, 0x00, 0x19, 0xed, 0x27, 0xc9
        };
        static const unsigned char local_member[] = {  /* lea hl, ix-10 / +4 */
            0x00, 0xed, 0x22, 0xf6, 0x11, 0x04, 0x00, 0x00, 0x19, 0xed, 0x27, 0xc9
        };
        static const unsigned char de_left[] = {       /* ld de, 5 / ret */
            0x00, 0x21, 0x01, 0x00, 0x00, 0x11, 0x05, 0x00, 0x00, 0xc9
        };
        static const unsigned char rlc_carry[] = {     /* scf / rlc l / ret */
            0x00, 0x37, 0xcb, 0x05, 0xc9
        };

        peep_answer = PEEP_PAIR;
        run(member, sizeof member);
        is("p->member: ld iy, (ix+6)", code[2], 0x31);
        is("p->member: ld hl, (iy+3)", code[4] == 0xfd && code[5] == 0x27 && code[6] == 0x03, 1);
        is("p->member: the load trimmed a byte", trimmed(4), 1);
        is("p->member: add hl, de and ld hl, (hl) gone", gone(8) && gone(9), 1);
        peep_answer = PEEP_VOID;        /* HL not read after: only E stops it */
        run(member_e, sizeof member_e);
        is("p->member stored from E, the constant's: kept", code[2], 0x27);
        peep_answer = PEEP_PAIR;
        run(member_far, sizeof member_far);
        is("p->member past (iy+d)'s reach: kept", code[2], 0x27);
        peep_answer = PEEP_LONG;
        run(member, sizeof member);
        is("p->member, E returned: kept", code[2], 0x27);
        peep_answer = PEEP_ANY;
        run(member, sizeof member);
        is("p->member, what returns read not known: kept", code[2], 0x27);
        peep_answer = PEEP_PAIR;
        run(local_member, sizeof local_member);
        is("a local's member: lea hl, ix-6", code[3], 0xfa);
        is("a local's member: ld de and add hl, de gone", gone(4) && gone(8), 1);
        run(de_left, sizeof de_left);
        is("DE loaded, an int returned: gone", gone(5), 1);
        peep_answer = PEEP_LONG;
        run(de_left, sizeof de_left);
        is("DE loaded, a long returned: kept", gone(5), 0);
        /* exit's way out, a jump through the stack to the startup with
         * the status in HL: the ret is not the function's, and HL kept,
         * in a function that returns nothing. */
        {
            static const unsigned char exit_jump[] = {
                0x00, 0x21, 0x00, 0x00, 0x00, 0xd5, 0xc9    /* ld hl, 0 / push de / ret */
            };

            peep_answer = PEEP_VOID;
            run(exit_jump, sizeof exit_jump);
            is("ld hl before push de / ret, a void function: kept", gone(1), 0);
        }
        peep_answer = PEEP_PAIR;
        run(rlc_carry, sizeof rlc_carry);
        is("scf before rlc, which reads no carry: gone", gone(1), 1);
        peep_answer = PEEP_ANY;
    }

    fprintf(stderr, "  %d of %d held\n", checks - failures, checks);

    return failures != 0;
}
