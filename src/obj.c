/*
 * Object files: what one compile hands to the linker.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "acc_build.h"

/* Zero when the build could not work out what it is -- see src/build_id.sh.
 * A compiler that cannot say which one it is cannot vouch for an object it
 * finds, so it makes every one of them again. */
#ifndef ACC_BUILD
#define ACC_BUILD 0
#endif

/* The shape of the file. Every number in it is three bytes, lowest first --
 * the width of an address on this machine, and what put24 and get24 already
 * read and write in one move. Names are a blob of NUL-terminated strings
 * that everything else points into by offset.
 *
 * Version 1 was agreed with zap, which writes it too, for assembly that C
 * calls: see docs/object-format-vs-elf.md, and below.
 *
 *   0   4   'A', 'C', 'C', 1        what it is, and the version of this
 *   4   3   build                   which acc made it (see src/build_id.sh);
 *                                   0 for one acc did not make
 *   7   3   text_len
 *   10  3   bss                     room it wants past the image, at zero:
 *                                   the low 20 bits its length, the top 4
 *                                   the log2 of its alignment
 *   13  3   nsyms
 *   16  3   nrelocs
 *   19  3   ndeps
 *   22  3   strings_len
 *   25  3   nitems
 *   28  3   nrelocs_a
 *   31      symbols   nsyms     * 7   name, value, flags
 *           relocs    nrelocs   * 6   at, target and kind
 *           relocs_a  nrelocs_a * 9   at, target and kind, addend
 *           deps      ndeps     * 12  path, size, sum, weighted
 *           items     nitems    * 3   where each starts in the text, and
 *                                     in the top 4 bits the log2 of its
 *                                     alignment
 *           strings   strings_len
 *           text      text_len
 *
 * A name that C can use is spelled as agondev spells it, with an underscore
 * in front: the function `f` is `_f`, main is `_main`, and the runtime's
 * helpers are the `_acc_rt_...` their labels in src/rt/helpers.s already
 * are. A name without one is an assembly object's own, which C cannot name.
 * Inside acc the underscore is taken off, and a name that had none is given
 * an @ in front instead, which no C name can have: see obj_c_name. 
 *
 * The text is what the compiler emitted with the image based at zero, so
 * every address in it is an offset from its own first byte. Placing it is
 * adding one number to the slots the relocations name.
 *
 * A symbol with OBJ_BSS has no room in the text: its value is where in this
 * object's bss it starts, and the address it ends up with is not known until
 * every object has been placed and the bss laid out after them all.
 *
 * A relocation's target, the low 20 bits of its second number, says what
 * the slot wants the address of:
 *
 *   0   the start of this object's text, which is nearly all of them, and
 *       what is there is an address inside that text.
 *   1   the start of this object's bss, and what is there is an offset into
 *       it -- which is how a variable that starts at zero is reached, and
 *       the only way a block's `static` can be, having no name to give.
 *   2+  the address of symbol (target - 2), with what is there as the
 *       amount to add to it.
 *
 * The top 4 bits are the kind: how much of that address the slot is, and
 * how it is made (T the target's address, A the amount added to it, P the
 * slot's own address):
 *
 *   0 ABS24   three bytes, T + A
 *   1 LOW8    one byte, (T + A) & 0xff
 *   2 HIGH8   one byte, ((T + A) >> 8) & 0xff
 *   3 UPPER8  one byte, ((T + A) >> 16) & 0xff
 *   4 PCREL8  one byte, T + A - (P + 1): a jr's or a djnz's, refused past
 *             the -128 to 127 it can say
 *   5 ABS16   two bytes, (T + A) & 0xffff
 *
 * For one in relocs, A is what is already in the slot, as wide as the slot
 * and signed only for PCREL8. One in relocs_a carries A itself and its slot
 * holds zero: HIGH8 and UPPER8 are always there, since the carry out of the
 * bytes below them needs the whole of A, and anything else may be. Both
 * tables are in order of where their slots are, and no slot is in both.
 *
 * Two reserved values rather than a byte saying which kind, because there is
 * one of these for every address the compiler wrote and a byte each is more
 * than the whole of the rest of the table.
 *
 * The items are the text cut where each function and each variable in it
 * begins, this file's own `static` ones included, and at zero: each runs to
 * where the next begins, in order. They are what lets a link take part of an
 * object rather than all of it. Taking an item means taking every item its
 * relocations point into, since that is every address it holds, and an item
 * that no one wants stays behind -- so a library is not made of files cut
 * to one function each for a program to carry only what it calls.
 *
 * The dependencies are every file the compile read. They are not needed to
 * link, and they are here rather than anywhere else because the question
 * they answer -- "does this object have to be made again?" -- is about the
 * object. An answer kept in a file of its own is one that can be lost, or go
 * stale, on its own. The last of them has an empty path and is not a file:
 * its marks are those of the -D, -U and -I options the compile was given,
 * which change what it reads as surely as an edit does. */
#define OBJ_VERSION  1
#define OBJ_HEADER   31
#define OBJ_SYM      7
#define OBJ_RELOC    6
#define OBJ_DEP      12

#define OBJ_ITEM     3
#define OBJ_RELOC_A  9
#define LOW20        0xfffff

/* An object's spelling of a name, and acc's: see the note on the format. */
static char *spelled;
static int   spelled_cap;

static const char *spell(const char *prefix, const char *name)
{
    int n = (int) strlen(prefix) + (int) strlen(name) + 1;

    if (n > spelled_cap) {
        spelled_cap = n * 2;
        spelled = realloc(spelled, (size_t) spelled_cap);
        if (!spelled)
            acc_error("out of memory for a name");
    }
    strcpy(spelled, prefix);
    strcat(spelled, name);

    return spelled;
}

const char *obj_c_name(const char *in_object)
{
    return in_object[0] == '_' ? in_object + 1 : spell("@", in_object);
}

const char *obj_object_name(const char *in_acc)
{
    return in_acc[0] == '@' ? in_acc + 1 : spell("_", in_acc);
}

/* ------------------------------------------------------------------ */
/* writing                                                             */

static int exported(const Sym *s);

/* Where each item begins: zero, and every function and every variable with
 * room in the text, whether or not its name leaves the file -- gathered by
 * obj_write's own walk of the symbols into `at`, n of them, and here put in
 * order and made each once. Answers how many are left. */
static int items_settle(int *at, int n)
{
    int i, kept = 0;

    at[n++] = 0;

    /* Sorted by insertion: they come nearly in order already, a function's
     * start being where the one before it ended, so this is close to one
     * pass -- where qsort called a comparison for every step. */
    for (i = 1; i < n; i++) {
        int v = at[i], j = i;

        while (j > 0 && at[j - 1] > v) {
            at[j] = at[j - 1];
            j--;
        }
        at[j] = v;
    }
    /* An object with no text has no items: each starts inside the text,
     * which is what take_object checks. */
    for (i = 0; i < n; i++)
        if ((i == 0 || at[i] != at[kept - 1]) && at[i] < out_len())
            at[kept++] = at[i];

    return kept;
}

/* Whether the symbol at s begins an item: a function with a body still in
 * the text -- a static one nothing called has been taken out, and is at -1
 * -- or a variable with room in it rather than in the bss. Asked in the walk
 * obj_write makes of the symbols anyway: a second walk of its own was 0.3%
 * of compiling the benchmark's two units, a file's symbols being everything
 * its headers declare as well as what it defines. */
static int starts_item(int s, const Sym *sym)
{
    if (sym->val < 0)
        return 0;
    if (sym->kind == SYM_FUNC)
        return (sym_flags(s) & SYMF_DEFINED) != 0;

    return exported(sym) && gen_bss_offset(s) < 0;
}

/* The names, end to end. Built as the symbols and the dependencies are
 * walked, and written out whole. */
static char *strings;
static int   strings_len, strings_cap;

static int string_add(const char *text)
{
    int at = strings_len, n = (int) strlen(text) + 1;

    if (strings_len + n > strings_cap) {
        strings_cap = strings_cap ? strings_cap * 2 : 256;
        while (strings_len + n > strings_cap)
            strings_cap *= 2;
        strings = realloc(strings, (size_t) strings_cap);
        if (!strings)
            acc_error("out of memory for an object's names");
    }
    memcpy(strings + at, text, (size_t) n);
    strings_len += n;

    return at;
}

/* Which file-scope symbols an object carries.
 *
 * Functions and variables, whether this file defines them or wants them from
 * elsewhere. An enum constant, a typedef and a tag are the compiler's own
 * bookkeeping and mean nothing to a linker, so they stay behind. */
static int exported(const Sym *s)
{
    switch (s->kind) {
    case SYM_FUNC:
    case SYM_GLOBAL:
    case SYM_GLOBAL_ARRAY:
    case SYM_GLOBAL_CONST:
        return 1;
    }

    return 0;
}

/* What a relocation at `at` wants: see the note on the format above.
 *
 * The calls a file cannot resolve are a handful -- most objects have none --
 * so they are walked. The slots that want the bss are not a handful, so they
 * are merged: both they and the relocations are in order of where they are,
 * and `bss` is how far along that walk has got. */
static int reloc_wants(int at, const int *slot_of, int nexterns, int *bss)
{
    int i;

    while (*bss < gen_nbss_fixups() && gen_bss_fixup_at(*bss) < at)
        ++*bss;
    if (*bss < gen_nbss_fixups() && gen_bss_fixup_at(*bss) == at)
        return 1;

    for (i = 0; i < nexterns; i++)
        if (gen_extern_at(i) == at)
            return slot_of[i];

    return 0;
}

static void put_num(FILE *f, int value)
{
    unsigned char bytes[3];

    put24(bytes, value);
    if (fwrite(bytes, 1, 3, f) != 3)
        acc_error("short write on the object");
}

/* The front of an object -- the header and every table -- gathered here
 * and written in one call. A call per number was one each for thousands of
 * them, and on the Agon each goes through the whole of the file layer: that
 * and not the tables was what a compile to an object spent writing them. */
static unsigned char *front;
static int            front_len, front_cap;

static void front_byte(int c)
{
    if (front_len == front_cap) {
        front_cap = front_cap ? front_cap * 2 : 1024;
        front = realloc(front, (size_t) front_cap);
        if (!front)
            acc_error("out of memory for the object");
    }
    front[front_len++] = (unsigned char) c;
}

static void front_num(int value)
{
    front_byte(value & 0xff);
    front_byte((value >> 8) & 0xff);
    front_byte((value >> 16) & 0xff);
}

/* Whether this object has the thing the symbol at s names, rather than
 * wanting it from elsewhere. A function has when its body has been read; a
 * variable has room here, in the text or in the bss -- a declaration that
 * only said extern reserves neither and leaves -1 behind. */
static int defined_here(int s, const Sym *sym)
{
    if (gen_bss_offset(s) >= 0)
        return 1;
    if (sym->kind == SYM_FUNC)
        return (sym_flags(s) & SYMF_DEFINED) != 0;

    return sym->val >= 0;
}

/* -map: where to say what each item is, or NULL. */
const char *obj_map_path;

/* The object's items by name, one line each -- `name offset size`, in the
 * order they come -- `static` ones too, which the object itself does not
 * name: see the note on items above, and test/size.sh, which reads this to
 * set acc's functions beside agondev's. An item no symbol starts, such as
 * the one at zero before the first function, is `-`. */
static void map_write(const int *items, int nitems)
{
    int nglobals = sym_nglobals(), step = (int) sizeof(Sym);
    FILE *f = fopen(obj_map_path, "w");
    int i, s;

    if (!f)
        acc_error("cannot write '%s'", obj_map_path);
    for (i = 0; i < nitems; i++) {
        int end = i + 1 < nitems ? items[i + 1] : out_len();
        const char *name = "-";

        if (end <= items[i])
            continue;
        for (s = 0; s < nglobals; s += step)
            if (starts_item(s, sym_at(s)) && sym_at(s)->val == items[i]) {
                name = name_text(sym_at(s)->name);
                break;
            }
        fprintf(f, "%s %d %d\n", name, items[i], end - items[i]);
    }
    fclose(f);
}

void obj_write(const char *path)
{
    FILE *f;
    int nglobals = sym_nglobals(), step = (int) sizeof(Sym);
    int nsyms = 0, nrelocs = out_nrelocs(), ndeps = lex_ndeps();
    int nexterns = gen_nexterns(), nwalk = nglobals / step;
    int *name_at, *index_of, *dep_at, *slot_of, *items, *want_at;
    char *used;
    int i, s, n, nitems = 0, nwants;

    /* The names first, because the header says how long they come to, and
     * because what each symbol and each dependency points at is settled by
     * putting them there. */
    strings_len = 0;
    name_at = malloc((size_t) (nwalk + 1) * sizeof *name_at);
    index_of = malloc((size_t) (nwalk + 1) * sizeof *index_of);
    dep_at = malloc((size_t) (ndeps + 1) * sizeof *dep_at);
    slot_of = malloc((size_t) (nexterns + 1) * sizeof *slot_of);
    items = malloc((size_t) (nwalk + 2) * sizeof *items);
    used = calloc((size_t) nwalk + 1, 1);
    if (!name_at || !index_of || !dep_at || !slot_of || !items || !used)
        acc_error("out of memory for the object");

    /* Which of the names this file does not define a relocation uses. The
     * rest are what its headers declared and nothing called, and a link has
     * nothing to do with them: 1,289 of the 1,692 symbols in the library
     * were those, sixteen percent of its bytes. */
    for (i = 0; i < nexterns; i++)
        used[gen_extern_sym(i) / step] = 1;

    for (s = 0, n = 0; s < nglobals; s += step, n++) {
        index_of[n] = -1;
        if (starts_item(s, sym_at(s)))
            items[nitems++] = sym_at(s)->val;
        /* What `static` said is this file's alone stays in it: two files may
         * each have one of that name, and a `static inline` in a header gives
         * every file that includes it a copy. */
        if (!exported(sym_at(s)) || (sym_flags(s) & SYMF_STATIC))
            continue;
        if (!used[n] && !defined_here(s, sym_at(s)))
            continue;
        index_of[n] = nsyms++;
        name_at[n] = string_add(obj_object_name(name_text(sym_at(s)->name)));
    }
    for (i = 0; i < ndeps; i++)
        dep_at[i] = string_add(lex_dep_path(i));
    nitems = items_settle(items, nitems);
    if (obj_map_path)
        map_write(items, nitems);

    /* And the names it wants and holds no address of, after every symbol a
     * relocation can point at. */
    nwants = gen_nwants();
    want_at = malloc((size_t) (nwants + 1) * sizeof *want_at);
    if (!want_at)
        acc_error("out of memory for the object");
    for (i = 0; i < nwants; i++)
        want_at[i] = string_add(obj_object_name(gen_want_name(i)));

    /* And which symbol each call out of this file wants. A symbol is named
     * by its byte offset into the compiler's table, so its place in the walk
     * above is that offset divided by the width of one. */
    for (i = 0; i < nexterns; i++)
        slot_of[i] = index_of[gen_extern_sym(i) / step] + 2;

    f = fopen(path, "wb");
    if (!f)
        acc_error("cannot write '%s'", path);

    front_len = 0;
    front_byte('A');
    front_byte('C');
    front_byte('C');
    front_byte(OBJ_VERSION);
    front_num(ACC_BUILD);
    front_num(out_len());
    front_num(gen_bss_len());
    front_num(nsyms + nwants);
    front_num(nrelocs);
    front_num(ndeps);
    front_num(strings_len);
    front_num(nitems);
    front_num(0);                       /* nrelocs_a: acc needs none */

    for (s = 0, n = 0; s < nglobals; s += step, n++) {
        const Sym *sym = sym_at(s);
        int bss_at = gen_bss_offset(s);
        int defined, value, kind;

        if (index_of[n] < 0)
            continue;

        /* A function is defined here when its body has been read. A variable
         * is defined here when this object has room for it, either in the
         * text or in the bss; a declaration that only said extern reserves
         * neither and leaves -1 behind, which is what makes the symbol one
         * the linker has to find somewhere else. */
        if (bss_at >= 0) {
            defined = 1;
            value = bss_at;
            kind = OBJ_BSS;
        } else if (sym->kind == SYM_FUNC) {
            defined = (sym_flags(s) & SYMF_DEFINED) != 0;
            value = defined ? sym->val : 0;
            kind = OBJ_FUNC;
        } else {
            defined = sym->val >= 0;
            value = defined ? sym->val : 0;
            kind = 0;
        }
        /* One this file uses and does not define, that #pragma weak said
         * is not to be looked for on its account. */
        if (!defined && name_weak(sym->name))
            kind |= OBJ_WEAK;
        front_num(name_at[n]);
        front_num(value);
        front_byte((defined ? OBJ_DEFINED : 0) | kind);
    }
    for (i = 0; i < nwants; i++) {
        front_num(want_at[i]);
        front_num(0);
        front_byte(OBJ_WANT | OBJ_FUNC);
    }

    for (i = 0, n = 0; i < nrelocs; i++) {
        int at = out_reloc_at(i);

        front_num(at);
        front_num(reloc_wants(at, slot_of, nexterns, &n));
    }

    for (i = 0; i < ndeps; i++) {
        unsigned size, sum, weighted;

        lex_dep_marks(i, &size, &sum, &weighted);
        front_num(dep_at[i]);
        front_num((int) size);
        front_num((int) sum);
        front_num((int) weighted);
    }

    for (i = 0; i < nitems; i++)
        front_num(items[i]);

    if ((int) fwrite(front, 1, (size_t) front_len, f) != front_len)
        acc_error("short write on '%s'", path);
    if (strings_len
        && (int) fwrite(strings, 1, (size_t) strings_len, f) != strings_len)
        acc_error("short write on '%s'", path);
    if (out_len()
        && (int) fwrite(out_img, 1, (size_t) out_len(), f) != out_len())
        acc_error("short write on '%s'", path);
    fclose(f);

    free(name_at);
    free(index_of);
    free(dep_at);
    free(slot_of);
    free(items);
    free(want_at);
    free(used);
}

/* ------------------------------------------------------------------ */
/* reading                                                             */

/* The whole file at once. An object is the size of the code it holds -- tens
 * of kilobytes at the outside -- and reading it in pieces would mean seeking
 * about on a card that is slow at exactly that. */
/* Bytes already in hand, taken as an object: what a member of an archive is,
 * and what a file becomes once it has been read.
 *
 * `complain` is whether something that is not an object is an error. Linking
 * one says yes; asking whether an object is still current says no, because
 * there the answer is simply "make it again". */
static int take_object(unsigned char *all, long size, const char *path,
                       Object *o, int complain)
{
    FILE *f = NULL;
    int at;

#define REFUSE(...) do {                                                \
        if (complain)                                                   \
            acc_error(__VA_ARGS__);                                     \
        if (f)                                                          \
            fclose(f);                                                  \
        free(all);                                                      \
                                                                        \
        return 0;                                                       \
    } while (0)

    if (size < OBJ_HEADER)
        REFUSE("'%s' is too short to be an object", path);
    if (all[0] != 'A' || all[1] != 'C' || all[2] != 'C')
        REFUSE("'%s' is not an object acc made", path);
    if (all[3] != OBJ_VERSION)
        REFUSE("'%s' is an object of version %d, and this acc reads "
               "version %d", path, all[3], OBJ_VERSION);

    o->all = all;
    o->path = path;
    o->build = get24(all + 4);
    o->text_len = get24(all + 7);
    o->bss_len = get24(all + 10) & LOW20;
    o->bss_align = get24(all + 10) >> 20;
    o->nsyms = get24(all + 13);
    o->nrelocs = get24(all + 16);
    o->ndeps = get24(all + 19);
    o->strings_len = get24(all + 22);
    o->nitems = get24(all + 25);
    o->nrelocs_a = get24(all + 28);

    at = OBJ_HEADER;
    o->syms = all + at;
    at += o->nsyms * OBJ_SYM;
    o->relocs = all + at;
    at += o->nrelocs * OBJ_RELOC;
    o->relocs_a = all + at;
    at += o->nrelocs_a * OBJ_RELOC_A;
    o->deps = all + at;
    at += o->ndeps * OBJ_DEP;
    o->items = all + at;
    at += o->nitems * OBJ_ITEM;
    o->strings = (char *) all + at;
    at += o->strings_len;
    o->text = all + at;
    at += o->text_len;

    /* Checked once, here, so that nothing below has to: a file that says it
     * holds more than it does would otherwise be read past its end. */
    if (at != (int) size || at < OBJ_HEADER)
        REFUSE("'%s' says it holds %d bytes and holds %ld", path, at, size);

    /* And what the format promises, checked here so that the linker can
     * take it on trust: an assembler writes these too. */
    {
        int i, a = 0, b = 0, last = -1;

        for (i = 0; i < o->nitems; i++) {
            int item = obj_item(o, i);

            if ((i == 0 && item != 0) || (i > 0 && item <= obj_item(o, i - 1))
                || item >= o->text_len)
                REFUSE("'%s' has its items out of order", path);
        }
        if (o->text_len && !o->nitems)
            REFUSE("'%s' has text and no item that holds it", path);
        for (i = 0; i < obj_nrelocs(o); i++) {
            int kind = obj_reloc_kind(o, i), at_ = obj_reloc_at(o, i);

            if (kind >= REL_KINDS)
                REFUSE("'%s' has a relocation of kind %d, which acc does "
                       "not know", path, kind);
            if ((kind == REL_HIGH8 || kind == REL_UPPER8) && i < o->nrelocs)
                REFUSE("'%s' has a HIGH8 or UPPER8 relocation with no "
                       "addend of its own", path);
            if (at_ < 0 || at_ + obj_reloc_width(kind) > o->text_len)
                REFUSE("'%s' has a relocation at %06x, outside its %d bytes",
                       path, at_, o->text_len);
            if (obj_reloc_sym(o, i) >= o->nsyms + 2)
                REFUSE("'%s' has a relocation for a symbol it has not got",
                       path);
            if (i == o->nrelocs)
                last = -1;
            if (at_ <= last)
                REFUSE("'%s' has its relocations out of order", path);
            last = at_;
        }

        /* No slot in both tables: a walk of the two in step. */
        while (a < o->nrelocs && b < o->nrelocs_a) {
            int x = obj_reloc_at(o, a), y = obj_reloc_at(o, o->nrelocs + b);

            if (x == y)
                REFUSE("'%s' has two relocations at %06x", path, x);
            if (x < y)
                a++;
            else
                b++;
        }
    }

    return 1;
#undef REFUSE
}

void obj_take(unsigned char *all, int len, const char *path, Object *o)
{
    (void) take_object(all, len, path, o, 1);
}

static int read_object(const char *path, Object *o, int complain)
{
    FILE *f = fopen(path, "rb");
    long size = 0;
    unsigned char *all;

    if (!f) {
        if (complain)
            acc_error("cannot open '%s'", path);

        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0
        || fseek(f, 0, SEEK_SET) != 0 || size < 0) {
        fclose(f);
        if (complain)
            acc_error("cannot read '%s'", path);

        return 0;
    }
    all = malloc((size_t) (size > 0 ? size : 1));
    if (!all)
        acc_error("out of memory for '%s'", path);
    if ((long) fread(all, 1, (size_t) size, f) != size) {
        fclose(f);
        free(all);
        if (complain)
            acc_error("short read on '%s'", path);

        return 0;
    }
    fclose(f);

    return take_object(all, size, path, o, complain);
}

void obj_read(const char *path, Object *o)
{
    (void) read_object(path, o, 1);
}

void obj_free(Object *o)
{
    free(o->all);
    o->all = NULL;
}

int obj_item(const Object *o, int i)
{
    return get24(o->items + i * OBJ_ITEM) & LOW20;
}

int obj_item_align(const Object *o, int i)
{
    return get24(o->items + i * OBJ_ITEM) >> 20;
}

/* The relocations, both tables as one: relocs first, then relocs_a. */
int obj_nrelocs(const Object *o)
{
    return o->nrelocs + o->nrelocs_a;
}

static const unsigned char *reloc_entry(const Object *o, int i)
{
    return i < o->nrelocs ? o->relocs + i * OBJ_RELOC
                          : o->relocs_a + (i - o->nrelocs) * OBJ_RELOC_A;
}

int obj_reloc_width(int kind)
{
    return kind == REL_ABS24 ? 3 : kind == REL_ABS16 ? 2 : 1;
}

int obj_reloc_kind(const Object *o, int i)
{
    return get24(reloc_entry(o, i) + 3) >> 20;
}

/* What is added to the target: the relocation's own when it is in relocs_a,
 * and otherwise what is in its slot -- as wide as the slot, and signed for a
 * PCREL8's one byte. */
long obj_reloc_addend(const Object *o, int i)
{
    const unsigned char *slot;

    if (i >= o->nrelocs) {
        long a = get24(reloc_entry(o, i) + 6);

        return a & 0x800000L ? a - 0x1000000L : a;
    }
    slot = o->text + obj_reloc_at(o, i);
    switch (obj_reloc_kind(o, i)) {
    case REL_ABS24:  return get24(slot);
    case REL_ABS16:  return slot[0] | slot[1] << 8;
    case REL_PCREL8: return (signed char) slot[0];
    default:         return slot[0];
    }
}

const char *obj_sym_name(const Object *o, int i)
{
    return o->strings + get24(o->syms + i * OBJ_SYM);
}

int obj_sym_value(const Object *o, int i)
{
    return get24(o->syms + i * OBJ_SYM + 3);
}

int obj_sym_flags(const Object *o, int i)
{
    return o->syms[i * OBJ_SYM + 6];
}

int obj_reloc_at(const Object *o, int i)
{
    return get24(reloc_entry(o, i));
}

int obj_reloc_sym(const Object *o, int i)
{
    return get24(reloc_entry(o, i) + 3) & LOW20;
}

const char *obj_dep_path(const Object *o, int i)
{
    return o->strings + get24(o->deps + i * OBJ_DEP);
}

void obj_dep_marks(const Object *o, int i, unsigned *size, unsigned *sum,
                   unsigned *weighted)
{
    *size = (unsigned) get24(o->deps + i * OBJ_DEP + 3);
    *sum = (unsigned) get24(o->deps + i * OBJ_DEP + 6);
    *weighted = (unsigned) get24(o->deps + i * OBJ_DEP + 9);
}

/* Whether an object is still the answer: it is there, this acc can read it,
 * and every file it was made from is byte for byte what it was.
 *
 * This is what makes a build incremental, and it asks with a checksum rather
 * than a timestamp because the Agon has no clock that survives being turned
 * off. MOS gets the time from the VDP, which starts from a fixed point at
 * every power-on, so a file written last session and one written this
 * session carry stamps that say nothing about which is newer.
 *
 * The cost is reading each file once more, which is a small fraction of
 * compiling it -- and nothing at all beside the compile it saves.
 *
 * Anything unexpected means "not current": an object from an older version,
 * a header that has been deleted, a file that will not open. Each of those
 * is a reason to compile again and none of them is a reason to stop.
 *
 * `source` is what this compile was asked to compile, and the first file an
 * object records is what its own compile was. They have to be the same file:
 * an object left over from compiling something else has every one of that
 * something else's files unchanged, and would otherwise answer yes. */
int obj_current(const char *path, const char *source)
{
    Object o;
    int i, ndeps, current = 1;

    if (!read_object(path, &o, 0))
        return 0;

    ndeps = o.ndeps;

    /* Made by a different acc, so what it holds is what that acc would have
     * produced and not what this one would. During acc's own development
     * that is the common case: the program's files are untouched and the
     * compiler is the thing that changed.
     *
     * A build that does not know its own number cannot vouch for anything,
     * so it compiles again every time. */
    if (!ACC_BUILD || o.build != ACC_BUILD
        || ndeps < 1 || strcmp(obj_dep_path(&o, 0), source) != 0) {
        obj_free(&o);

        return 0;
    }
    for (i = 0; i < ndeps && current; i++) {
        unsigned was_size, was_sum, was_weighted;
        unsigned size, sum, weighted;

        obj_dep_marks(&o, i, &was_size, &was_sum, &was_weighted);
        if (!lex_file_marks(obj_dep_path(&o, i), &size, &sum, &weighted)
            || size != was_size || sum != was_sum || weighted != was_weighted)
            current = 0;
    }
    obj_free(&o);

    /* An object that records nothing it was made from is one this acc cannot
     * vouch for, so it is made again. */
    return current && ndeps > 0;
}

/* ------------------------------------------------------------------ */
/* archives                                                            */

/* A library: objects end to end, with a list of what each of them defines in
 * front of them.
 *
 *   0   4   'A', 'C', 'R', 1        what it is, and the version of this
 *   4   3   build                   which acc made it: see src/build_id.sh
 *   7   3   nmembers
 *   10  3   ndefs
 *   13  3   strings_len
 *   16      members   nmembers * 9   name, where it starts, how long it is
 *           index     ndefs    * 6   name, which member
 *           strings   strings_len
 *           the members themselves, each a whole object file
 *
 * The index is what an archive is for. Without it, finding who defines
 * `strlen` means reading every member's symbol table, and on a card that is
 * the slow thing. With it, it is a seek and a binary search: the index is
 * sorted by name, and it is the only part of the file a link has to read
 * before it knows which members it wants.
 *
 * A member is a whole object, unchanged, so anything that can read one can
 * read a member -- and the linker places one the same way either way. */
#define AR_VERSION  1
#define AR_HEADER   16
#define AR_MEMBER   9
#define AR_DEF      6

/* Which symbols an archive lists: the ones a member defines, since those are
 * the only ones a link can come here looking for. What a member wants from
 * elsewhere is its own business, and is found by looking again. */
typedef struct {
    int name, member;
} ArDef;

static ArDef *ar_defs;
static int    nar_defs, ar_defs_cap;

static void ar_def_add(int name, int member, const char *text)
{
    int i;

    if (nar_defs == ar_defs_cap) {
        ar_defs_cap = ar_defs_cap ? ar_defs_cap * 2 : 64;
        ar_defs = realloc(ar_defs, (size_t) ar_defs_cap * sizeof *ar_defs);
        if (!ar_defs)
            acc_error("out of memory for the archive's index");
    }

    /* In its place, so that the index comes out sorted and a link can search
     * it rather than walk it. Sorted as it is built rather than afterwards:
     * there is no sort to hand on the Agon, and one written here would be
     * one more thing to be wrong. */
    for (i = nar_defs; i > 0; i--) {
        if (strcmp(strings + ar_defs[i - 1].name, text) <= 0)
            break;
        ar_defs[i] = ar_defs[i - 1];
    }
    ar_defs[i].name = name;
    ar_defs[i].member = member;
    nar_defs++;
}

/* The name a member is known by: the file's, without the directories in
 * front of it. Nothing needs it to link -- the index says which member -- but
 * a library that cannot say what is in it is hard to be sure of. */
static const char *ar_base_name(const char *path)
{
    const char *base = path, *scan;

    for (scan = path; *scan; scan++)
        if (*scan == '/' || *scan == '\\')
            base = scan + 1;

    return base;
}

void ar_write(const char *path, const char **members, int nmembers)
{
    FILE *f;
    Object o;
    int *name_at, *len_of;
    int i, j, at;

    strings_len = 0;
    nar_defs = 0;
    name_at = malloc((size_t) (nmembers + 1) * sizeof *name_at);
    len_of = malloc((size_t) (nmembers + 1) * sizeof *len_of);
    if (!name_at || !len_of)
        acc_error("out of memory for the archive");

    /* Everything but the members themselves is worked out first, because
     * where the members go depends on how long all of it comes to. */
    for (i = 0; i < nmembers; i++) {
        name_at[i] = string_add(ar_base_name(members[i]));
        obj_read(members[i], &o);
        len_of[i] = (int) (o.text + o.text_len - o.all);
        for (j = 0; j < o.nsyms; j++)
            if (obj_sym_flags(&o, j) & OBJ_DEFINED)
                ar_def_add(string_add(obj_sym_name(&o, j)), i,
                           obj_sym_name(&o, j));
        obj_free(&o);
    }

    f = fopen(path, "wb");
    if (!f)
        acc_error("cannot write '%s'", path);

    fputc('A', f);
    fputc('C', f);
    fputc('R', f);
    fputc(AR_VERSION, f);
    put_num(f, ACC_BUILD);
    put_num(f, nmembers);
    put_num(f, nar_defs);
    put_num(f, strings_len);

    at = AR_HEADER + nmembers * AR_MEMBER + nar_defs * AR_DEF + strings_len;
    for (i = 0; i < nmembers; i++) {
        put_num(f, name_at[i]);
        put_num(f, at);
        put_num(f, len_of[i]);
        at += len_of[i];
    }
    for (i = 0; i < nar_defs; i++) {
        put_num(f, ar_defs[i].name);
        put_num(f, ar_defs[i].member);
    }
    if (strings_len
        && (int) fwrite(strings, 1, (size_t) strings_len, f) != strings_len)
        acc_error("short write on '%s'", path);

    for (i = 0; i < nmembers; i++) {
        FILE *in = fopen(members[i], "rb");
        int left = len_of[i];

        if (!in)
            acc_error("cannot open '%s'", members[i]);
        while (left > 0) {
            char buf[1024];
            int want = left < (int) sizeof buf ? left : (int) sizeof buf;

            if ((int) fread(buf, 1, (size_t) want, in) != want
                || (int) fwrite(buf, 1, (size_t) want, f) != want)
                acc_error("short copy of '%s'", members[i]);
            left -= want;
        }
        fclose(in);
    }
    fclose(f);

    free(name_at);
    free(len_of);
}

/* ------------------------------------------------------------------ */

/* Everything but the members, which are read one at a time and only if the
 * link turns out to want them. */
void ar_open(const char *path, Archive *a)
{
    FILE *f = fopen(path, "rb");
    unsigned char head[AR_HEADER];
    long size;
    int front;

    if (!f)
        acc_error("cannot open '%s'", path);
    if (fread(head, 1, AR_HEADER, f) != AR_HEADER)
        acc_error("'%s' is too short to be a library", path);
    if (head[0] != 'A' || head[1] != 'C' || head[2] != 'R')
        acc_error("'%s' is not a library acc made", path);
    if (head[3] != AR_VERSION)
        acc_error("'%s' is a library of version %d, and this acc reads "
                  "version %d", path, head[3], AR_VERSION);

    a->path = path;
    a->build = get24(head + 4);
    a->nmembers = get24(head + 7);
    a->ndefs = get24(head + 10);
    a->strings_len = get24(head + 13);

    front = a->nmembers * AR_MEMBER + a->ndefs * AR_DEF + a->strings_len;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0)
        acc_error("cannot read '%s'", path);
    if (front < 0 || AR_HEADER + front > size)
        acc_error("'%s' says it lists more than it holds", path);

    a->front = malloc((size_t) front);
    if (!a->front)
        acc_error("out of memory for '%s'", path);
    if (fseek(f, AR_HEADER, SEEK_SET) != 0
        || (int) fread(a->front, 1, (size_t) front, f) != front)
        acc_error("short read on '%s'", path);
    fclose(f);

    a->members = a->front;
    a->defs = a->front + a->nmembers * AR_MEMBER;
    a->strings = (char *) a->defs + a->ndefs * AR_DEF;
    a->size = (int) size;
}

void ar_close(Archive *a)
{
    free(a->front);
    a->front = NULL;
}

const char *ar_member_name(const Archive *a, int i)
{
    return a->strings + get24(a->members + i * AR_MEMBER);
}

/* Which member defines `name`, or -1. A binary search, which is what the
 * index is sorted for. */
int ar_find(const Archive *a, const char *name)
{
    int low = 0, high = a->ndefs - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        const unsigned char *def = a->defs + mid * AR_DEF;
        int order = strcmp(a->strings + get24(def), name);

        if (order == 0)
            return get24(def + 3);
        if (order < 0)
            low = mid + 1;
        else
            high = mid - 1;
    }

    return -1;
}

/* One member, read as the object it is. */
void ar_member(const Archive *a, int i, Object *o)
{
    const unsigned char *entry = a->members + i * AR_MEMBER;
    int at = get24(entry + 3), len = get24(entry + 6);
    FILE *f;
    unsigned char *all;

    if (at < AR_HEADER || len < OBJ_HEADER || at + len > a->size)
        acc_error("'%s' says a member is at %06x and %d bytes, and it is not",
                  a->path, at, len);
    f = fopen(a->path, "rb");
    all = malloc((size_t) len);
    if (!f || !all)
        acc_error("cannot read '%s'", a->path);
    if (fseek(f, at, SEEK_SET) != 0
        || (int) fread(all, 1, (size_t) len, f) != len)
        acc_error("short read on '%s'", a->path);
    fclose(f);

    obj_take(all, len, ar_member_name(a, i), o);
}
