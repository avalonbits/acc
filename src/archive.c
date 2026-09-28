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

    if (!f)
        return 0;
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
    a->file = f;

    a->members = a->front;
    a->defs = a->front + a->nmembers * AR_MEMBER;
    a->strings = (char *) a->defs + a->ndefs * AR_DEF;
    a->size = (int) size;

    return 1;
}

void ar_close(Archive *a)
{
    free(a->front);
    a->front = NULL;
    if (a->file)
        fclose((FILE *) a->file);
    a->file = NULL;
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
    all = malloc((size_t) len);
    if (!all)
        acc_error("out of memory for a member of '%s'", a->path);
    f = (FILE *) a->file;
    if (fseek(f, at, SEEK_SET) != 0
        || (int) fread(all, 1, (size_t) len, f) != len)
        acc_error("short read on '%s'", a->path);

    obj_take(all, len, ar_member_name(a, i), o);
}
