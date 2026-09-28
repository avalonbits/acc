/*
 * The runtime's helpers: which a program uses, calls to them, and laying
 * down the ones used, and only those, at the end of the program.
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
/* the runtime helpers                                                 */

/* The operations the chip has no instruction for. acc has nothing to link
 * against, so it carries them and drops the ones a program uses into that
 * program's image -- see src/rt/helpers.s, which is where they are written
 * and read. A program that uses none pays nothing.
 *
 * Calls to them are recorded like calls to a function defined further down
 * the file, because that is what they are: the address is not known until the
 * end, when the runtime is laid out after the last function.
 *
 * It goes in whole rather than a routine at a time, because the routines
 * share code -- the four ways of dividing are one loop with four ways in --
 * and splitting them would mean four copies of that loop.
 *
 * So it goes in by groups, which src/rt/embed.py finds: runs of the blob
 * that nothing outside jumps into relatively and that fall into nothing
 * after them, each with the groups its calls reach. A program carries the
 * groups it uses and the ones those need, laid down in the blob's order,
 * and the calls between them are relocated to where they went. Carried as
 * a prefix of the blob, as it was, a program that multiplied once carried
 * every floating-point routine, which sat before the long long ones: more
 * than a third of a small program's image. */
static int rt_base = 0;                 /* where the blob landed */

/* The groups something has wanted, as flags and as the order they were
 * first wanted in, so that gen_restore can take back the ones wanted since
 * a mark: the log is cut back to the length the mark saw. */
static unsigned char rt_group_used[RT_NGROUPS];
static unsigned char rt_used_log[RT_NGROUPS];
int           rt_nused;
static short         rt_new_at[RT_NGROUPS];   /* where each landed, or -1 */

void rt_unwant_to(int n)
{
    while (rt_nused > n)
        rt_group_used[rt_used_log[--rt_nused]] = 0;
}

RtFixup *rt_fixups;
int      nrt_fixups, rt_fixups_cap;

/* The symbol each helper is known by, when something has had to name one:
 * SYM_NONE until then.
 *
 * A file compiled to an object does not carry the blob -- one copy of it per
 * object is one copy too many -- so a call to a helper leaves the object as a
 * call to `acc_rt_mul` and the like, and the link resolves them all against
 * the single copy it lays down. Which is what a helper always was: a function
 * the program calls and does not define. */
int rt_syms[RT_COUNT];

void rt_syms_init(void)
{
    int i;

    for (i = 0; i != RT_COUNT; i++)
        rt_syms[i] = SYM_NONE;
}

/* The helper of that name, or -1. Asked of every name a link cannot resolve,
 * which is a handful, so the names are walked rather than hashed. */
int rt_which(const char *name)
{
    int i;

    for (i = 0; i != RT_COUNT; i++)
        if (strcmp(rt_name[i], name) == 0)
            return i;

    return -1;
}

/* That something wants this helper, whatever laid the want down: a call
 * emitted here, or a name that came in from an object. */
void rt_wanted(int which)
{
    int g = rt_entry_group[which];

    if (!rt_group_used[g]) {
        rt_group_used[g] = 1;
        rt_used_log[rt_nused++] = (unsigned char) g;
    }
}

/* Its symbol, made the first time one is asked for. Only at the end of a
 * compile, where pushing a file-scope symbol cannot move a Sym * that
 * something is holding. */
int rt_symbol(int which)
{
    if (rt_syms[which] == SYM_NONE) {
        int sym = sym_push(name_intern(rt_name[which],
                                       (int) strlen(rt_name[which])),
                           SYM_FUNC, 0);

        sym_set_flags(sym, SYMF_DECLARED | SYMF_PARAMS);
        rt_syms[which] = sym;
    }

    return rt_syms[which];
}

/* The operators with no instruction behind them. */
int needs_helper(int op)
{
    switch (op) {
    case TK_AMP: case TK_PIPE: case TK_CARET:
    case TK_SHL: case TK_SHR:
    case TK_STAR: case TK_SLASH: case TK_PERCENT:
        return 1;
    }

    return 0;
}

void rt_call(int which)
{
    if (nrt_fixups == rt_fixups_cap) {
        rt_fixups_cap = rt_fixups_cap ? rt_fixups_cap * 2 : 16;
        rt_fixups = realloc(rt_fixups, rt_fixups_cap * sizeof *rt_fixups);
        if (!rt_fixups)
            acc_error("out of memory for the runtime fixups");
    }
    rt_wanted(which);
    out_opcode24(0xcd, 0);                       /* call nn */
    rt_fixups[nrt_fixups].which = (unsigned char) which;
    rt_fixups[nrt_fixups].at = out_here() - ACC_INT_SIZE;
    out_reloc(rt_fixups[nrt_fixups].at);
    nrt_fixups++;
}

/* Where a place in rt_code, in group g, went in the image: the group's new
 * start, and as far into it as it was into the group. The group is always
 * known from a table, the entry's or the call's: every function's prologue
 * is a call into the runtime, and walking the groups for each was more
 * than a twentieth of a compile. */
static int rt_moved(int g, int at)
{
    return rt_base + rt_new_at[g] + (at - rt_group_start[g]);
}

static int rt_entry_moved(int which)
{
    return rt_moved(rt_entry_group[which], rt_entry[which]);
}

/* The ways into the blob that were laid down, for -map on a link: each runs
 * to the next one in its group, or to the group's end. */
__attribute__((noinline))
static void rt_map(void)
{
    int i, j;

    for (i = 0; i != RT_COUNT; i++) {
        int g = rt_entry_group[i], end = rt_group_start[g + 1];

        if (rt_new_at[g] < 0)
            continue;
        for (j = 0; j != RT_COUNT; j++)
            if (rt_entry[j] > rt_entry[i] && rt_entry[j] < end)
                end = rt_entry[j];
        obj_link_map_item(rt_entry_moved(i), end - rt_entry[i],
                          rt_name[i], "(runtime)", rt_entry[i]);
    }
}

/* The blob, once, wherever the image has got to -- which is after everything
 * else, since this is the last thing written. Every call to a helper is then
 * pointed at it: the ones this compile emitted directly, and the ones that
 * arrived as a name from an object. */
void rt_emit_used(void)
{
    unsigned char need[RT_NEED_BYTES];
    int i, g, len = 0;

    /* What is wanted, asked of the calls as they stand now: a function's
     * prologue is retargeted to acc_rt_frameset0 once it is known to have no
     * frame, and a function taken out calls nothing. And the helpers an
     * object named, which have a symbol. */
    rt_unwant_to(0);
    for (i = 0; i != nrt_fixups; i++)
        rt_wanted(rt_fixups[i].which);
    for (i = 0; i != RT_COUNT; i++)
        if (rt_syms[i] != SYM_NONE)
            rt_wanted(i);
    if (!rt_nused)
        return;

    /* The groups wanted and everything they need, in the blob's order. */
    memset(need, 0, sizeof need);
    for (i = 0; i < rt_nused; i++)
        for (g = 0; g != RT_NEED_BYTES; g++)
            need[g] |= rt_group_needs[rt_used_log[i]][g];
    rt_base = out_here();
    for (g = 0; g != RT_NGROUPS; g++) {
        rt_new_at[g] = -1;
        if (!(need[g >> 3] & (1 << (g & 7))))
            continue;
        rt_new_at[g] = (short) len;
        for (i = rt_group_start[g]; i < rt_group_start[g + 1]; i++)
            out_byte(rt_code[i]);
        len += rt_group_start[g + 1] - rt_group_start[g];
    }

    /* The calls the routines make to each other, now that each has an
     * address -- those in the groups that were laid down, whose targets
     * were laid down with them. */
    for (i = 0; i != RT_NFIX; i++) {
        int at;

        if (rt_new_at[rt_fix_group_at[i]] < 0)
            continue;
        at = rt_moved(rt_fix_group_at[i], rt_fix[i].at);
        out_reloc(at);
        out_patch24(at, rt_moved(rt_fix_group_to[i], rt_fix[i].to));
    }

    /* And the calls the compiled program makes to them. */
    for (i = 0; i != nrt_fixups; i++)
        out_patch24(rt_fixups[i].at, rt_entry_moved(rt_fixups[i].which));

    if (obj_link_map_on())
        rt_map();

    /* The ones that came in by name have a symbol, and the fixups waiting on
     * it are filled in with the rest of them below. */
    for (i = 0; i != RT_COUNT; i++)
        if (rt_syms[i] != SYM_NONE && rt_new_at[rt_entry_group[i]] >= 0) {
            sym_at(rt_syms[i])->val = rt_entry_moved(i);
            sym_set_flags(rt_syms[i], SYMF_DEFINED);
        }
}
