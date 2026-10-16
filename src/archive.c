/*
 * Libraries: objects end to end, with a list in front of what each of them
 * defines, so that a link reads only the members it wants.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "acc_build.h"
#include "version.h"
#include "obj_int.h"

/* ------------------------------------------------------------------ */
/* archives                                                            */

/* A library: objects end to end, with a list of what each of them defines in
 * front of them.
 *
 *   0   4   'A', 'C', 'R', 2        what it is, and the version of this
 *   4   3   build                   which acc made it: see src/build_id.sh
 *   7   3   nmembers
 *   10  3   ndefs
 *   13  3   strings_len
 *   16      members   nmembers * 9   name, where it starts, how long it is
 *           index     ndefs    * 8   name, which member, the name's hash
 *           strings   strings_len
 *           the members themselves, each a whole object file
 *
 * The index is what an archive is for. Without it, finding who defines
 * `strlen` means reading every member's symbol table, and on a card that is
 * the slow thing. With it, it is a binary search: the index is sorted by
 * the hash of each name (ar_hash), two bytes, low first, and then by the
 * name, and it is the only part of the file a link has to read before it
 * knows which members it wants. The link holds the index and not the names,
 * which are most of the list; the search is of the hashes, in memory, and
 * a name is read from the file only to be sure of one whose hash matches.
 *
 * Version 1 had no hash, its index entries six bytes and sorted by name, and
 * a library of it is still read: searched by name, from the file.
 *
 * A member is a whole object, unchanged, so anything that can read one can
 * read a member -- and the linker places one the same way either way. */
#define AR_VERSION  2
#define AR_HEADER   16
#define AR_MEMBER   9
#define AR_DEF      8
#define AR_DEF_V1   6

/* Which symbols an archive lists: the ones a member defines, since those are
 * the only ones a link can come here looking for. What a member wants from
 * elsewhere is its own business, and is found by looking again. */
typedef struct {
    int      name, member;
    unsigned hash;
} ArDef;

static ArDef *ar_defs;
static int    nar_defs, ar_defs_cap;

/* A name's hash for the index: djb2 in sixteen bits, the same on every
 * machine, and times 33 by adds, since a multiply on the eZ80 is a call
 * into the runtime. */
static unsigned ar_hash(const char *text)
{
    unsigned short hash = 5381;

    for (; *text; text++) {
        unsigned short times32 = (unsigned short) (hash + hash);

        times32 = (unsigned short) (times32 + times32);
        times32 = (unsigned short) (times32 + times32);
        times32 = (unsigned short) (times32 + times32);
        times32 = (unsigned short) (times32 + times32);
        hash = (unsigned short) (times32 + hash + (unsigned char) *text);
    }

    return hash;
}

static void ar_def_add(int name, int member, const char *text)
{
    if (nar_defs == ar_defs_cap) {
        ar_defs_cap = ar_defs_cap ? ar_defs_cap * 2 : 64;
        ar_defs = realloc(ar_defs, (size_t) ar_defs_cap * sizeof *ar_defs);
        if (!ar_defs)
            acc_error("out of memory for the archive's index");
    }
    {
        ArDef *def = ar_defs + nar_defs;

        def->name = name;
        def->member = member;
        def->hash = ar_hash(text);
    }
    nar_defs++;
}

/* Whether `x` goes before `y` in the index: by hash, and then by name. */
static int def_before(const ArDef *x, const ArDef *y)
{
    if (x->hash != y->hash)
        return x->hash < y->hash;

    return strcmp(strings + x->name, strings + y->name) < 0;
}

/* The index sorted, a merge of runs that double each pass: n log n, and
 * stable, so that of two members that define one name the first keeps the
 * place the search finds first. In bytes, and unsigned: an index into
 * ArDefs is a multiply on the eZ80, and a signed compare a call to repair
 * the flags. */
static void defs_sort(void)
{
    size_t total = (size_t) nar_defs * sizeof *ar_defs, run;
    char *from = (char *) ar_defs, *to = malloc(total + 1), *swap;

    if (!to)
        acc_error("out of memory for the archive's index");
    for (run = sizeof *ar_defs; run < total; run += run) {
        size_t at;

        for (at = 0; at < total; at += run + run) {
            char *left = from + at, *out = to + at;
            char *mid = run < total - at ? left + run : from + total;
            char *end = run + run < total - at ? left + run + run : from + total;
            char *right = mid, *stop = out + (end - left);

            for (; out != stop; out += sizeof *ar_defs) {
                char **take = &right;

                if (left != mid
                    && (right == end
                        || !def_before((ArDef *) (void *) right,
                                       (ArDef *) (void *) left)))
                    take = &left;
                memcpy(out, *take, sizeof *ar_defs);
                *take += sizeof *ar_defs;
            }
        }
        swap = from;
        from = to;
        to = swap;
    }
    if (from != (char *) ar_defs) {
        memcpy(ar_defs, from, total);
        to = from;
    }
    free(to);
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

/* A member's bytes, from its object into the library. Its own function,
 * with its kilobyte of buffer on the stack: in ar_write the buffer made
 * that frame 1,141 bytes, past the 128 an (ix+d) reaches, and each of
 * ar_write's sixty-four other accesses to a local a computed address. A
 * static buffer would do the same for the image and take the kilobyte from
 * the heap; on the stack it is in the room the stack keeps anyway. */
__attribute__((noinline))
static void copy_member(FILE *in, FILE *out, int left, const char *path)
{
    char buf[1024];

    while (left > 0) {
        int want = left < (int) sizeof buf ? left : (int) sizeof buf;

        if ((int) fread(buf, 1, (size_t) want, in) != want
            || (int) fwrite(buf, 1, (size_t) want, out) != want)
            acc_error("short copy of '%s'", path);
        left -= want;
    }
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
    for (i = 0; i != nmembers; i++) {
        name_at[i] = string_add(ar_base_name(members[i]));
        obj_read(members[i], &o, 0);
        len_of[i] = (int) (o.text + o.text_len - o.all);
        for (j = 0; j != o.nsyms; j++)
            if (obj_sym_flags(&o, j) & OBJ_DEFINED)
                ar_def_add(string_add(obj_sym_name(&o, j)), i,
                           obj_sym_name(&o, j));
        obj_free(&o);
    }
    defs_sort();

    f = write_open(path);

    fputc('A', f);
    fputc('C', f);
    fputc('R', f);
    fputc(AR_VERSION, f);
    put_num(f, ACC_BUILD);
    put_num(f, nmembers);
    put_num(f, nar_defs);
    put_num(f, strings_len);

    at = AR_HEADER + nmembers * AR_MEMBER + nar_defs * AR_DEF + strings_len;
    for (i = 0; i != nmembers; i++) {
        put_num(f, name_at[i]);
        put_num(f, at);
        put_num(f, len_of[i]);
        at += len_of[i];
    }
    for (i = 0; i != nar_defs; i++) {
        put_num(f, ar_defs[i].name);
        put_num(f, ar_defs[i].member);
        fputc((int) (ar_defs[i].hash & 0xff), f);
        fputc((int) (ar_defs[i].hash >> 8), f);
    }
    if (strings_len
        && (int) fwrite(strings, 1, (size_t) strings_len, f) != strings_len)
        acc_error("short write on '%s'", path);

    for (i = 0; i != nmembers; i++) {
        FILE *in = fopen(members[i], "rb");
        int left = len_of[i];

        if (!in)
            acc_error("cannot open '%s'", members[i]);
        copy_member(in, f, left, members[i]);
        fclose(in);
    }
    write_done();

    free(name_at);
    free(len_of);
}

/* ------------------------------------------------------------------ */

/* Everything but the members, which are read one at a time and only if the
 * link turns out to want them. */
/* The library at `path`, its list read: 0 if there is no file there. */
int ar_open_if(const char *path, Archive *a)
{
    FILE *f = fopen(path, "rb");
    unsigned char head[AR_HEADER];
    long size;
    int front;
    size_t msize, dsize;

    if (!f)
        return 0;
    if (fread(head, 1, AR_HEADER, f) != AR_HEADER)
        acc_error("'%s' is too short to be a library", path);
    if (head[0] != 'A' || head[1] != 'C' || head[2] != 'R')
        acc_error("'%s' is not a library acc made", path);
    if (head[3] != AR_VERSION && head[3] != 1)
        acc_error("'%s' is a library of version %d, and this acc reads "
                  "version %d", path, head[3], AR_VERSION);

    a->path = path;
    a->build = get24(head + 4);
    a->nmembers = get24(head + 7);
    a->ndefs = get24(head + 10);
    a->strings_len = get24(head + 13);
    a->version = head[3];

    msize = (size_t) a->nmembers * AR_MEMBER;
    dsize = (size_t) a->ndefs * AR_DEF_V1;
    if (a->version != 1)
        dsize += (size_t) (a->ndefs + a->ndefs);     /* AR_DEF, eight */
    front = (int) (msize + dsize) + a->strings_len;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0)
        acc_error("cannot read '%s'", path);
    if (front < 0 || AR_HEADER + front > size)
        acc_error("'%s' says it lists more than it holds", path);

    /* The two tables, a block each; the names stay in the file. Held
     * whole, the list of libc.a was 19 KB in one piece, which a link that
     * follows a compile could not always find. */
    a->members = malloc(msize + 1);
    a->defs = malloc(dsize + 1);
    if (!a->members || !a->defs)
        acc_error("out of memory for '%s'", path);
    if (fseek(f, AR_HEADER, SEEK_SET) != 0
        || fread(a->members, 1, msize, f) != msize
        || fread(a->defs, 1, dsize, f) != dsize)
        acc_error("short read on '%s'", path);
    a->file = f;
    a->strings_at = (long) (AR_HEADER + msize + dsize);
    a->size = (int) size;

    return 1;
}

void ar_close(Archive *a)
{
    free(a->members);
    free(a->defs);
    a->members = a->defs = NULL;
    if (a->file)
        fclose((FILE *) a->file);
    a->file = NULL;
}

/* The name at `at` among the strings, compared with `name` as strcmp would
 * order the two: read from the file a piece at a time, as far as they agree
 * and the next byte. */
static int name_order(const Archive *a, int at, const char *name)
{
    FILE *f = (FILE *) a->file;
    unsigned char piece[AR_NAME_MAX];
    size_t got, k;

    if (fseek(f, a->strings_at + at, SEEK_SET) != 0)
        acc_error("cannot read '%s'", a->path);
    for (;;) {
        got = fread(piece, 1, sizeof piece, f);
        if (!got)
            acc_error("short read on '%s'", a->path);
        for (k = 0; k != got; k++, name++) {
            if (piece[k] != (unsigned char) *name)
                return piece[k] < (unsigned char) *name ? -1 : 1;
            if (!*name)
                return 0;
        }
    }
}

/* A member's name, for the object read from it to carry: read into a
 * buffer the next call writes over, which is the next member's, as a link
 * reads one member at a time. Static, and not in the Archive, which is in
 * its callers' frames. */
const char *ar_member_name(const Archive *a, int i)
{
    static char name[AR_NAME_MAX];
    FILE *f = (FILE *) a->file;
    size_t got;

    if (fseek(f, a->strings_at + get24(a->members + i * AR_MEMBER), SEEK_SET) != 0)
        acc_error("cannot read '%s'", a->path);
    got = fread(name, 1, sizeof name - 1, f);
    name[got] = 0;

    return name;
}

/* Which member a version 1 library says defines `name`, or -1: a binary
 * search of an index sorted by name, each name read from the file. */
static int find_by_name(const Archive *a, const char *name)
{
    unsigned low = 0, high = (unsigned) a->ndefs;

    /* Unsigned, halved by a shift and the entry found by adds: a signed
     * `/ 2` is a call to the runtime's divide and `* AR_DEF` one to its
     * multiply, at every step for every name a link asks a library for. */
    while (low != high) {
        unsigned mid = (low + high) >> 1;
        const unsigned char *def = a->defs + mid + mid + mid;
        int order;

        def += mid + mid + mid;         /* AR_DEF_V1, six */
        order = name_order(a, get24(def), name);
        if (order == 0)
            return get24(def + 3);
        if (order < 0)
            low = mid + 1;
        else
            high = mid;
    }

    return -1;
}

/* Entry i of a version 2 index: eight bytes each, found by adds. */
static const unsigned char *def_entry(const Archive *a, unsigned i)
{
    unsigned twice = i + i, four = twice + twice;

    return a->defs + four + four;
}

/* Which member defines `name`, or -1. The first entry whose hash is not
 * below the name's is found by halving, the hashes compared high byte
 * first, which needs no shift; from there each entry of the same hash is
 * the name or not, which only the file can say. Two names in a library
 * seldom share a hash, so that is about one read for a name the library
 * has, and none for most it does not. */
int ar_find(const Archive *a, const char *name)
{
    unsigned short hash;
    unsigned char low_byte, high_byte;
    unsigned low = 0, high = (unsigned) a->ndefs;
    const unsigned char *def;

    if (a->version == 1)
        return find_by_name(a, name);
    hash = (unsigned short) ar_hash(name);
    memcpy(&low_byte, &hash, 1);                /* the eZ80 and the host are */
    memcpy(&high_byte, (char *) &hash + 1, 1);  /* little-endian */
    while (low != high) {
        unsigned mid = (low + high) >> 1;

        def = def_entry(a, mid);
        if (def[7] < high_byte || (def[7] == high_byte && def[6] < low_byte))
            low = mid + 1;
        else
            high = mid;
    }
    for (; low != (unsigned) a->ndefs; low++) {
        def = def_entry(a, low);
        if (def[7] != high_byte || def[6] != low_byte)
            break;
        if (name_order(a, get24(def), name) == 0)
            return get24(def + 3);
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
    all = malloc((size_t) len);
    if (!all)
        acc_error("out of memory for a member of '%s'", a->path);
    f = (FILE *) a->file;
    if (fseek(f, at, SEEK_SET) != 0
        || (int) fread(all, 1, (size_t) len, f) != len)
        acc_error("short read on '%s'", a->path);

    obj_take(all, len, ar_member_name(a, i), o);
}
