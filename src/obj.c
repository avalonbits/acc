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
 *   0   4   'A', 'C', 'C', 2        what it is, and the version of this
 *   4   3   build                   which acc made it: see src/build_id.sh
 *   7   3   text_len
 *   10  3   nsyms
 *   13  3   nrelocs
 *   16  3   ndeps
 *   19  3   strings_len
 *   22      symbols   nsyms   * 7   name, value, flags
 *           relocs    nrelocs * 6   at, sym
 *           deps      ndeps   * 12  path, size, sum, weighted
 *           strings   strings_len
 *           text      text_len
 *
 * The text is what the compiler emitted with the image based at zero, so
 * every address in it is an offset from its own first byte. Placing it is
 * adding one number to the slots the relocations name.
 *
 * A relocation's `sym` is zero for a slot that holds an address inside this
 * object -- which is nearly all of them -- and otherwise one more than the
 * symbol whose address the slot wants, with what is in the slot as the
 * amount to add to it. One more, so that zero can mean "no symbol" without
 * spending a byte on saying which of the two kinds it is.
 *
 * The dependencies are every file the compile read. They are not needed to
 * link, and they are here rather than anywhere else because the question
 * they answer -- "does this object have to be made again?" -- is about the
 * object. An answer kept in a file of its own is one that can be lost, or go
 * stale, on its own. */
#define OBJ_VERSION  2
#define OBJ_HEADER   22
#define OBJ_SYM      7
#define OBJ_RELOC    6
#define OBJ_DEP      12

/* ------------------------------------------------------------------ */
/* writing                                                             */

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

/* The symbol a relocation wants, one more than its place in the table, or
 * zero when the slot holds an address inside this object.
 *
 * The calls a file cannot resolve are a handful -- most objects have none --
 * so they are walked rather than indexed. */
static int reloc_symbol(int at, const int *slot_of, int nexterns)
{
    int i;

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

void obj_write(const char *path)
{
    FILE *f;
    int nglobals = sym_nglobals(), step = (int) sizeof(Sym);
    int nsyms = 0, nrelocs = out_nrelocs(), ndeps = lex_ndeps();
    int nexterns = gen_nexterns(), nwalk = nglobals / step;
    int *name_at, *index_of, *dep_at, *slot_of;
    int i, s, n;

    /* The names first, because the header says how long they come to, and
     * because what each symbol and each dependency points at is settled by
     * putting them there. */
    strings_len = 0;
    name_at = malloc((size_t) (nwalk + 1) * sizeof *name_at);
    index_of = malloc((size_t) (nwalk + 1) * sizeof *index_of);
    dep_at = malloc((size_t) (ndeps + 1) * sizeof *dep_at);
    slot_of = malloc((size_t) (nexterns + 1) * sizeof *slot_of);
    if (!name_at || !index_of || !dep_at || !slot_of)
        acc_error("out of memory for the object");

    for (s = 0, n = 0; s < nglobals; s += step, n++) {
        index_of[n] = -1;
        if (!exported(sym_at(s)))
            continue;
        index_of[n] = nsyms++;
        name_at[n] = string_add(name_text(sym_at(s)->name));
    }
    for (i = 0; i < ndeps; i++)
        dep_at[i] = string_add(lex_dep_path(i));

    /* And which symbol each call out of this file wants. A symbol is named
     * by its byte offset into the compiler's table, so its place in the walk
     * above is that offset divided by the width of one. */
    for (i = 0; i < nexterns; i++)
        slot_of[i] = index_of[gen_extern_sym(i) / step] + 1;

    f = fopen(path, "wb");
    if (!f)
        acc_error("cannot write '%s'", path);

    fputc('A', f);
    fputc('C', f);
    fputc('C', f);
    fputc(OBJ_VERSION, f);
    put_num(f, ACC_BUILD);
    put_num(f, out_len());
    put_num(f, nsyms);
    put_num(f, nrelocs);
    put_num(f, ndeps);
    put_num(f, strings_len);

    for (s = 0, n = 0; s < nglobals; s += step, n++) {
        const Sym *sym = sym_at(s);
        int defined;

        if (index_of[n] < 0)
            continue;
        /* A function is defined here when its body has been read, and a
         * variable when this object has room for it. A declaration that only
         * said extern reserves nothing and leaves -1 behind, which is what
         * makes the symbol one the linker has to find elsewhere. */
        defined = sym->kind == SYM_FUNC ? (sym_flags(s) & SYMF_DEFINED) != 0
                                        : sym->val >= 0;
        put_num(f, name_at[n]);
        put_num(f, defined ? sym->val : 0);
        fputc((defined ? OBJ_DEFINED : 0)
              | (sym->kind == SYM_FUNC ? OBJ_FUNC : 0), f);
    }

    for (i = 0; i < nrelocs; i++) {
        int at = out_reloc_at(i);

        put_num(f, at);
        put_num(f, reloc_symbol(at, slot_of, nexterns));
    }

    for (i = 0; i < ndeps; i++) {
        unsigned size, sum, weighted;

        lex_dep_marks(i, &size, &sum, &weighted);
        put_num(f, dep_at[i]);
        put_num(f, (int) size);
        put_num(f, (int) sum);
        put_num(f, (int) weighted);
    }

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
}

/* ------------------------------------------------------------------ */
/* reading                                                             */

/* The whole file at once. An object is the size of the code it holds -- tens
 * of kilobytes at the outside -- and reading it in pieces would mean seeking
 * about on a card that is slow at exactly that. */
/* `complain` is whether a file that is not an object is an error. Linking
 * one says yes; asking whether an object is still current says no, because
 * there the answer is simply "make it again". */
static int read_object(const char *path, Object *o, int complain)
{
    FILE *f = fopen(path, "rb");
    long size = 0;
    unsigned char *all;
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

    all = NULL;
    if (!f)
        REFUSE("cannot open '%s'", path);
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0
        || fseek(f, 0, SEEK_SET) != 0)
        REFUSE("cannot read '%s'", path);
    if (size < OBJ_HEADER)
        REFUSE("'%s' is too short to be an object", path);

    all = malloc((size_t) size);
    if (!all)
        acc_error("out of memory for '%s'", path);
    if ((long) fread(all, 1, (size_t) size, f) != size)
        REFUSE("short read on '%s'", path);
    fclose(f);
    f = NULL;

    if (all[0] != 'A' || all[1] != 'C' || all[2] != 'C')
        REFUSE("'%s' is not an object acc made", path);
    if (all[3] != OBJ_VERSION)
        REFUSE("'%s' is an object of version %d, and this acc reads "
               "version %d", path, all[3], OBJ_VERSION);

    o->all = all;
    o->path = path;
    o->build = get24(all + 4);
    o->text_len = get24(all + 7);
    o->nsyms = get24(all + 10);
    o->nrelocs = get24(all + 13);
    o->ndeps = get24(all + 16);
    o->strings_len = get24(all + 19);

    at = OBJ_HEADER;
    o->syms = all + at;
    at += o->nsyms * OBJ_SYM;
    o->relocs = all + at;
    at += o->nrelocs * OBJ_RELOC;
    o->deps = all + at;
    at += o->ndeps * OBJ_DEP;
    o->strings = (char *) all + at;
    at += o->strings_len;
    o->text = all + at;
    at += o->text_len;

    /* Checked once, here, so that nothing below has to: a file that says it
     * holds more than it does would otherwise be read past its end. */
    if (at != (int) size || at < OBJ_HEADER)
        REFUSE("'%s' says it holds %d bytes and holds %ld", path, at, size);

    return 1;
#undef REFUSE
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
    return get24(o->relocs + i * OBJ_RELOC);
}

int obj_reloc_sym(const Object *o, int i)
{
    return get24(o->relocs + i * OBJ_RELOC + 3);
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
