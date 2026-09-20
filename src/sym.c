/*
 * The symbol table.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>

#include "acc.h"

/* One array in two regions: file-scope symbols at the bottom, the current
 * function's parameters and locals above them, dropped when it ends. Lookup
 * walks backwards, so a local naturally shadows a file-scope name without any
 * scope chain to maintain.
 *
 * Which region a symbol goes in follows from its kind, so no caller can put
 * one in the wrong place. That matters because the tempting thing -- push
 * wherever the parser happens to be -- is wrong for the case that matters
 * most: a call to a function that has not been defined yet creates the
 * symbol, and the call is inside some other function, so pushing it as a
 * local would drop it at that function's closing brace and leave the call
 * pointing at nothing.
 *
 * The locals are walked backwards and the file-scope names are looked up
 * directly, which is the split the shape of the problem asks for. A function
 * has a handful of locals and the walk stops at the first match, so a table
 * would cost more than it saved. File scope is the other way round: it only
 * grows, and a walk of it is as long as the program has functions. Each name
 * carries its file-scope symbol with it in the name arena (name_global), so
 * finding one is a load.
 *
 * Measured, when file scope was walked too: two inputs identical byte for byte
 * except which of two hundred functions main called -- the last one declared
 * or the first -- took 7.80 s and 8.76 s for ten compiles. Forty-four cycles
 * a symbol walked, on a walk whose length is the program's, done once per
 * name the program mentions. Quadratic, and this machine is not fast enough
 * to carry that.
 *
 * Symbols are named by index and not by address. The array is grown with
 * realloc, so every Sym * in the program is invalidated by the next push, and
 * the two places that most want to hold on to a symbol -- a call waiting for
 * its callee's address, and a function definition waiting to record its own --
 * are both separated from their push by an arbitrary amount of parsing. Handing
 * out indices makes that safe by construction rather than by remembering.
 *
 * An index is the symbol's offset into the table in bytes, not its position.
 * Turning a position into an address is a multiply by the size of a Sym, which
 * on this target is `call __ishl` -- there is no barrel shifter -- and it was
 * paid wherever a symbol was touched: several times a name in the parser, and
 * in every step of the walk below. An offset needs only the add. So the
 * counts below are in bytes too, and step by the size of a Sym. */
Sym           *sym_table;     /* sym_at reads it directly */
static int     nsyms, nglobals, cap;    /* bytes, not symbols */

#ifdef ACC_HASH_STATS
unsigned long sym_probes;
#define COUNT_PROBE() (sym_probes++)
#else
#define COUNT_PROBE() ((void) 0)
#endif

void sym_init(void)
{
    cap = 64 * sizeof *sym_table;
    sym_table = malloc(cap);
    if (!sym_table)
        acc_error("out of memory for symbols");
    nsyms = nglobals = 0;
}

/* Out of line for the same reason out_grow is: sym_push runs for every name
 * the program declares, and inlining a realloc and an error string into it
 * buys a stack frame on every one of them to serve a path taken a handful of
 * times in a compile. The attribute is load-bearing -- called from one place,
 * it goes straight back inline without it. */
__attribute__((noinline))
static void syms_grow(void)
{
    cap *= 2;
    sym_table = realloc(sym_table, cap);
    if (!sym_table)
        acc_error("out of memory for symbols");
}

/* A symbol in the function's own region whatever its kind: an enum
 * constant, a typedef or a tag declared inside a function belongs to the
 * block it is declared in, where the same declaration at file scope does
 * not. */
static void fn_shift(int at, int bytes);

static inline __attribute__((always_inline))
int push_local(NameRef name, int kind, int val)
{
    Sym *sym;
    int at;

    if (nsyms == cap)
        syms_grow();
    at = nsyms;
    sym = sym_at(at);
    sym->name = name;
    sym->kind = (unsigned char) kind;
    sym->val = val;
    sym->type = TY_INT;     /* until the declaration says otherwise */
    sym->ext = 0;
    sym->quals = 0;
    nsyms += sizeof *sym;

    return at;
}

int sym_push_local(NameRef name, int kind, int val)
{
    return push_local(name, kind, val);
}

int sym_push(NameRef name, int kind, int val)
{
    Sym *sym;
    int at;

    if (sym_kind_local(kind))
        return push_local(name, kind, val);
    if (nsyms == cap)
        syms_grow();

    /* File scope. It goes in below the locals, which keeps every index
     * already handed out for a file-scope symbol -- the ones the code
     * generator is holding in its fixups -- pointing at the same symbol.
     * Only the locals move, and nothing holds a local's index across a push.
     * A function has a handful of them, so the move is a few dozen bytes. */
    at = nglobals;
    sym = sym_at(at);
    memmove(sym + 1, sym, (size_t) (nsyms - at));

    /* And what the table beside it holds for the locals -- an array's count
     * -- moves with them: `sizeof a` after a call to a function not yet
     * seen read another symbol's. */
    if (nsyms != at)            /* at file scope there are no locals */
        fn_shift(at, nsyms - at);
    sym->name = name;
    sym->kind = (unsigned char) kind;
    sym->val = val;
    sym->type = TY_INT;         /* a function called before it is defined is
                                 * assumed to return int, as C says */
    sym->ext = 0;
    sym->quals = 0;
    sym_set_params(at, 0, 0);   /* and to take nothing known */
    nsyms += sizeof *sym;
    nglobals += sizeof *sym;
    name_set_global(name, at);

    return at;
}

/* The file-scope region, in bytes: symbols [0, sym_nglobals()) stepping by
 * the size of a Sym. What an object file exports comes from this walk, and
 * it is the only thing outside sym.c that needs to know the table has two
 * regions. */
int sym_nglobals(void)
{
    return nglobals;
}

int sym_find(NameRef name)
{
    /* The locals, innermost first, so one shadows a file-scope name of the
     * same spelling. There are a handful and the walk stops at the first
     * match. The counter is unsigned so the loop test is not a signed
     * compare, which on this target is a helper call to repair the flags. */
    unsigned i = (unsigned) nsyms;

    while (i > (unsigned) nglobals) {
        i -= sizeof(Sym);
        COUNT_PROBE();
        if (sym_at(i)->name == name)
            return (int) i;
    }

    COUNT_PROBE();

    return name_global(name);
}


/* The end of a function. Functions do not nest, so there is exactly one
 * scope to drop and no mark to remember: everything above the file-scope
 * region is this function's. */
/* ------------------------------------------------------------------ */
/* what a function takes                                               */

/* The declared type of every parameter of every function, end to end, with
 * each function's Sym recording where its run starts and how long it is.
 *
 * A call has to convert each argument to the type the parameter was declared
 * with -- a long takes four bytes in two slots where an int takes three in
 * one, so without this a call to a function with a long parameter puts the
 * bytes in the wrong places and everything after it reads the wrong slot.
 * C89 would call that the programmer's fault for not writing a prototype;
 * C99, which acc is aimed at, requires the conversion.
 *
 * Kept beside the symbols rather than in them: a Sym is eight bytes, and two
 * more fields would make it twelve for every local as well as every
 * function. */
/* Two arrays of bytes rather than one of pairs: reaching the pair would be
 * a shift of the index, which is a call into the runtime on this target, on
 * every argument of every call. */
static Type          *param_type;
static unsigned char *param_ext;       /* a struct's, which says how big */
static unsigned param_used, param_cap;

/* Where each function's run starts and how long it is, indexed by its symbol.
 * File-scope symbols keep their index when others are added, so this stays
 * lined up with them.
 *
 * A record is as wide as a Sym, so that a symbol's index -- its offset into
 * the symbol table -- is also its signature's offset into this one, and
 * finding it is an add like sym_at. */
typedef union {
    struct {
        int           first;
        unsigned char count;
        unsigned char flags;    /* SYMF_* */
    } s;
    unsigned char size[sizeof(Sym)];
} FnSig;

typedef char fn_sig_is_a_sym_wide[sizeof(FnSig) == sizeof(Sym) ? 1 : -1];

static unsigned char *fn_sigs;
static unsigned       fn_cap;           /* bytes */

#define fn_sig(sym)  ((FnSig *) (fn_sigs + (sym)))

static void fn_room(unsigned want)
{
    if (want < fn_cap)
        return;
    while (fn_cap <= want)
        fn_cap = fn_cap ? fn_cap * 2 : 64 * sizeof(FnSig);
    fn_sigs = realloc(fn_sigs, fn_cap);
    if (!fn_sigs)
        acc_error("out of memory for the function signatures");
}

/* The records from `at` on, `bytes` of them, one symbol's width further up:
 * see sym_push. */
static void fn_shift(int at, int bytes)
{
    fn_room((unsigned) (at + bytes + sizeof(FnSig)));
    memmove(fn_sigs + at + sizeof(FnSig), fn_sigs + at, (size_t) bytes);
}

void sym_set_params(int sym, int first, int count)
{
    fn_room((unsigned) sym);
    fn_sig(sym)->s.first = first;
    fn_sig(sym)->s.count = (unsigned char) count;
    fn_sig(sym)->s.flags = 0;   /* whoever sets the parameters says the rest */
}

void sym_set_flags(int sym, int flags)
{
    fn_room((unsigned) sym);
    fn_sig(sym)->s.flags |= (unsigned char) flags;
}

unsigned char sym_flags(int sym)
{
    return fn_sig(sym)->s.flags;
}

/* The reads need no room check: a symbol only has a signature because
 * sym_push or sym_set_params made room for it first, and both run before any
 * call site can ask. Checking on every read cost a compare and a branch on
 * the path of every argument of every call. */
int sym_params_first(int sym)
{
    return fn_sig(sym)->s.first;
}

int sym_nparams(int sym)
{
    return fn_sig(sym)->s.count;
}

int sym_params_begin(void)
{
    return (int) param_used;
}

void sym_param_add(Type type, int ext)
{
    if (param_used == param_cap) {
        param_cap = param_cap ? param_cap * 2 : 64;
        param_type = realloc(param_type, param_cap);
        param_ext = realloc(param_ext, param_cap);
        if (!param_type || !param_ext)
            acc_error("out of memory for the parameter types");
    }
    param_type[param_used] = type;
    param_ext[param_used] = (unsigned char) ext;
    param_used++;
}

Type sym_param_type(int first, int index)
{
    return param_type[first + index];
}

int sym_param_ext(int first, int index)
{
    return param_ext[first + index];
}

/* The mark is a count of bytes above the file-scope symbols rather than a
 * position in the table, because a file-scope symbol pushed inside the scope
 * -- a call to a function not seen yet -- goes in underneath the locals and
 * moves them all along by one. */
int sym_scope_begin(void)
{
    return nsyms - nglobals;
}

void sym_scope_end(int mark)
{
    nsyms = nglobals + mark;
}

int sym_declared_in(int sym, int mark)
{
    if (mark < 0)
        return sym < nglobals;

    return sym >= nglobals + mark;
}

void sym_set_count(int sym, int count)
{
    fn_room((unsigned) sym);
    fn_sig(sym)->s.first = count;
}

int sym_count(int sym)
{
    return fn_sig(sym)->s.first;
}

/* ------------------------------------------------------------------ */
/* extended types                                                      */

/* The table TY_EXT's and TY_STRUCT's extension indexes, from 1; 0 is no
 * extension. An array is an element type with its own extension, a count,
 * and its size, worked out once. A struct or union is its members, as a
 * chain through the member table below, its size, and its tag for the
 * diagnostics; it is incomplete -- declared, but its members not yet given
 * -- until ext_record_done. */
#define NEXT_TYPES 255

/* Eight bytes an entry, and the rest in arrays of their own: an entry is
 * found by a shift of its index, which a width that is not a power of two
 * turns into a call to multiply -- and ext_bytes is on the path of every
 * pointer step. The byte arrays need neither. */
static struct {
    Type          elem;
    unsigned char elem_x;
    int           count;        /* elements, or a record's first member */
    int           bytes;
} ext_types[NEXT_TYPES + 1];

enum { EXT_ARRAY = 0, EXT_STRUCT = 1, EXT_UNION = 2, EXT_COMPLETE = 4,
       EXT_BITS = 8, EXT_FUNC = 16, EXT_FLEX = 32 };

static unsigned char ext_what[NEXT_TYPES + 1];      /* the kind, and whether
                                                     * a record is complete */
static NameRef       ext_tags[NEXT_TYPES + 1];

static int next_types;

static int ext_new(void)
{
    if (next_types == NEXT_TYPES)
        acc_error_at(tok_line, "this program has more than %d different "
                               "array shapes and structs, which acc cannot "
                               "tell apart yet", NEXT_TYPES);

    return ++next_types;
}

int ext_array(Type elem, int elem_x, int count)
{
    int i;

    for (i = 1; i <= next_types; i++)
        if (!(ext_what[i] & (EXT_STRUCT | EXT_UNION | EXT_FUNC))
            && ext_types[i].elem == elem
            && ext_types[i].elem_x == elem_x && ext_types[i].count == count)
            return i;

    if ((long) count * type_bytes(elem, elem_x) > 0x7fffff)
        acc_error_at(tok_line, "an array this large does not fit in memory");

    i = ext_new();
    ext_types[i].elem = elem;
    ext_types[i].elem_x = (unsigned char) elem_x;
    ext_what[i] = EXT_ARRAY | EXT_COMPLETE;
    ext_types[i].count = count;
    ext_types[i].bytes = count * type_bytes(elem, elem_x);

    return i;
}

/* A function type: its result, and its parameters as a run of the
 * parameter table -- `count` of them from `first`, `declared` whether they
 * were given at all. Interned by what they are, not where the run is. */
int ext_func(Type ret, int ret_x, int first, int count, int declared)
{
    int i, j;

    for (i = 1; i <= next_types; i++) {
        if (!(ext_what[i] & EXT_FUNC) || ext_types[i].elem != ret
            || ext_types[i].elem_x != ret_x
            || ext_types[i].bytes != (count | declared << 8))
            continue;
        for (j = 0; j < count; j++)
            if (param_type[ext_types[i].count + j] != param_type[first + j]
                || param_ext[ext_types[i].count + j] != param_ext[first + j])
                break;
        if (j == count)
            return i;
    }
    i = ext_new();
    ext_types[i].elem = ret;
    ext_types[i].elem_x = (unsigned char) ret_x;
    ext_types[i].count = first;
    ext_types[i].bytes = count | declared << 8;
    ext_what[i] = EXT_FUNC | EXT_COMPLETE;

    return i;
}

int ext_func_first(int x)
{
    return ext_types[x].count;
}

int ext_func_count(int x)
{
    return ext_types[x].bytes & 0xff;
}

int ext_func_declared(int x)
{
    return ext_types[x].bytes >> 8 & 1;
}

int ext_func_variadic(int x)
{
    return ext_types[x].bytes >> 9 & 1;
}

Type ext_elem(int x)
{
    return ext_types[x].elem;
}

int ext_elem_x(int x)
{
    return ext_types[x].elem_x;
}

int ext_count(int x)
{
    return ext_types[x].count;
}

int ext_bytes(int x)
{
    return ext_types[x].bytes;
}

/* ------------------------------------------------------------------ */
/* structs and unions                                                  */

/* Every member of every struct, each record's a chain from its first. A
 * member is named by its offset into the table in bytes, as a symbol is, so
 * that reaching one is an add and not a multiply by eleven. */
typedef struct {
    NameRef       name;
    int           offset;
    int           next;         /* the record's next member, or -1 */
    Type          type;
    unsigned char ext;
    unsigned char quals;        /* SQ_CONST, when the member's type is */
    unsigned char bits;         /* a bit-field's descriptor, 0 if it is not
                                 * one: see bitfield_at */
} Member;

static Member *members;
static int     members_used, members_cap;       /* bytes */

#define member_at(m)  ((Member *) ((char *) members + (m)))

int ext_record(int is_union, NameRef tag)
{
    int x = ext_new();

    ext_what[x] = is_union ? EXT_UNION : EXT_STRUCT;
    ext_types[x].count = -1;
    ext_types[x].bytes = 0;
    ext_tags[x] = tag;

    return x;
}

int ext_is_union(int x)
{
    return (ext_what[x] & EXT_UNION) != 0;
}

void ext_set_bits(int x)
{
    ext_what[x] |= EXT_BITS;
}

int ext_has_bits(int x)
{
    return (ext_what[x] & EXT_BITS) != 0;
}

/* Whether the last member is an array with no size, which makes the record
 * one that cannot be a member of another or an element of an array: what
 * follows it in memory is the caller's business, not the type's. */
void ext_set_flex(int x)
{
    ext_what[x] |= EXT_FLEX;
}

int ext_has_flex(int x)
{
    return (ext_what[x] & EXT_FLEX) != 0;
}

int ext_complete(int x)
{
    return (ext_what[x] & EXT_COMPLETE) != 0;
}

NameRef ext_tag(int x)
{
    return ext_tags[x];
}

void ext_record_done(int x, int first, int bytes)
{
    ext_types[x].count = first;
    ext_types[x].bytes = bytes;
    ext_what[x] |= EXT_COMPLETE;
}

int member_add(NameRef name, Type type, int ext, int offset, int quals)
{
    Member *m;
    int at = members_used;

    if (members_used == members_cap) {
        members_cap = members_cap ? members_cap * 2 : 32 * (int) sizeof *m;
        members = realloc(members, (size_t) members_cap);
        if (!members)
            acc_error("out of memory for struct members");
    }
    m = member_at(at);
    m->name = name;
    m->offset = offset;
    m->next = -1;
    m->type = type;
    m->ext = (unsigned char) ext;
    m->quals = (unsigned char) quals;
    m->bits = 0;
    members_used += sizeof *m;

    return at;
}

void member_link(int member, int next)
{
    member_at(member)->next = next;
}

int member_first(int x)
{
    return ext_types[x].count;
}

int member_next(int member)
{
    return member_at(member)->next;
}

int member_find(int x, NameRef name)
{
    int m;

    for (m = ext_types[x].count; m >= 0; m = member_at(m)->next)
        if (member_at(m)->name == name)
            return m;

    return -1;
}

NameRef member_name(int member)
{
    return member_at(member)->name;
}

Type member_type(int member)
{
    return member_at(member)->type;
}

int member_ext(int member)
{
    return member_at(member)->ext;
}

void member_set_bits(int member, int bits)
{
    member_at(member)->bits = (unsigned char) bits;
}

int member_bits(int member)
{
    return member_at(member)->bits;
}

/* ------------------------------------------------------------------ */
/* bit-fields                                                          */

/* Every distinct bit-field shape, from 1: where in its bytes it starts,
 * how wide it is, how many bytes hold it, and whether it is signed. A value
 * that is a bit-field's address carries its index, as it carries an
 * extension. */
#define NBITFIELDS 255

static BitField bitfields[NBITFIELDS + 1];
static int      nbitfields;

int bitfield_intern(int pos, int width, int is_signed)
{
    int i, bytes = (pos + width + 7) / 8;

    for (i = 1; i <= nbitfields; i++)
        if (bitfields[i].pos == pos && bitfields[i].width == width
            && bitfields[i].is_signed == is_signed)
            return i;
    if (nbitfields == NBITFIELDS)
        acc_error_at(tok_line, "more than %d different bit-fields", NBITFIELDS);
    nbitfields++;
    bitfields[nbitfields].pos = (unsigned char) pos;
    bitfields[nbitfields].width = (unsigned char) width;
    bitfields[nbitfields].bytes = (unsigned char) bytes;
    bitfields[nbitfields].is_signed = (unsigned char) is_signed;

    return nbitfields;
}

const BitField *bitfield_at(int i)
{
    return &bitfields[i];
}

int member_quals(int member)
{
    return member_at(member)->quals;
}

int member_offset(int member)
{
    return member_at(member)->offset;
}

void sym_drop_locals(void)
{
    nsyms = nglobals;
}
