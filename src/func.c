/*
 * Functions: locals and the frame, the stack, beginning and ending a
 * function, returning from one, and calling one.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "runtime.h"
#include "gen_int.h"

static void call_through(void);
#ifdef OPT_ACC
static int func_start;          /* the function's first byte, for peep.c */
#endif

#ifdef OPT_ACC
/* The locals as a stack: an inlined call's body gives its room back where
 * it ends, for what comes after to take again. locals_size is the most
 * they ever took, and the spills are below that. */
static int local_depth, scope_marks[32], nscopes;

void gen_local_scope(int open)
{
    if (open) {
        if (nscopes < 32)
            scope_marks[nscopes] = local_depth;
        nscopes++;

        return;
    }
    if (--nscopes < 32)
        local_depth = scope_marks[nscopes];
}

/* From here every local below all the room taken: for values that live
 * through the whole function, past where any body's locals were. */
void gen_local_settle(void)
{
    local_depth = locals_size;
}

int gen_local(int size)
{
    local_depth += size;
    if (local_depth > locals_size)
        locals_size = local_depth;

    return -local_depth;
}
#else
int gen_local(int size)
{
    locals_size += size;

    return -locals_size;
}
#endif

/* How many bytes of declared locals are kept where (ix+d) can reach them.
 *
 * (ix+d) reaches 128 bytes below the frame pointer, and the scratch area
 * shares them with the locals -- which is what used to make a function with
 * a few dozen variables in it refuse to compile at all.
 *
 * The line is drawn from measurement, and drawn so that nothing that
 * compiles today gets slower. Over the 1012 functions in acc's own source,
 * its library and its test cases, the most any of them declares is 84 bytes
 * of locals, and the scratch -- now that its slots are reused -- wants four
 * on average. So the locals keep 96 of the 128 and the scratch has the rest:
 * every function in that corpus keeps every one of its locals in reach of
 * (ix+d), which is the fast way to touch one and the only reason to have a
 * frame pointer at all.
 *
 * What a function declares past 96 goes where the arrays and the structs
 * already go, and is reached by a computed address -- ten bytes and a couple
 * of dozen cycles on every use of it, against the alternative, which was to
 * refuse to compile the function at all. A function whose scratch then wants
 * more than the 32 left over is still refused, which is what it was before;
 * that is the remaining half of this, and it is the scratch's half. */
#define NEAR_LOCALS 96

int gen_local_fits(int size)
{
#ifdef OPT_ACC
    return local_depth + size <= NEAR_LOCALS;
#else
    return locals_size + size <= NEAR_LOCALS;
#endif
}

int gen_local_far(int size)
{
    int array = gen_local_array();

    gen_local_array_size(array, size);

    return array;
}

/* The stack, as an array whose length only the program knows.
 *
 * The frame itself is reached through ix and does not move, so taking room
 * off the stack costs nothing but sp: the bytes are below everything the
 * frame holds, and `ld sp, ix` in the epilogue gives them all back at once.
 * What these three add is giving them back earlier -- at the end of the
 * block that took them, so that a loop does not take the room again on
 * every turn. */
void gen_stack_take(int slot)
{
    save_regs_below(1);
    force_into(vsp - 1, R_HL);          /* the byte count */
    vdrop();
    evict_reg(R_DE);
    ex_de_hl();
    ld_rr_imm(R_HL, 0);
    out_byte(0x39);                     /* add hl, sp */
    or_a_a();
    sbc_hl_rr(R_DE);                    /* hl = sp - bytes */
    out_byte(0xf9);                     /* ld sp, hl */
    ld_ix_rr(slot, R_HL);               /* and that is where the array is */
}

void gen_stack_mark(int slot)
{
    evict_reg(R_HL);
    ld_rr_imm(R_HL, 0);
    out_byte(0x39);                     /* add hl, sp */
    ld_ix_rr(slot, R_HL);
}

void gen_stack_back(int slot)
{
    evict_reg(R_HL);
    ld_rr_ix(R_HL, slot);
    out_byte(0xf9);                     /* ld sp, hl */
}

int gen_local_array(void)
{
    if (narrays == array_end_cap) {
        array_end_cap = array_end_cap ? array_end_cap * 2 : 16;
        array_end = realloc(array_end, (size_t) array_end_cap * sizeof *array_end);
        if (!array_end)
            acc_error("out of memory for local arrays");
    }
    array_end[narrays] = arrays_size;

    return narrays++;
}

/* Called once per array, before the next one is declared. */
void gen_local_array_size(int array, int size)
{
    arrays_size += size;
    array_end[array] = arrays_size;
}

/* The address of a local array's first element, in HL:
 *
 *   push de / ld de, K / push ix / pop hl / add hl, de / pop de
 *
 * with K patched when the function ends. DE is kept, because whatever it
 * holds may still be wanted. */
void vaddr_array(int array, Type elem)
{
    if (type_ptr_depth(elem) == TY_PTR_MAX)
        acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                     TY_PTR_MAX);

    evict_reg(R_HL);
    out_byte2(0xd5, 0x11);               /* push de; ld de, nn */
    if (narray_patches == array_patches_cap) {
        array_patches_cap = array_patches_cap ? array_patches_cap * 2 : 16;
        array_patches = realloc(array_patches,
                                (size_t) array_patches_cap * sizeof *array_patches);
        if (!array_patches)
            acc_error("out of memory for local arrays");
    }
    array_patches[narray_patches].at = out_here();
    array_patches[narray_patches].array = array;
    narray_patches++;
    out_word24(0);
    out_byte2(0xdd, 0xe5);               /* push ix */
    out_byte3(0xe1, 0x19, 0xd1);         /* pop hl; add hl, de; pop de */
    vpush(VAL_REG, type_ptr_to(elem), R_HL);
}

/* A string literal's bytes, and the terminator, somewhere in the image:
 * returns where. Its address is known the moment it is written, which is
 * what lets a string be an ordinary constant pointer.
 *
 * At file scope nothing runs between the declarations, so the bytes go where
 * the output is. Inside a function the code is running through here, so
 * they are jumped over: four bytes of code and a jump taken each time, which
 * is what writing them where they are read costs -- the alternative, a pool
 * after the code with every use patched, makes the address unknown until the
 * end and a string no longer a constant. */
int gen_data_bytes;

int gen_data(const char *bytes, int len)
{
    int over = in_function ? gen_jump() : -1, at, i;

    at = out_here();
    for (i = 0; i < len; i++)
        out_byte((unsigned char) bytes[i]);
    out_byte(0);
    gen_data_bytes += len + 1;
    if (over >= 0)
        gen_label(over);

    return at;
}

/* `count` bytes from address `from` to a local array at `offset`: a string
 * copied into the char array it initialises. HL, DE and BC are all written,
 * so whatever they hold goes to the frame first: an initialiser is at a
 * declaration, but a compound literal's is inside an expression -- `t =
 * (T) { ... }` -- where one of them may hold t's address.
 *
 *   (the array's address in HL) / ex de, hl / ld hl, from / ld bc, count / ldir */
void gen_copy_to_array(int array, int offset, int from, int count)
{
    save_regs_below(0);
    vaddr_array(array, TY_CHAR);
    vdrop();
    if (offset) {
        out_byte(0x11);                  /* ld de, offset */
        out_word24(offset);
        out_byte(0x19);                  /* add hl, de */
    }
    out_byte(0xeb);                      /* ex de, hl */
    out_byte(0x21);                      /* ld hl, from */
    out_reloc(out_here());
    out_word24(from);
    out_byte(0x01);                      /* ld bc, count */
    out_word24(count);
    out_byte2(0xed, 0xb0);               /* ldir */
}

/* Bytes [from, from + size) of a local array set to zero, which is what the
 * elements an initialiser does not mention start as: the first byte cleared,
 * and ldir copying it along the rest. The registers go to the frame first,
 * for gen_copy_to_array's reason. */
void gen_zero_array(int array, int from, int size)
{
    save_regs_below(0);
    vaddr_array(array, TY_CHAR);
    vdrop();
    if (from) {
        out_byte(0x11);                  /* ld de, from */
        out_word24(from);
        out_byte(0x19);                  /* add hl, de */
    }
    out_byte2(0x36, 0x00);               /* ld (hl), 0 */
    if (size > 1) {
        out_byte3(0xe5, 0xd1, 0x13);     /* push hl; pop de; inc de */
        out_byte(0x01);                  /* ld bc, nn */
        out_word24(size - 1);
        out_byte2(0xed, 0xb0);           /* ldir */
    }
}

/* Where the scratch area is free from: past the end of everything in it that
 * is still wanted.
 *
 * What a spilled register holds is on the value stack -- that is why it was
 * spilled -- so a slot the stack does not point at is nobody's. The slots
 * used to be handed out in order and all released together at the end of the
 * statement, which made the area as large as everything a statement ever
 * spilled rather than as large as it holds at its worst moment. One call
 * with six arguments and a `?:` in each wanted 55 bytes that way, a function
 * full of them 580, and (ix+d) reaches 128: that is what made a function
 * with a few dozen variables in it refuse to compile.
 *
 * spill_locked is for the one thing that lives in the scratch without the
 * stack saying so: the slot a `?:` parks its middle operand in has to
 * survive the compiling of the third operand, and nothing on the stack
 * points at it while that happens. */
/*
 * And the slots handed out that the stack does not point at yet. A slot is
 * taken before the value is built in it, and building it may need a
 * register that is holding something -- which is spilled, to a slot of its
 * own, while the stack still has no word of the first one. -inf < x as an
 * argument after another comparison spilled the first answer into the
 * float being negated. So such a slot stays reserved until vpush_scratch
 * puts it on the stack, or the statement ends. */
static int spill_free_from(void)
{
    int floor = spill_locked, i, size;

    for (i = 0; i < vtop; i++) {
        int start = spill_start_of(vstack + i, &size);

        if (start >= 0 && start + size > floor)
            floor = start + size;
    }
    for (i = 0; i != nspill_pending; i++)
        if (spill_pending[i].end > floor)
            floor = spill_pending[i].end;

    return floor;
}

static int spill_take(int size)
{
    int start = spill_free_from();

    spill_used = start + size;
    if (spill_used > spill_peak)
        spill_peak = spill_used;

    return -(locals_size + spill_used);
}

/* Room in the scratch area for the parameters of a call being inlined,
 * held for as long as the body is compiled: the value stack does not point
 * at it, the parameters' names do, so it is locked as a `?:` locks its
 * middle operand's slot. Returns where it starts; `*lock` is what
 * gen_inline_end puts back. */
int gen_inline_begin(int size, int *lock)
{
    int disp;

    *lock = spill_locked;
    disp = spill_take(size);
    spill_locked = spill_used;
    gen_effects++;                      /* it is still a call */

    return disp;
}

void gen_inline_end(int lock)
{
    spill_locked = lock;
}

/* A slot for a value about to be built in it and then pushed. It is
 * reserved until it is pushed: see vpush_scratch. */
int spill_slot_of(int size)
{
    int disp = spill_take(size);

    /* Full: the oldest gives way. It has not happened; four is two more
     * than any operator here holds at once. */
    if (nspill_pending == SPILL_PENDING) {
        memmove(spill_pending, spill_pending + 1,
                (SPILL_PENDING - 1) * sizeof *spill_pending);
        nspill_pending--;
    }
    spill_pending[nspill_pending].disp = disp;
    spill_pending[nspill_pending].end = spill_used;
    nspill_pending++;

    return disp;
}

/* A slot for a register being spilled, whose value is on the stack the
 * moment it is stored: nothing to reserve. */
int spill_slot(void)
{
    return spill_take(ACC_INT_SIZE);
}

/* A scratch slot's value pushed, and so on the stack, which now says the
 * slot is taken: its reservation is done with. */
void vpush_scratch(Type type, int slot)
{
    unsigned char i;

    vpush(VAL_LOCAL, type, slot);
    for (i = 0; i != nspill_pending; i++)
        if (spill_pending[i].disp == slot) {
            spill_pending[i] = spill_pending[--nspill_pending];

            return;
        }
}

/* What gen_stmt_end frees, but only as far as nothing still wants it: the
 * end of a value inside an expression rather than of a statement. An
 * initialiser's elements end this way, and a compound literal's initialiser
 * is inside whatever expression it appears in -- the third operand of a `?:`,
 * say, whose middle is parked in a slot nothing on the stack points at.
 * Freeing all of it there let the rest of that operand build over the slot,
 * and `n > 1 ? f8() : (int){ n } ? ...` answered with what it had built. */
void gen_value_end(void)
{
    const Value *v;

    spill_used = spill_free_from();
    nwide_consts = 0;
    for (v = vstack; v < vsp; v++)
        if (v->kind == VAL_WIDE && v->val >= nwide_consts)
            nwide_consts = v->val + 1;
}

/* Every return jumps to the one epilogue at the function's end, rather
 * than each writing its own: a jump is two bytes once it is shortened, and
 * ld sp, ix; pop ix; ret is five -- a function has four and a half
 * returns, on zap. The jumps are chained through their holes, as patch_to
 * reads them, and the chain is patched when the end is reached. */
/* The constants this function has returned, and where each return's code
 * began and ended: `return 0;` a third time is a jump to the first, two
 * bytes when it is near, where the load, the byte in A and the jump to the
 * epilogue were nine. One that a rewind has reached into since is dropped:
 * out_rewind_floor says how far back the image has been taken. */
#define CONST_RETS 8
static int const_ret_val[CONST_RETS], const_ret_at[CONST_RETS];
static int const_ret_end[CONST_RETS];
static int nconst_rets;

static int      return_chain;
static unsigned return_epoch;           /* out_rewinds at the last one */

static void return_jump(void)
{
    int hole = jump_op(JP_ANY);

    put24(out_img + (hole - out_base), return_chain);
    return_chain = hole;
    return_epoch = out_rewinds;
}

#ifdef OPT_ACC
static int func_sym;            /* opt-acc: the function being made */
#endif
int frame_unused, frame_sp_kept; /* see gen_int.h */

#ifndef OPT_ACC
/* Whether the code from `from` to `to` reads or writes IX: an instruction
 * of it has the DD prefix, or is lea or pea from IX -- or a byte among them
 * looks like one, a string's or a block static's, which costs only the
 * frame it keeps. Not the bytes of an address, a jump's or one relocated:
 * those are where the image went, and an object's are not where a
 * program's are, which would keep the frame in one and not the other. */
static int ix_read(int from, int to)
{
    const int *r = out_relocs + 1 + func_mark.reloc, *r_end = out_reloc_put;
    const int *j = jump_at + func_mark.jump, *j_end = jump_at + njumps;
    unsigned at = (unsigned) (from - out_base), end = (unsigned) (to - out_base);
    unsigned jbase = (unsigned) out_base;

    for (; at != end; at++) {
        const unsigned char *p = out_img + at;

        while (r != r_end && (unsigned) *r + 3 <= at)
            r++;
        while (j != j_end && (unsigned) *j - jbase + 4 <= at)
            j++;
        if ((r != r_end && (unsigned) *r <= at)
            || (j != j_end && (unsigned) *j - jbase + 1 <= at))
            continue;                   /* an address's bytes */
        if (*p == 0xdd)
            return 1;
        if (*p == 0xed && at + 1 != end)
            switch (p[1]) {
            case 0x02: case 0x12: case 0x22: case 0x32:   /* lea rr, ix+d */
            case 0x54: case 0x55: case 0x65:    /* lea ix/iy, pea ix+d */
                return 1;
            }
    }

    return 0;
}
#endif

void gen_func_begin(int fn, int nparams, Type returns)
{
    return_type = returns;
    return_ext = sym_at(fn)->ext;

    (void) nparams;

    sym_at(fn)->val = out_here();
#ifdef OPT_ACC
    func_start = out_here();
    func_sym = fn;
    frame_unused = frame_sp_kept = 0;
#endif
    nconst_rets = 0;
    npool = npool_sites = 0;
    out_on_rewind = gen_rewound;
    static_begin(fn);
    mark_here(&func_mark);
    vtop = 0;
    vsp = vstack;
    locals_size = 0;
#ifdef OPT_ACC
    local_depth = nscopes = 0;
#endif
    spill_used = 0;
    spill_peak = 0;
    spill_locked = 0;
    nspill_pending = 0;
    arrays_size = 0;
    narrays = 0;
    narray_patches = 0;
    iy_local = 0;
    in_function = 1;

    /* ld hl, -frame / call acc_rt_frameset: the frame agondev's __frameset
     * builds -- IX saved and pointed at, room made below -- and in eight
     * bytes rather than the thirteen of writing it out, since every
     * function has one. IX then points at the saved IX, so the first
     * argument is at ix+6: three bytes of saved IX and three of return
     * address. The size is not known until the body has been read, so it
     * is filled in at the end, and a function with no frame has the load
     * taken out and calls acc_rt_frameset0 instead. */
#ifdef OPT_ACC
    /* opt-acc writes the frame out instead -- push ix; ld ix, 0; add ix,
     * sp; ld hl, -frame; add hl, sp; ld sp, hl -- fifteen bytes and no
     * call, about 15 cycles less on every entry. With no frame the last
     * six are cut (frame_cut), leaving nine -- or, where gen_func_end
     * would rather, made a call as acc's is (frame_wants_call). */
    out_word24(0xdde5dd);                       /* push ix; ld ix, */
    out_word24(0x000021);                       /*   0 */
    out_word24(0x39dd00);                       /* ; add ix, sp */
    out_byte(0x21);                             /* ld hl, nn */
    frame_patch = out_here();
    out_word24(0);
    out_byte2(0x39, 0xf9);                      /* add hl, sp; ld sp, hl */
    frame_call = -1;
#else
    out_byte(0x21);                              /* ld hl, nn */
    frame_patch = out_here();
    out_word24(0);
    frame_call = nrt_fixups;
    rt_call(RT_FRAMESET);
#endif
}

static void gen_forget(void);

#ifdef OPT_ACC
/* opt-acc's prologue, its frame known: with none, the ld hl and the two
 * after it cut; with up to 128 bytes, ld hl, -frame / add hl, sp made
 * lea hl, ix-frame -- IX is SP there -- three bytes for five, and the two
 * left over cut. Where relax_function cuts, and how much (frame_cut_len),
 * or -1. */
static int frame_lea(void)
{
    int size = frame_size();
    unsigned char *op = out_img + (frame_patch - 1 - out_base);

    if (!size) {
        frame_cut_len = 6;

        return frame_patch - 1;
    }
    if (size > 128)
        return -1;
    op[0] = 0xed;                       /* lea hl, ix-frame */
    op[1] = 0x22;
    op[2] = (unsigned char) -size;
    frame_cut_len = 2;

    return frame_patch + 2;
}
#endif

/* Whether opt-acc's prologue is made a call to acc_rt_frameset, as acc's
 * is: four or five bytes smaller -- seven with a frame past (ix+d)'s
 * reach -- and some fifteen cycles more on every entry. */
#ifdef OPT_ACC
/* The functions said hot, by name: a set, open addressing over a table
 * twice as big as it is full. */
static NameRef *hot_names;
static int hot_n, hot_cap;

static unsigned hot_slot(NameRef name, int cap)
{
    unsigned at = (unsigned) name * 2654435761u & (unsigned) (cap - 1);

    while (hot_names[at] && hot_names[at] != name)
        at = (at + 1) & (unsigned) (cap - 1);

    return at;
}

void hot_add(NameRef name)
{
    unsigned at;

    if (2 * (hot_n + 1) > hot_cap) {
        NameRef *old = hot_names;
        int old_cap = hot_cap, k;

        hot_cap = hot_cap ? 2 * hot_cap : 16;
        hot_names = calloc((size_t) hot_cap, sizeof *hot_names);
        if (!hot_names)
            acc_error("out of memory for symbols");
        for (k = 0; k != old_cap; k++)
            if (old[k])
                hot_names[hot_slot(old[k], hot_cap)] = old[k];
        free(old);
    }
    at = hot_slot(name, hot_cap);
    if (!hot_names[at]) {
        hot_names[at] = name;
        hot_n++;
    }
}

static int hot_has(NameRef name)
{
    return hot_cap && hot_names[hot_slot(name, hot_cap)] == name;
}

/* No frame at all, the code made never reading IX: the whole prologue cut,
 * and the epilogue only the ret. Where relax_function cuts. */
static int frame_none(void)
{
    frame_cut_len = 15;

    return func_start;
}

static int frame_wants_call(void)
{
    return !hot_has(sym_at(func_sym)->name);
}

/* The prologue made a call: ld hl, -frame / call acc_rt_frameset, or call
 * acc_rt_frameset0 for no frame, over the start of what was written out;
 * the call's fixup put first among the function's, where its place is,
 * and the rest of the prologue cut. Where relax_function cuts. */
static int frame_to_call(void)
{
    int start = frame_patch - 10, size = frame_size(), at;
    unsigned char *op = out_img + (start - out_base);

    if (size) {
        op[0] = 0x21;                           /* ld hl, -frame */
        put24(op + 1, -size);
        at = start + 5;
        frame_cut_len = 7;
    } else {
        at = start + 1;
        frame_cut_len = 11;
    }
    op[at - start - 1] = 0xcd;                  /* call nn */
    put24(op + (at - start), 0);
    rt_insert(func_mark.rt, size ? RT_FRAMESET : RT_FRAMESET0, at);
    out_reloc(at);

    return at + ACC_INT_SIZE;
}
#endif

void gen_func_end(void)
{
    /* A return that is the last thing in the function jumps to the next
     * byte: taken back, it falls into the epilogue instead. Whatever jumps
     * to where it was lands on the epilogue, where it went through it. */
    if (return_chain && out_here() == return_chain + ACC_INT_SIZE
        && out_rewinds == return_epoch) {
        int next = get24(out_img + (return_chain - out_base));

        out_rewind(return_chain - 1);
        jumps_forget(return_chain - 1);
        return_chain = next;
    }
    patch_to_here(return_chain);
    return_chain = 0;

#ifndef OPT_ACC
    /* With no frame, SP is where the prologue left it at every return --
     * an array whose length is known only as it runs takes room below a
     * slot of the frame, and a struct argument's is given back after its
     * call -- so it is IX already, and pop ix is all the epilogue does, as
     * agondev's is. And where nothing reads IX either, there need be no
     * frame at all: the prologue cut whole, and the epilogue a ret. */
    frame_unused = !frame_size() && !ix_read(frame_patch + 7, out_here());
    frame_sp_kept = 1;
#endif
    if (!frame_unused) {
        if (!frame_sp_kept || frame_size())     /* SP is IX already */
            out_byte2(0xdd, 0xf9);      /* ld sp, ix */
        out_byte2(0xdd, 0xe1);          /* pop ix */
    }
    out_byte(0xc9);                              /* ret */

    out_patch24(frame_patch, -frame_size());
    in_function = 0;
    iy_local = 0;

    /* Each local array's address, now that the frame is laid out: where
     * it is within a displacement's reach, the ten bytes vaddr_array wrote
     * -- push de; ld de, d; push ix; pop hl; add hl, de; pop de -- are made
     * lea hl, ix+d, and the seven after it are cut with the jumps. */
    {
        int above = locals_size + spill_peak, i;

        const ArrayPatch *ap = array_patches;

        narr_cuts = 0;
        for (i = 0; i < narray_patches; i++, ap++) {
            int at = ap->at;
            int d = -(above + array_end[ap->array]);
            unsigned char *p = out_img + (at - 2 - out_base);

            if (!disp_fits(d)) {
                out_patch24(at, d);
                continue;
            }
            p[0] = 0xed;                /* lea hl, ix+d */
            p[1] = 0x22;
            p[2] = (unsigned char) d;
            if (narr_cuts == arr_cuts_cap) {
                arr_cuts_cap = arr_cuts_cap ? arr_cuts_cap * 2 : 16;
                arr_cut_at = realloc(arr_cut_at,
                                     (size_t) arr_cuts_cap * sizeof *arr_cut_at);
                if (!arr_cut_at)
                    acc_error("out of memory for local arrays");
            }
            arr_cut_at[narr_cuts++] = at + 1;
        }
    }

    /* Last, so that everything written into the function is written before
     * any of it moves -- and before static_end measures how long it is. */
#ifndef OPT_ACC
    if (!frame_size())
        rt_fixups[frame_call].which = RT_FRAMESET0;
#endif
#ifdef OPT_ACC
    relax_function(&func_mark, frame_unused ? frame_none()
                               : frame_wants_call() ? frame_to_call() : frame_lea());
    peep_function(&func_mark, func_start);
#else
    /* No frame: the ld hl of its size cut, or the call to make it too. */
    frame_cut_len = frame_unused ? 8 : 4;
    relax_function(&func_mark, frame_size() ? -1 : frame_patch - 1);
#endif
    pool_emit();
    static_end();
    gen_forget();
    out_flush();
}

/* The tables a function needs while it is compiled, given back once it is:
 * each is empty again when the next begins, and kept at the size the
 * largest function made it, they held that much of the Agon's heap to the
 * end of the file -- the end being where a big file runs out. One that
 * stayed small is kept, so that a file of small functions does not pay for
 * growing them again each time. */
#define FORGET_ABOVE 1024       /* bytes */
#define FORGET(p, cap, width) do {                                      \
        if ((unsigned) (cap) > FORGET_ABOVE / (width)) {                \
            free(p);                                                    \
            (p) = NULL;                                                 \
            (cap) = 0;                                                  \
        }                                                               \
    } while (0)

#ifdef ACC_TABLE_STATS
/* What the per-function tables still hold, for test/forget.sh. */
unsigned gen_table_bytes(void)
{
    return (unsigned) jumps_cap * (sizeof *jump_at + 1)
           + (unsigned) relax_cap * (sizeof *relax_cuts + 2 * sizeof(int) + 1)
           + (unsigned) pool_sites_cap * 2 * sizeof *pool_site_at
           + (unsigned) array_end_cap * sizeof *array_end
           + (unsigned) array_patches_cap * sizeof *array_patches
           + (unsigned) arr_cuts_cap * sizeof *arr_cut_at;
}
#endif

static void gen_forget(void)
{
    /* Divided, not multiplied: the constant folds, where a multiply of
     * the count would be a call to the runtime's. */
    if ((unsigned) jumps_cap > FORGET_ABOVE / sizeof *jump_at) {
        free(jump_cc);
        jump_cc = NULL;
    }
    FORGET(jump_at, jumps_cap, sizeof *jump_at);
    jumps_rewind(0);
    if ((unsigned) relax_cap > FORGET_ABOVE / sizeof *relax_cuts) {
        free(relax_target);
        free(relax_slot);
        free(relax_short);
        relax_target = relax_slot = NULL;
        relax_short = NULL;
    }
    FORGET(relax_cuts, relax_cap, sizeof *relax_cuts);
    if ((unsigned) pool_sites_cap > FORGET_ABOVE / sizeof *pool_site_at) {
        free(pool_site_entry);
        pool_site_entry = NULL;
    }
    FORGET(pool_site_at, pool_sites_cap, sizeof *pool_site_at);
    FORGET(array_end, array_end_cap, sizeof *array_end);
    FORGET(array_patches, array_patches_cap, sizeof *array_patches);
    FORGET(arr_cut_at, arr_cuts_cap, sizeof *arr_cut_at);
    out_forget();
}

void gen_return(int line, const char *spot)
{
    int recorded = -1;             /* its index among const_ret_val */

    /* C99 has a return with a value only in a function that returns one, and
     * one without only in a function that does not. */
    if (return_type == TY_VOID && vtop > 0)
        acc_error_spot(line, spot, "this function returns void, so 'return' "
                                   "cannot give it a value");
    if (return_type != TY_VOID && vtop == 0)
        acc_error_spot(line, spot, "this function returns a value, so "
                                   "'return' needs one");

    /* A struct is copied to where the caller asked for it, which is the
     * hidden first argument, and that address is the answer in HL -- as
     * agondev does it. */
    if (type_is_struct(return_type)) {
        Value *top = vsp - 1;

        if (!type_is_struct(top->type) || top->ext != return_ext)
            acc_error_spot(line, spot, "this function returns a struct, and "
                                       "'return' has to give it one of the "
                                       "same type");
        top->type = type_ptr_to(TY_CHAR);
        force_into(top, R_HL);
        ld_rr_ix(R_DE, 2 * ACC_PTR_SIZE);
        ld_rr_imm(R_BC, ext_bytes(return_ext));
        out_byte2(0xed, 0xb0);          /* ldir */
        ld_rr_ix(R_HL, 2 * ACC_PTR_SIZE);
        vdrop();
        return_jump();

        return;
    }

    /* The result goes in HL, which is where agondev returns an int as well --
     * worth matching even with nothing to link against, because it is what
     * lets the two be mixed later. */
    if (vtop > 0) {
        int reg;

        if (type_wide(return_type)) {
            /* HL with the high byte in E, which is where agondev puts a
             * four-byte result. The value is in the frame, so this is two
             * loads. An eight-byte one is HL, DE and BC from the bottom up,
             * BC's upper byte reading one past the slot, which the caller
             * does not look at. */
            int at;

            vconvert(return_type);
            wide_needs_slot();
            at = (vsp - 1)->val;
            if (type_eight(return_type)) {
                ld_rr_ix(R_HL, at);
                ld_rr_ix(R_DE, at + ACC_INT_SIZE);
                ld_rr_ix(R_BC, at + 2 * ACC_INT_SIZE);
            } else {
                ld_rr_ix(R_HL, at);
                ld_e_ix(at + ACC_INT_SIZE);
            }
            vdrop();
            return_jump();

            return;
        }

        vconvert(return_type);

        /* A constant this function has returned before: a jump back to
         * where that return loaded it, which goes on to the epilogue. */
        if (val_number((vsp - 1)->kind)) {
            int v = (vsp - 1)->val, i, kept = 0;

            for (i = 0; i != nconst_rets; i++)
                if (out_rewind_floor >= const_ret_end[i]) {
                    const_ret_val[kept] = const_ret_val[i];
                    const_ret_at[kept] = const_ret_at[i];
                    const_ret_end[kept++] = const_ret_end[i];
                }
            nconst_rets = kept;
            out_rewind_floor = INT_MAX;
            for (i = 0; i != nconst_rets; i++)
                if (const_ret_val[i] == v) {
                    vdrop();
                    gen_jump_to(const_ret_at[i]);

                    return;
                }
            if (nconst_rets < CONST_RETS) {
                const_ret_val[nconst_rets] = v;
                const_ret_at[nconst_rets] = out_here();
                const_ret_end[nconst_rets] = INT_MAX;   /* until it is done */
                recorded = nconst_rets++;
            }
        }
        reg = vpop_reg();
        if (reg != R_HL)
            mov_rr(R_HL, reg);

        /* A one-byte result goes in A. HL keeps the widened value as well,
         * which costs one byte and is what lets a call to a function defined
         * further down the file -- where the return type is not known yet --
         * still read its answer. */
        if (RETURNS_IN_A(return_type))
            ld_a_l();
    }
    return_jump();
    if (recorded >= 0)
        const_ret_end[recorded] = out_here();
}

#ifdef OPT_ACC
/* A return whose answer is in HL already, as the function's type has it
 * -- widened by its sign, or not, where that is a byte: A given its low
 * byte then, and the jump to the epilogue. What gen_return makes, without
 * widening it again. */
void gen_return_hl(void)
{
    if (RETURNS_IN_A(return_type))
        ld_a_l();
    return_jump();
}
#endif

/* That a struct argument and its parameter are the same struct: a struct
 * cannot be converted to anything, nor anything to one. */
__attribute__((noinline))
static void struct_argument(Type param, int param_ext, int which)
{
    if (!type_is_struct(param) || !type_is_struct(vtype())
        || (vsp - 1)->ext != param_ext)
        acc_error_at(tok_line, "argument %d has to be %s", which + 1,
                     type_is_struct(param) ? "a struct of the parameter's type"
                                           : "a number, not a struct");
}

/* A struct argument, from the address on top: its bytes copied onto the
 * stack, in as many whole slots as they fill, the struct at the lowest
 * address -- as agondev passes one. Returns the slots it took. */
__attribute__((noinline))
static int push_struct(void)
{
    int bytes = ext_bytes((vsp - 1)->ext);
    int slots = (bytes + ACC_INT_SIZE - 1) / ACC_INT_SIZE;

    /* ldir takes all three registers, and the arguments still to go may be
     * in them. */
    save_regs_below(1);
    (vsp - 1)->type = type_ptr_to(TY_CHAR);
    force_into(vsp - 1, R_HL);
    ex_de_hl();                             /* DE: the struct */
    ld_rr_imm(R_HL, -slots * ACC_INT_SIZE);
    out_byte2(0x39, 0xf9);                  /* add hl, sp; ld sp, hl */
    ex_de_hl();                             /* HL: the struct, DE: its copy */
    ld_rr_imm(R_BC, bytes);
    out_byte2(0xed, 0xb0);                  /* ldir */
    vdrop();

    return slots;
}

/* Arguments are pushed right to left, each in a whole three-byte slot, and
 * the caller takes them off again -- which is agondev's convention. */
/* What a call calls: a function by name, or whatever a pointer below the
 * arguments points at -- with the type it returns either way. */
typedef struct {
    Type type;
    int  ext, quals;
    int  fn;                    /* the function, or SYM_NONE for a pointer */
} Callee;

static void call_to(const Callee *callee, int nargs, int params_first,
                    int nparams);

/* exit: back to the stub that called main, as though main had returned.
 *
 * Not a library routine, because there is nothing in C to write it with: it
 * has to put the stack back where it was before main was entered and carry
 * on from where main would have returned to. The stub left both of those
 * in two cells behind itself -- see gen_startup -- so this is four
 * instructions with the answer already in HL, written out at the call
 * rather than called, which saves the return nobody comes back for.
 *
 * Whatever main's arguments left on the stack is still above the restored
 * pointer, and the stub's `pop bc` twice takes them off, exactly as if main
 * had returned in the ordinary way. */
static int exit_builtin(const Sym *f, int nargs)
{
    if (nargs != 1 || f->type != TY_VOID
        || strcmp(name_text(f->name), "exit") != 0)
        return 0;

    save_regs_below(nargs);
    force_into(vsp - 1, R_HL);          /* the status, which the tail reads */
    vdrop();

    out_byte(0xed);
    out_opcode24(0x5b, 0);              /* ld de, (carry on at) */
    fixup_add(exit_cell(1), out_here() - ACC_INT_SIZE);
    out_byte(0xed);
    out_opcode24(0x7b, 0);              /* ld sp, (the stack main was on) */
    fixup_add(exit_cell(0), out_here() - ACC_INT_SIZE);
    push_rr(R_DE);
    out_byte(0xc9);                     /* ret, into what DE was */

    vpush(VAL_VOID, TY_VOID, 0);

    return 1;
}

/* memcpy, memmove, memset and memchr, done by the instructions that do them.
 *
 * The eZ80 copies a block with ldir and fills one with ldir reading its own
 * output a byte behind; C says it a byte at a time, and acc compiled that
 * faithfully into a loop that cost zap seven percent of everything it ran.
 * So a call to one of the three by name goes to a helper written in the
 * instruction instead, with the operands in the registers it wants rather
 * than on the stack.
 *
 * By name, which C allows: these three are the implementation's to define,
 * and a program that writes its own means the library's, not something of
 * its own that happens to be spelled the same. The library still has all
 * three compiled from C, for a program that takes one's address.
 *
 * Returns 0 when this is not one of them, or not the shape of one, and the
 * ordinary call is emitted instead. */
static int mem_builtin(const Sym *f, int nargs)
{
    const char *name;
    int which;

    if (nargs != 3 || !type_pointer(f->type))
        return 0;
    name = name_text(f->name);
    if (strcmp(name, "memcpy") == 0)
        which = RT_MEMCPY;
    else if (strcmp(name, "memmove") == 0)
        which = RT_MEMMOVE;
    else if (strcmp(name, "memset") == 0)
        which = RT_MEMSET;
    else if (strcmp(name, "memchr") == 0)
        which = RT_MEMCHR;
    else
        return 0;

    /* Nothing below the arguments may be left in a register: ldir takes
     * three of them and the helper is free with the rest, exactly as a call
     * would be. */
    save_regs_below(nargs);

    /* How many, first: it is the last argument and the top of the stack, so
     * taking it now leaves the other two where force_into expects them. */
    force_into(vsp - 1, R_BC);

    if (which == RT_MEMSET || which == RT_MEMCHR) {
        force_into(vsp - 2, R_HL);      /* the byte, which goes in A */
        ld_a_l();
        force_into(vsp - 3, R_HL);      /* and where to put it or find it */
    } else {
        force_into(vsp - 2, R_HL);      /* the source, which ldir reads */
        force_into(vsp - 3, R_DE);      /* and the destination it writes */
    }

    rt_call(which);
    vdrop();
    vdrop();
    vdrop();
    vpush_reg(R_HL);
    (vsp - 1)->type = f->type;
    (vsp - 1)->ext = (unsigned char) f->ext;

    return 1;
}

void gen_call(int fn, int nargs, int params_first, int nparams)
{
    gen_effects++;
    Callee callee;
    const Sym *f = sym_at(fn);

    if (mem_builtin(f, nargs) || exit_builtin(f, nargs))
        return;

    callee.type = f->type;
    callee.ext = f->ext;
    callee.quals = f->quals & SQ_CONST ? VQ_CONST : 0;
    callee.fn = fn;
    call_to(&callee, nargs, params_first, nparams);
}

/* A function's address, as a pointer to it: a constant when it is already
 * defined, and otherwise loaded with a hole that gen_finish fills in, as a
 * call to it would be. */
int gen_data_context;
int gen_pending_sym = SYM_NONE;

/* A variable whose address is not known yet, which is what a declaration
 * that only says extern leaves behind: some file defines it, and which
 * address that is comes from the linker -- or from gen_finish, when this
 * file turns out to define it further down.
 *
 * The same shape as a call to a function not yet seen: a load of zero with
 * the slot written down. It costs a register load where a variable with an
 * address is a constant the value stack can carry about and fold into an
 * index, which is why it is only done for the ones that need it. */
#ifdef OPT_ACC
void gen_data_begin(void)
{
    gen_pending_sym = SYM_NONE;
    gen_data_context = 1;
}

void gen_data_end(void)
{
    gen_data_context = 0;
}

void gen_pending_clear(void)
{
    gen_pending_sym = SYM_NONE;
}
#endif

void vpush_global_addr(int sym)
{
    /* In a global's initial value there is no code to put a hole in, so the
     * bytes are the hole: the same path a function's address takes. */
    if (gen_data_context) {
        if (gen_pending_sym != SYM_NONE)
            acc_error_at(tok_line, "one address not yet known in each initial "
                                   "value, at most");
        gen_pending_sym = sym;
        vpush_const(0, type_ptr_to(sym_at(sym)->type));

        return;
    }
    vpush_const(0, type_ptr_to(sym_at(sym)->type));
    force_reg(vsp - 1);
    fixup_add(sym, out_here() - ACC_INT_SIZE);
    gaddr_end = out_here();             /* see vbinop */
    gaddr_reg = (vsp - 1)->val;
    gaddr_epoch = out_rewinds;
}

void vpush_function(int fn)
{
    const Sym *f = sym_at(fn);

    want(fn);

    /* Asked of the flag rather than of the address, here and at the call
     * below, because an object puts its first function at offset zero --
     * and zero is also what a function that has not been defined has. Taking
     * the second path for a function that is in fact defined would still
     * come out right, since the fixup is filled in with the same address,
     * but not with the same instructions: this one leaves a constant on the
     * value stack for whatever wants it, and that one forces a register. */
    if (sym_flags(fn) & SYMF_DEFINED) {
        vpush_const(f->val, type_ptr_to(TY_FUNC));
        vset_addr();

        return;
    }

    /* In a global's initial value there is no code to put the hole in: the
     * value is 0 for now, and the caller fills its bytes in once it knows
     * where they went -- see gen_data_fixup. */
    if (gen_data_context) {
        if (gen_pending_sym != SYM_NONE)
            acc_error_at(tok_line, "one function not yet defined in each "
                                   "initial value, at most");
        gen_pending_sym = fn;
        vpush_const(0, type_ptr_to(TY_FUNC));

        return;
    }
    vpush_const(0, type_ptr_to(TY_FUNC));
    force_reg(vsp - 1);
    fixup_add(fn, out_here() - ACC_INT_SIZE);
}

/* The three bytes at `at`, in the image, are a function's address, which
 * gen_finish writes there once the function is defined. */
void gen_data_fixup(int fn, int at)
{
    fixup_add(fn, at);
}

/* The same for a link's slot, filled now if `fn` has its address already:
 * one placed before the object that wants it. Kept for the end, a fixup
 * is a patch to bytes long gone to the file (see out_flush), and a link
 * of zap had two thousand of them. A weak one waits: something after it
 * may be what it turns out to be. */
void gen_link_fixup(int fn, int at)
{
    int bss = gen_bss_offset(fn);

    /* A variable in the bss whose place in it is known: the slot gets that,
     * and is one of the bss's, which bss_emit moves to where the bss went. */
    if (bss != -1 && !name_weak(sym_at(fn)->name)) {
        out_add24(at, bss);
        gen_bss_fixup(at);

        return;
    }
    if (no_address(fn) || name_weak(sym_at(fn)->name)) {
        gen_data_fixup(fn, at);

        return;
    }
    want(fn);
    out_reloc(at);
    out_add24(at, sym_at(fn)->val);
}

/* A call through the pointer to a function under the arguments. */
void gen_call_indirect(int nargs)
{
    gen_effects++;
    Value *fp = vsp - 1 - nargs;
    Callee callee;
    int x = fp->ext;

    if (fp->type != type_ptr_to(TY_FUNC))
        acc_error_at(tok_line, "only a function or a pointer to one can be "
                               "called");
    callee.type = ext_elem(x);
    callee.ext = ext_elem_x(x);
    callee.quals = 0;
    callee.fn = SYM_NONE;
    call_to(&callee, nargs, ext_func_first(x),
            ext_func_declared(x) ? ext_func_count(x) : 0);
}

/* Names an object wants in the program without holding an address of
 * them. There is one: printf reaches its floating conversions through a
 * weak reference, so that a program that only prints integers does not
 * carry them, and a file that passes a float or a double to a function
 * that takes more than it names asks for them here. The link takes them
 * from the library if it has them, and says nothing if it has not -- a
 * variadic function of the program's own is not printf. */
static const char *wants_named[4];
static int         nwants_named;

static void gen_want(const char *name)
{
    int i;

    for (i = 0; i != nwants_named; i++)
        if (!strcmp(wants_named[i], name))
            return;
    if (nwants_named < (int) (sizeof wants_named / sizeof *wants_named))
        wants_named[nwants_named++] = name;
}

int gen_nwants(void)
{
    return nwants_named;
}

const char *gen_want_name(int i)
{
    return wants_named[i];
}

/* Whether IY is kept across a call to this function: always, in a function
 * with a local in it, since the callee may use IY -- except around setjmp,
 * whose second return, from longjmp, comes back with the stack the pop
 * would read long since written over. setjmp keeps IY in the jmp_buf, and
 * longjmp puts it back. */
static int keeps_iy(const Callee *callee)
{
    static NameRef setjmp_name;

    if (!iy_local)
        return 0;
    if (!setjmp_name)
        setjmp_name = name_intern("setjmp", 6);

    return callee->fn == SYM_NONE || sym_at(callee->fn)->name != setjmp_name;
}

static void call_to(const Callee *callee, int nargs, int params_first,
                    int nparams)
{
    int i, argslots = 0, keep_iy = keeps_iy(callee);

    /* Anything still live in a register has to come out before the call.
     * The result comes back in HL and the callee is free with the rest, so a
     * value left in one does not survive -- which is how `f(..) - g(..)` lost
     * f's answer the moment g was called. The arguments are exempt: they are
     * about to be pushed and consumed. */
    save_regs_below(nargs);
    if (keep_iy)
        iy_save();

    for (i = 0; i != nargs; i++) {
        /* Converted to the type the parameter was declared with. The
         * arguments come off the stack last one first, so this is the
         * parameter that many from the end. */
        int which = nargs - 1 - i;

        /* A struct argument has to meet a struct parameter; anything else
         * meeting one is refused by the conversion. */
        if (type_is_struct((vsp - 1)->type)) {
            if (which < nparams)
                struct_argument(sym_param_type(params_first, which),
                                sym_param_ext(params_first, which), which);
            argslots += push_struct();
            continue;
        }
        if (which >= nparams && gen_objects && type_float((vsp - 1)->type))
            gen_want("acc_format_float");
        if (which < nparams) {
            Type param = sym_param_type(params_first, which);

            if (type_is_struct(param))
                struct_argument(param, sym_param_ext(params_first, which),
                                which);
            vconvert(param);
        }

        if (type_eight(vtype())) {
            /* Three slots, nine bytes, which is what agondev gives a long
             * long: the top one first, so that the eight bytes lie in order
             * from the lowest address. The ninth is read from past the end
             * of the value and means nothing. */
            int slot;

            /* The pushes below go through HL, and an argument still to be
             * pushed may be sitting in it -- `f(&a, 1L)` lost the address.
             * Everything but this argument goes to the frame first. */
            save_regs_below(1);
            wide_needs_slot();
            slot = (vsp - 1)->val;

            ld_rr_ix(R_HL, slot + 2 * ACC_INT_SIZE);
            push_rr(R_HL);
            ld_rr_ix(R_HL, slot + ACC_INT_SIZE);
            push_rr(R_HL);
            ld_rr_ix(R_HL, slot);
            push_rr(R_HL);
            vdrop();
            argslots += 3;
        } else if (type_wide(vtype())) {
            /* Two slots, six bytes, which is what agondev gives a long. The
             * high half goes first because the stack grows downwards, so the
             * low bytes end up at the lower address. HL is used, so the
             * arguments still to go come out of the registers first. */
            int slot;

            save_regs_below(1);
            wide_needs_slot();
            slot = (vsp - 1)->val;

            ld_rr_imm(R_HL, 0);
            ld_l_ix(slot + ACC_INT_SIZE);       /* not through E: DE may hold
                                                 * an argument still to go */
            push_rr(R_HL);
            ld_rr_ix(R_HL, slot);
            push_rr(R_HL);
            vdrop();
            argslots += 2;      /* six bytes, so two slots to take back */
        } else {
            int reg = vpop_reg();

            push_rr(reg);
            argslots++;
        }
    }

    /* A struct comes back in a temporary the caller provides, in the array
     * area, whose address is passed ahead of the arguments. */
    if (type_is_struct(callee->type)) {
        int temp = gen_local_array(), x = callee->ext;

        gen_local_array_size(temp, ext_bytes(x));
        vaddr_array(temp, TY_CHAR);
        push_rr(vpop_reg());
        argslots++;
    }

    if (callee->fn == SYM_NONE) {
        call_through();
    } else if (sym_flags(callee->fn) & SYMF_DEFINED) {
        want(callee->fn);
        out_reloc(out_here() + 1);
        out_opcode24(0xcd, sym_at(callee->fn)->val);    /* call nn */
    } else {
        /* Defined further down the file, or not at all. The site is recorded
         * and filled in once the whole file has been read; gen_finish says so
         * if it never was. */
        out_opcode24(0xcd, 0);
        fixup_add(callee->fn, out_here() - ACC_INT_SIZE);
    }

    /* Discarded into DE, the cheapest form -- but for a long, whose high
     * byte comes back in E, into BC: popping DE put an argument's byte in
     * its place. */
    if (type_eight(callee->type)) {
        /* HL, DE and BC all hold the answer, so the arguments come off
         * into IY. */
        int slot = spill_slot_of(8);

        for (i = 0; i < argslots; i++)
            out_byte2(0xfd, 0xe1);              /* pop iy */
        if (keep_iy)
            iy_restore();                       /* the local */
        ld_ix_rr(slot, R_HL);
        ld_ix_rr(slot + ACC_INT_SIZE, R_DE);
        out_byte(0x79);                         /* ld a, c */
        ld_ix_a(slot + 2 * ACC_INT_SIZE);
        out_byte(0x78);                         /* ld a, b */
        ld_ix_a(slot + 7);
        vpush_scratch(callee->type, slot);
        (vsp - 1)->ext = (unsigned char) callee->ext;

        return;
    }

    for (i = 0; i < argslots; i++)
        pop_rr(type_wide(callee->type) ? R_BC : R_DE);
    if (keep_iy)
        iy_restore();

    if (type_wide(callee->type)) {
        /* HL with the high byte in E; put it where every long lives. */
        int slot = spill_slot_of(ACC_LONG_SIZE);

        ld_ix_rr(slot, R_HL);
        out_byte(0x7b);                          /* ld a, e */
        ld_ix_a(slot + ACC_INT_SIZE);
        vpush_scratch(callee->type, slot);
        (vsp - 1)->ext = (unsigned char) callee->ext;

        return;
    }

    /* The temporary's address, which the callee hands back in HL: the
     * struct, as a value. */
    if (type_is_struct(callee->type)) {
        vpush(VAL_REG, TY_STRUCT, R_HL);
        (vsp - 1)->ext = callee->ext;

        return;
    }

    /* A void function gives back nothing, and a value of kind VAL_VOID says
     * so: dropping it, as a statement does, is all it is good for. */
    if (callee->type == TY_VOID) {
        vpush(VAL_VOID, TY_VOID, 0);

        return;
    }

    /* Read the answer from where the callee's type says it is. */
    if (RETURNS_IN_A(callee->type))
        widen_loaded(callee->type);
    vpush_reg(R_HL);
    (vsp - 1)->type = type_promote(callee->type);
    (vsp - 1)->ext = (unsigned char) callee->ext;
    (vsp - 1)->quals = (unsigned char) callee->quals;
    if (type_unsigned(callee->type) && type_size(callee->type) < ACC_INT_SIZE
        && !type_pointer(callee->type))
        vset_width(vsp - 1, type_size(callee->type));
}

/* The call itself, through the pointer now on top of the stack -- the
 * arguments are pushed. There is no `call (iy)`: the return address is
 * pushed by hand and the jump taken, which is what agondev's __indcallhl
 * does in a routine of its own. The pointer has been put in the frame or
 * is a constant, since everything below the arguments was saved from the
 * registers the arguments needed. */
static void call_through(void)
{
    Value *fp = vsp - 1;

    if (val_const(fp->kind)) {
        out_byte2(0xfd, 0x21);                  /* ld iy, nn */
        if (fp->kind == VAL_ADDR)
            out_reloc(out_here());
        else if (fp->kind == VAL_BSS)
            gen_bss_fixup(out_here());
        out_word24(fp->val);
    } else if (fp->kind == VAL_LOCAL && disp_fits(fp->val)) {
        out_byte3(0xdd, 0x31, fp->val);         /* ld iy, (ix+d) */
    } else {
        int reg = force_reg(fp);

        push_rr(reg);
        out_byte2(0xfd, 0xe1);                  /* pop iy */
    }
    vdrop();
    out_reloc(out_here() + 1);
    out_opcode24(0x21, out_here() + 7);         /* ld hl, back */
    push_rr(R_HL);
    out_byte2(0xfd, 0xe9);                      /* jp (iy) */
}

#ifdef OPT_ACC
/* This file's marks -- where a sequence just emitted ended, and the
 * out_rewinds it was made at -- copied out, or back, for genlog.c, which
 * compiles a function again from where it began and needs every mark as it
 * stood then. Returns how many bytes they take. */
size_t func_marks(unsigned char *buf, int restore)
{
    size_t at = 0;

    STATE_VAR(return_chain);
    STATE_VAR(return_epoch);
    STATE_VAR(gen_data_context);
    STATE_VAR(gen_pending_sym);

    return at;
}
#endif
