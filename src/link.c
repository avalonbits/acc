/*
 * The linker: objects and libraries placed in the image, their symbols
 * joined to one another's, and the default library looked in last.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "ctype.h"
#include "fmt.h"
#include "timing.h"
#include "version.h"
#include "parse_int.h"

/* A name from an object, as a symbol in the compiler's own table.
 *
 * The linker reuses what the compiler already has: a call whose target is not
 * known yet is a fixup, and gen_finish fills the fixups in once everything
 * has been read. That is the same problem a call to a function further down
 * the file is, so it is the same machinery -- an object is just a file whose
 * functions arrive all at once. */
static int link_symbol(const char *text, int flags)
{
    const char *c_name = obj_c_name(text);
    NameRef name = name_intern(c_name, (int) strlen(c_name));
    int sym = name_global(name);

    if (sym == SYM_NONE) {
        /* As the kind the object said, so that what is said about one that
         * nothing defines is about the right sort of thing -- and so that a
         * variable's "no address yet" is the -1 the compiler uses for it. */
        int func = (flags & OBJ_FUNC) != 0;

        sym = sym_push(name, func ? SYM_FUNC : SYM_GLOBAL, func ? 0 : -1);
        sym_set_flags(sym, SYMF_DECLARED | SYMF_PARAMS);
    }

    /* Weak while every reference to it is: see name_set_weak. A definition
     * is not a reference, and nor is a want, which holds no address. */
    if (flags & OBJ_WEAK)
        name_set_weak(name, 1);
    else if (!(flags & (OBJ_DEFINED | OBJ_WANT)))
        name_set_weak(name, 0);

    return sym;
}

/* The names objects want in the program and hold no address of: see
 * gen_want. A link looks for them in a library as it does for a call no
 * one has answered, and says nothing if none has them. */
static int *wanted;
static int  nwanted, wanted_cap;

static void link_want(const char *text)
{
    int sym = link_symbol(text, OBJ_FUNC | OBJ_WANT), i;

    for (i = 0; i != nwanted; i++)
        if (wanted[i] == sym)
            return;
    if (nwanted == wanted_cap) {
        wanted_cap = wanted_cap ? wanted_cap * 2 : 8;
        wanted = realloc(wanted, (size_t) wanted_cap * sizeof *wanted);
        if (!wanted)
            acc_error("out of memory for what the objects want");
    }
    wanted[nwanted++] = sym;
}

static void place_object(Object *op, const char *path);

static void link_object(const char *path)
{
    Object o;

    obj_read(path, &o, 1);
    place_object(&o, path);
    out_flush();
}

/* What a link has taken from one member of a library so far: where each of
 * its items went, or -1 for one still left behind, and where its bss went,
 * or -1 while nothing that was taken wants it. */
typedef struct {
    int *placed;
    int  bss;
} Taken;

/* Which item the text offset `at` is in: the last one to start at or
 * before it, of the `n` that start at `start`, which take_items reads out
 * of the object once. Read through obj_item, each step of the halving was
 * a frame and a call to mask the entry's top nibble, and halving was a
 * fifth of linking zap.
 *
 * Unsigned, and halved by a shift: `/ 2` of a signed int is a call to the
 * runtime's signed divide, which was a third of linking a hello world. */
/* start[i], found by adding i to the pointer once for each byte of an
 * int: as an index, a call to the runtime's __imulu (see reloc_entry in
 * obj.c). The host's ints are other sizes. */
__attribute__((noinline))
static int item_start(const int *start, int i)
{
    if (sizeof (int) != 3)
        return start[i];

    return *(const int *) (const void *) ((const char *) start + i + i + i);
}

static int item_in(const int *start, int n, int at)
{
    unsigned low = 0, high = (unsigned) n - 1;

    /* A member of a library has a few items: walked by a pointer, which
     * steps by an add, where each step of the halving is a call to shift
     * and one to item_start. Halved past 32, an object's hundreds. */
    if ((unsigned) n <= 32u) {
        const int *p = start + 1;
        int i = 1;

        /* Counted alongside: `p - start` is a divide by three, and
         * `start + n` a multiply. */
        while (i != n && (unsigned) *p <= (unsigned) at) {
            p++;
            i++;
        }

        return i - 1;
    }

    /* Offsets are never negative: compared unsigned, which is no call. */
    while (low < high) {
        unsigned mid = (low + high + 1) >> 1;

        if ((unsigned) item_start(start, (int) mid) <= (unsigned) at)
            low = mid;
        else
            high = mid - 1;
    }

    return (int) low;
}

static int item_end(const Object *o, int i)
{
    return i + 1 < o->nitems ? obj_item(o, i + 1) : o->text_len;
}

/* Part of an object: the item `name` is in, and every item that one holds
 * an address in, and theirs, and so on -- which is everything it can reach,
 * since every address the compiler writes is a relocation. A null `name`
 * is all of it, which is an object placed whole. What was taken before
 * stays where it went, and what is new goes where the image has got to, in
 * the order it was in, so that items next to each other in the object are
 * next to each other in the image -- each padded to where its alignment
 * says it may start.
 *
 * `name` is spelled as the object spells it. A symbol whose item is taken is
 * defined here; one whose item is not is left for another member, or
 * another look at this one. The bss is taken whole, the first time
 * anything taken wants it: it costs the machine room past the image and
 * none in the image itself. */
/* The line -map gives an item the link has just placed: named by the
 * global it starts with, if any does. */
__attribute__((noinline))
static void link_map_item(const Object *o, int i, int at, const char *path)
{
    const char *name = "-";
    int s;

    for (s = 0; s != o->nsyms; s++) {
        int flags = obj_sym_flags(o, s);

        if ((flags & OBJ_DEFINED) && !(flags & OBJ_BSS)
            && obj_sym_value(o, s) == obj_item(o, i)) {
            name = obj_c_name(obj_sym_name(o, s));
            break;
        }
    }
    obj_link_map_item(at, item_end(o, i) - obj_item(o, i), name, path,
                      obj_item(o, i));
}

/* The `n` bytes of an object's text from `at`, into the image: from the
 * text in hand, or for an object read with its front only, which is placed
 * whole and in order, the next `n` from its file. */
__attribute__((noinline))
static void copy_text(Object *o, int at, int n)
{
    while ((unsigned) (out_limit - out_put) < (unsigned) n)
        out_grow();
    if (o->text)
        memcpy(out_put, o->text + at, (size_t) n);
    else if ((int) fread(out_put, 1, (size_t) n, (FILE *) o->file) != n)
        acc_error("short read on '%s'", o->path);
    out_put += n;
}

/* The first of relocations lo to hi, which rise, whose slot is at `at` or
 * past it. */
static int reloc_from(const Object *o, int lo, int hi, int at)
{
    while (lo != hi) {
        int mid = lo + ((unsigned) (hi - lo) >> 1);

        if ((unsigned) obj_reloc_at(o, mid) < (unsigned) at)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo;
}

static void take_items(Object *op, Taken *t, const char *name, const char *path)
{
    Object o = *op;
    char *want = calloc((size_t) o.nitems + 1, 1);
    int *queue = malloc(((size_t) o.nitems + 1) * sizeof *queue);
    int *start = malloc(((size_t) o.nitems + 1) * sizeof *start);
    int nqueue = 0, i, r, want_bss = !name, new_bss = 0, want_now = 0;
    int nrel = obj_nrelocs(&o), item, delta = 0;

    if (!want || !queue || !start)
        acc_error("out of memory for '%s'", path);

    /* Where each item starts, and the text's end after the last. */
    {
        int *put = start;

        for (i = 0; i != o.nitems; i++)
            *put++ = obj_item(&o, i);
        *put = o.text_len;
    }

    /* The item the wanted name is in -- or its bss, if that is where it is.
     * Or every item, for an object placed whole. */
    for (i = 0; !name && i < o.nitems; i++)
        want[i] = 1;
    for (i = 0; name && i < o.nsyms; i++) {
        int flags = obj_sym_flags(&o, i);

        if (!(flags & OBJ_DEFINED) || strcmp(obj_sym_name(&o, i), name))
            continue;
        if (flags & OBJ_BSS) {
            want_bss = 1;
        } else {
            item = item_in(start, o.nitems, obj_sym_value(&o, i));

            if (item_start(t->placed, item) < 0 && !want[item]) {
                want[item] = 1;
                queue[nqueue++] = item;
            }
        }
    }

    /* And everything that reaches, a relocation at a time: each item's
     * own, found by halving in the two tables, which are each in order of
     * where their slots are, as take_object made sure. */
    while (nqueue) {
        int from, to, run;

        item = queue[--nqueue];
        from = item_start(start, item);
        to = item_start(start, item + 1);

        for (run = 0; run != 2; run++) {
            int end = run ? nrel : o.nrelocs;

            r = reloc_from(&o, run ? o.nrelocs : 0, end, from);
            for (; r != end; r++) {
                ObjReloc rel;
                int next;

                obj_reloc_read(&o, r, &rel);
                if ((unsigned) rel.at >= (unsigned) to)
                    break;
                if (rel.sym == 1)
                    want_bss = 1;
                if (rel.sym != 0)
                    continue;
                next = (int) obj_reloc_add(&rel, o.text + rel.at);
                if ((unsigned) next - (unsigned) from < (unsigned) to - (unsigned) from)
                    continue;           /* inside the item: wanted already */
                next = item_in(start, o.nitems, next);
                if (item_start(t->placed, next) < 0 && !want[next]) {
                    want[next] = 1;
                    queue[nqueue++] = next;
                }
            }
        }
    }

    /* Walked by pointers, each a step of an add: indexed, the two tables
     * of three-byte ints were a multiply apiece. */
    {
        int *pl = t->placed;
        const int *sp = start;

        for (i = 0; i != o.nitems; i++, pl++, sp++) {
            int step;

            if (!want[i] || *pl >= 0)
                continue;
            step = 1 << obj_item_align(&o, i);
            want_now = 1;               /* the object's wants come with it */
            while (out_here() & (step - 1))
                out_byte(0);
            *pl = out_here();
            copy_text(&o, sp[0], sp[1] - sp[0]);
            if (obj_link_map_on())
                link_map_item(&o, i, *pl, path);
        }
    }
    /* An object placed whole is placed as it is: its start rounded up to
     * the largest alignment in it, and every item's offset a multiple of
     * its own (place_object), so no padding comes between them, and every
     * item is as far from where it is in the object as the first. */
    if (!name && o.nitems)
        delta = *t->placed - *start;
    if (want_bss && t->bss < 0) {
        t->bss = gen_bss_reserve_aligned(o.bss_len, o.bss_align);
        new_bss = 1;
    }

    /* What is new, at the address it now has. */
    for (i = 0; i != o.nsyms; i++) {
        int flags = obj_sym_flags(&o, i), value, sym;

        if ((flags & OBJ_WANT) && want_now)
            link_want(obj_sym_name(&o, i));
        if (!(flags & OBJ_DEFINED))
            continue;
        value = obj_sym_value(&o, i);
        item = 0;
        if (flags & OBJ_BSS) {
            if (!new_bss)
                continue;
        } else if (name) {
            item = item_in(start, o.nitems, value);
            if (!want[item])
                continue;
        }                               /* the whole object: delta */
        sym = link_symbol(obj_sym_name(&o, i), flags);
        if ((sym_flags(sym) & SYMF_DEFINED) || gen_bss_offset(sym) >= 0)
            acc_error("'%s' is defined in more than one object, and '%s' is "
                      "one of them", obj_c_name(obj_sym_name(&o, i)), path);
        if (flags & OBJ_BSS) {
            gen_bss_symbol(sym, t->bss + value);

            continue;
        }
        sym_at(sym)->val = !name ? value + delta
                                 : item_start(t->placed, item) + value
                                   - item_start(start, item);
        sym_set_flags(sym, SYMF_DECLARED | SYMF_DEFINED | SYMF_PARAMS);
    }

    /* The slots in what is new that hold an address, each made the kind it
     * is: see the note on the format in src/obj.c. An address inside the
     * object is taken to where its item went, now or on an earlier look;
     * the bss's and a symbol's are filled when the link ends, since neither
     * is known before. The item a slot is in is found by walking along
     * with the slots, which rise in each table. */
    for (r = 0, item = 0; r != nrel; r++) {
        ObjReloc rel;
        int at, which, kind, dest;
        long a;

        obj_reloc_read(&o, r, &rel);
        at = rel.at;
        which = rel.sym;
        kind = rel.kind;
        if (!name) {
            dest = at + delta;
        } else {
            if (r == o.nrelocs)                 /* the second table */
                item = 0;
            while ((unsigned) item + 1 != (unsigned) o.nitems
                   && (unsigned) item_start(start, item + 1) <= (unsigned) at)
                item++;
            if (!want[item])
                continue;
            dest = item_start(t->placed, item) + at - item_start(start, item);
        }
        /* Read from where the slot was copied to: an object read with its
         * front only has no text in hand to read it from. */
        a = obj_reloc_add(&rel, out_img + (dest - out_base));
        if (which == 0) {
            int target = item;
            unsigned lo, len;

            /* Most addresses inside a member taken in part are of the
             * item the slot is in -- a jump inside its own function --
             * and need no search. */
            if (name) {
                lo = (unsigned) item_start(start, item);
                len = (unsigned) item_start(start, item + 1) - lo;
                if ((unsigned) a - lo >= len)
                    target = item_in(start, o.nitems, (int) a);
            }

            gen_slot(dest, kind, !name ? (int) a + delta
                                       : item_start(t->placed, target) + a
                                         - item_start(start, target));
            /* An address in the image, which -r has to name as it does the
             * compiler's own: a jump inside a routine of the runtime, a
             * call to a static function of a member. */
            if (kind == REL_ABS24)
                out_reloc(dest);
        } else if (which == 1 && kind == REL_ABS24) {
            out_patch24(dest, (int) a + t->bss);
            gen_bss_fixup(dest);
        } else if (which == 1) {
            gen_late_fixup(-1, dest, kind, t->bss + a);
        } else if (kind == REL_ABS24) {
            /* The symbol is added to what is in the slot: now, or by
             * gen_finish. */
            out_patch24(dest, (int) a);
            gen_link_fixup(link_symbol(obj_sym_name(&o, which - 2),
                                       obj_sym_flags(&o, which - 2)), dest);
        } else {
            gen_late_fixup(link_symbol(obj_sym_name(&o, which - 2),
                                       obj_sym_flags(&o, which - 2)),
                           dest, kind, a);
        }
    }
    free(want);
    free(queue);
    free(start);
}

/* One object, placed where the image has got to: all of its items, from a
 * start rounded up to the largest alignment any of them asks for, which
 * leaves every one of them aligned if its offset is a multiple of its own --
 * and the object is refused if one is not. */
static void place_object(Object *op, const char *path)
{
    Taken t;
    int i, most = 0;

    for (i = 0; i != op->nitems; i++) {
        int align = obj_item_align(op, i);

        if (obj_item(op, i) & ((1 << align) - 1))
            acc_error("'%s' has an item at %06x that is to be aligned to %d "
                      "bytes, and cannot be where it is", path,
                      obj_item(op, i), 1 << align);
        if (align > most)
            most = align;
    }
    while (out_here() & ((1 << most) - 1))
        out_byte(0);

    t.placed = malloc(((size_t) op->nitems + 1) * sizeof *t.placed);
    if (!t.placed)
        acc_error("out of memory for '%s'", path);
    {
        int *pl = t.placed;

        for (i = 0; i != op->nitems; i++)
            *pl++ = -1;
    }
    t.bss = -1;
    take_items(op, &t, NULL, path);
    free(t.placed);
    obj_free(op);
}

/* What a link waits on, in the order it asks a library for it: the calls
 * and addresses and a compile's calls into the runtime together, in the
 * order of their slots -- the order a link of the file's object meets them
 * in, so that building a program in one step and in two takes the same
 * members in the same order -- then the slots of the kinds an assembly
 * object has (gen_late_fixup), then what objects want.
 *
 * A variable whose room is in a bss has no address until the link ends, and
 * is defined all the same. A weak name is not looked for on its own
 * account. A slot that wants the bss is not waiting on a name at all. */
typedef struct {
    int i, n, l, nl, k;
    const int *rat, *rsym, *rend;       /* the runtime's, walked by pointer */
} Waits;

/* One walk at a time, and static: in link_archive's frame it took the frame
 * past what (ix+d) reaches. */
static Waits waits;

static void waits_start(Waits *w)
{
    int nr = gen_rt_first(&w->rat, &w->rsym);

    w->rend = w->rat + nr;
    w->i = w->l = w->k = 0;
    w->n = gen_nfixups();
    w->nl = gen_nlate();
}

/* Another look, from where the last stopped: what it passed is now
 * defined, or a name no member has, and stays so -- nothing a link does
 * takes a fixup or a want away, only adds them -- so only what was added
 * since is new. A look from the start went through every slot of the
 * program again, each a fixup's lookup, for every member taken. */
static void waits_more(Waits *w)
{
    w->n = gen_nfixups();
    w->nl = gen_nlate();
}

/* The next name still waited on, or -2 when there are no more. */
static int waits_next(Waits *w)
{
    for (;;) {
        int sym, weak = 1;

        if (w->i != w->n || w->rat != w->rend) {
            if (w->rat != w->rend
                && (w->i == w->n
                    || (unsigned) *w->rat < (unsigned) gen_fixup_at(w->i))) {
                sym = *w->rsym++;
                w->rat++;
            } else
                sym = gen_fixup_sym(w->i++);
        } else if (w->l != w->nl) {
            sym = gen_late_sym(w->l++);
        } else if (w->k != nwanted) {
            sym = wanted[w->k++];
            weak = 0;
        } else {
            return -2;
        }
        if (sym < 0 || !gen_no_address(sym) || gen_bss_offset(sym) >= 0
            || (weak && name_weak(sym_at(sym)->name)))
            continue;

        return sym;
    }
}

/* Whether anything is still waiting on a name: a program that calls
 * nothing it does not define has no use for a library, and reading one's
 * index is 19 KB of heap for libc.a. */
static int link_short(void)
{
    waits_start(&waits);

    return waits_next(&waits) != -2;
}

/* A library, which is asked only for what the link is short of.
 *
 * Round and round until it has nothing more to offer: what is taken may
 * call something that nothing has called yet, and that something may be in
 * this same library -- or in a part of a member that has been looked at
 * already, which is looked at again. Which is why a link takes more than one
 * look at a library and only one at an object.
 *
 * What is taken from a member is only what the program reaches: see
 * take_items. An object named on the command line is placed whole, as a
 * program's own code is. */
/* The names asked of the library being read that it has no member for: see
 * name_set_missed. */
static NameRef *missed, *missed_put, *missed_limit;

/* Twice the room, counted in bytes as the image's tables are. */
__attribute__((noinline))
static void missed_grow(void)
{
    size_t used = (size_t) ((char *) missed_put - (char *) missed);
    size_t room = used ? used + used : 32 * sizeof *missed;

    missed = realloc(missed, room);
    if (!missed)
        acc_error("out of memory for a library's names");
    missed_put = (NameRef *) (void *) ((char *) missed + used);
    missed_limit = (NameRef *) (void *) ((char *) missed + room);
}

/* A library open for a link: its index in memory, and what has been taken
 * from each member, kept between looks so that a second look -- the
 * runtime's, after the C library's -- takes from a member it took from
 * before only what is still left there. */
typedef struct {
    Archive a;
    Taken  *taken;
    const char *path;
} Library;

/* Opened, or 0 if it is not there and need not be. */
static int library_open(Library *lib, const char *path, int must)
{
    /* The default libraries may not be there: a card without them links
     * what it is given, as the host build once did. Opened rather than
     * asked after first, which is a second search of the directory. */
    if (!ar_open_if(path, &lib->a)) {
        if (must)
            acc_error("cannot open '%s'", path);

        return 0;
    }
    lib->path = path;
    lib->taken = calloc((size_t) lib->a.nmembers + 1, sizeof *lib->taken);
    if (!lib->taken)
        acc_error("out of memory for '%s'", path);

    return 1;
}

static void library_close(Library *lib)
{
    int m;

    for (m = 0; m != lib->a.nmembers; m++)
        if (lib->taken[m].placed)
            free(lib->taken[m].placed);
    free(lib->taken);
    ar_close(&lib->a);
}

static void library_search(Library *lib)
{
    const char *path = lib->path;
    Taken *taken = lib->taken;
    int again = 1, m;

    waits_start(&waits);
    while (again) {
        int sym;

        again = 0;
        waits_more(&waits);
        while ((sym = waits_next(&waits)) != -2) {
            const char *name;
            Object o;

            if (name_missed(sym_at(sym)->name))
                continue;
            name = obj_object_name(name_text(sym_at(sym)->name));
            m = ar_find(&lib->a, name);
            if (m < 0) {
                if (missed_put == missed_limit)
                    missed_grow();
                *missed_put++ = sym_at(sym)->name;
                name_set_missed(sym_at(sym)->name, 1);
                continue;
            }
            ar_member(&lib->a, m, &o);
            if (!taken[m].placed) {
                int k;

                taken[m].placed = malloc(((size_t) o.nitems + 1)
                                         * sizeof *taken[m].placed);
                if (!taken[m].placed)
                    acc_error("out of memory for '%s'", path);
                {
                    int *pl = taken[m].placed;

                    for (k = 0; k != o.nitems; k++)
                        *pl++ = -1;
                }
                taken[m].bss = -1;
            }
            take_items(&o, &taken[m], obj_object_name(name_text(sym_at(sym)->name)),
                       ar_member_name(&lib->a, m));
            obj_free(&o);
            out_flush();
            if (gen_no_address(sym) && gen_bss_offset(sym) < 0)
                acc_error("'%s' says it defines '%s', and its member does "
                          "not", path, name_text(sym_at(sym)->name));
            again = 1;
        }
    }
    while (missed_put != missed)
        name_set_missed(*--missed_put, 0);
}

static void link_archive(const char *path, int must)
{
    Library lib;

    if (!library_open(&lib, path, must))
        return;
    library_search(&lib);
    library_close(&lib);
}

/* The objects and libraries a program is linked from, in the order given,
 * and then the default library if the build names one and it is there. A
 * library is read only while something is still waiting on a name. */
void link_inputs(const char **objs, int nobjs)
{
    int i;

    for (i = 0; i != nobjs; i++) {
        if (!is_archive(objs[i]))
            link_object(objs[i]);
        else if (link_short())
            link_archive(objs[i], 1);
    }
#ifdef ACC_LIBC
    /* The runtime first, from the library beside the C library: every
     * program calls it, and its index is a few names, so a program that
     * calls nothing in the C library does not read that one's thousand.
     * Then the C library, if anything still waits, and the runtime again
     * for what the C library's members call. */
    {
        static char rt[sizeof ACC_LIBC + 8];
        const char *slash = strrchr(ACC_LIBC, '/');
        int dir = slash ? (int) (slash - ACC_LIBC) + 1 : 0;

        Library run, c;
        int have_rt;

        memcpy(rt, ACC_LIBC, (size_t) dir);
        strcpy(rt + dir, "rt.a");
        /* The runtime opened once for both its looks: an open is some
         * 150,000 cycles of MOS's, a sixth of linking a hello world. */
        if (!link_short())
            return;
        have_rt = library_open(&run, rt, 0);
        if (have_rt)
            library_search(&run);
        if (link_short() && library_open(&c, ACC_LIBC, 0)) {
            library_search(&c);
            library_close(&c);
            if (have_rt && link_short())
                library_search(&run);
        }
        if (have_rt)
            library_close(&run);
    }
#endif
}
