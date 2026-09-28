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
 * before it. */
static int item_of(const Object *o, int at)
{
    unsigned low = 0, high = (unsigned) o->nitems - 1;

    /* Unsigned, and halved by a shift: `/ 2` of a signed int is a call to
     * the runtime's signed divide, which was a third of linking a hello
     * world. */
    while (low < high) {
        unsigned mid = (low + high + 1) >> 1;

        if (obj_item(o, (int) mid) <= at)
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

static void take_items(Object *op, Taken *t, const char *name, const char *path)
{
    Object o = *op;
    char *want = calloc((size_t) o.nitems + 1, 1);
    int *queue = malloc(((size_t) o.nitems + 1) * sizeof *queue);
    int nqueue = 0, i, r, want_bss = !name, new_bss = 0, want_now = 0;
    int nrel = obj_nrelocs(&o);

    if (!want || !queue)
        acc_error("out of memory for '%s'", path);

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
            int item = item_of(&o, obj_sym_value(&o, i));

            if (t->placed[item] < 0 && !want[item]) {
                want[item] = 1;
                queue[nqueue++] = item;
            }
        }
    }

    /* And everything that reaches, a relocation at a time. */
    while (nqueue) {
        int item = queue[--nqueue], from = obj_item(&o, item);
        int to = item_end(&o, item);

        for (r = 0; r != nrel; r++) {
            int at = obj_reloc_at(&o, r), which = obj_reloc_sym(&o, r), next;

            if (at < from || at >= to)
                continue;
            if (which == 1)
                want_bss = 1;
            if (which != 0)
                continue;
            next = item_of(&o, (int) obj_reloc_addend(&o, r, o.text + at));
            if (t->placed[next] < 0 && !want[next]) {
                want[next] = 1;
                queue[nqueue++] = next;
            }
        }
    }

    for (i = 0; i != o.nitems; i++) {
        int step = 1 << obj_item_align(&o, i);

        if (!want[i] || t->placed[i] >= 0)
            continue;
        want_now = 1;                   /* the object's wants come with it */
        while (out_here() & (step - 1))
            out_byte(0);
        t->placed[i] = out_here();
        copy_text(&o, obj_item(&o, i), item_end(&o, i) - obj_item(&o, i));
        if (obj_link_map_on())
            link_map_item(&o, i, t->placed[i], path);
    }
    if (want_bss && t->bss < 0) {
        t->bss = gen_bss_reserve_aligned(o.bss_len, o.bss_align);
        new_bss = 1;
    }

    /* What is new, at the address it now has. */
    for (i = 0; i != o.nsyms; i++) {
        int flags = obj_sym_flags(&o, i), value, item = 0, sym;

        if ((flags & OBJ_WANT) && want_now)
            link_want(obj_sym_name(&o, i));
        if (!(flags & OBJ_DEFINED))
            continue;
        value = obj_sym_value(&o, i);
        if (flags & OBJ_BSS) {
            if (!new_bss)
                continue;
        } else {
            item = item_of(&o, value);
            if (!want[item])
                continue;
        }
        sym = link_symbol(obj_sym_name(&o, i), flags);
        if ((sym_flags(sym) & SYMF_DEFINED) || gen_bss_offset(sym) >= 0)
            acc_error("'%s' is defined in more than one object, and '%s' is "
                      "one of them", obj_c_name(obj_sym_name(&o, i)), path);
        if (flags & OBJ_BSS) {
            gen_bss_symbol(sym, t->bss + value);

            continue;
        }
        sym_at(sym)->val = t->placed[item] + value - obj_item(&o, item);
        sym_set_flags(sym, SYMF_DECLARED | SYMF_DEFINED | SYMF_PARAMS);
    }

    /* The slots in what is new that hold an address, each made the kind it
     * is: see the note on the format in src/obj.c. An address inside the
     * object is taken to where its item went, now or on an earlier look;
     * the bss's and a symbol's are filled when the link ends, since neither
     * is known before. */
    for (r = 0; r != nrel; r++) {
        int at = obj_reloc_at(&o, r), which = obj_reloc_sym(&o, r), item, dest;
        int kind = obj_reloc_kind(&o, r);
        long a;

        item = item_of(&o, at);
        if (!want[item])
            continue;
        dest = t->placed[item] + at - obj_item(&o, item);
        /* Read from where the slot was copied to: an object read with its
         * front only has no text in hand to read it from. */
        a = obj_reloc_addend(&o, r, out_img + (dest - out_base));
        if (which == 0) {
            int target = item_of(&o, (int) a);

            gen_slot(dest, kind, t->placed[target] + a - obj_item(&o, target));
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
    for (i = 0; i != op->nitems; i++)
        t.placed[i] = -1;
    t.bss = -1;
    take_items(op, &t, NULL, path);
    free(t.placed);
    obj_free(op);
}

#ifdef ACC_LIBC
/* Whether the default library is there: a card without it links what it is
 * given, as a host build does. */
static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;
    fclose(f);

    return 1;
}
#endif

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
    int i, n, r, nr, l, nl, k;
} Waits;

/* One walk at a time, and static: in link_archive's frame it took the frame
 * past what (ix+d) reaches. */
static Waits waits;

static void waits_start(Waits *w)
{
    w->i = w->r = w->l = w->k = 0;
    w->n = gen_nfixups();
    w->nr = gen_nrt();
    w->nl = gen_nlate();
}

/* The next name still waited on, or -2 when there are no more. */
static int waits_next(Waits *w)
{
    for (;;) {
        int sym, weak = 1;

        if (w->i != w->n || w->r != w->nr) {
            if (w->r != w->nr
                && (w->i == w->n
                    || (unsigned) gen_rt_at(w->r) < (unsigned) gen_fixup_at(w->i)))
                sym = gen_rt_sym(w->r++);
            else
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
static void link_archive(const char *path)
{
    Archive a;
    Taken *taken;
    int again = 1, m;

    ar_open(path, &a);
    taken = calloc((size_t) a.nmembers + 1, sizeof *taken);
    if (!taken)
        acc_error("out of memory for '%s'", path);

    while (again) {
        int sym;

        again = 0;
        waits_start(&waits);
        while ((sym = waits_next(&waits)) != -2) {
            const char *name;
            Object o;

            name = obj_object_name(name_text(sym_at(sym)->name));
            m = ar_find(&a, name);
            if (m < 0)
                continue;
            ar_member(&a, m, &o);
            if (!taken[m].placed) {
                int k;

                taken[m].placed = malloc(((size_t) o.nitems + 1)
                                         * sizeof *taken[m].placed);
                if (!taken[m].placed)
                    acc_error("out of memory for '%s'", path);
                for (k = 0; k != o.nitems; k++)
                    taken[m].placed[k] = -1;
                taken[m].bss = -1;
            }
            take_items(&o, &taken[m], obj_object_name(name_text(sym_at(sym)->name)),
                       ar_member_name(&a, m));
            obj_free(&o);
            out_flush();
            if (gen_no_address(sym) && gen_bss_offset(sym) < 0)
                acc_error("'%s' says it defines '%s', and its member does "
                          "not", path, name_text(sym_at(sym)->name));
            again = 1;
        }
    }
    for (m = 0; m != a.nmembers; m++)
        free(taken[m].placed);
    free(taken);
    ar_close(&a);
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
            link_archive(objs[i]);
    }
#ifdef ACC_LIBC
    if (link_short() && file_exists(ACC_LIBC))
        link_archive(ACC_LIBC);
#endif
}
