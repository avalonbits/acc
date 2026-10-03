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
 * Each name carries its symbol with it in the name arena, so finding one
 * is a load: its file-scope symbol, or while a local has the name, that
 * local -- and the field's value from before the local is kept beside it
 * (see name_bind), for the local's scope to put back when it ends. A walk
 * of the locals was quadratic in a block's size: every name declared or
 * read in it walked back over the rest.
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
/* Bytes, not symbols. The first two are seen outside, so that opening and
 * closing a scope -- which every statement's body does -- is inline: see
 * sym_scope_begin in acc.h. */
int            sym_nbytes, sym_nglobal_bytes;
static int     cap;

/* The three bytes in front of a name's text: its file-scope symbol plus
 * one, 0 for none -- or, NAME_LOCAL and up, the local that has the name
 * now, as its offset above the file-scope region, which a file-scope push
 * moving the locals along does not change. What the field held before
 * that local took it is in `shadow`, at the local's offset: a table as
 * wide as the locals, only ever as long as the most a function has. */
#define NAME_LOCAL 0x400000

static unsigned char *shadow;
static int     shadow_cap;

static unsigned char *name_field(NameRef ref)
{
    return (unsigned char *) name_arena + ref - 3;
}

/* Whether a field holds a local: one at NAME_LOCAL or above, compared
 * unsigned. Not the top bit, which is the sign of a 24-bit int and makes
 * any test of it a signed one -- a helper call here. The local's offset is
 * the field less NAME_LOCAL, where a mask would be `call __iand`. */
static int field_local(const unsigned char *field)
{
    return (unsigned) get24(field) >= NAME_LOCAL;
}

/* `name` now the local at offset `local` above the file-scope region. */
__attribute__((noinline))
static void name_bind(NameRef name, int local)
{
    if ((unsigned) local + sizeof(Sym) > (unsigned) shadow_cap) {
        shadow_cap = shadow_cap ? 2 * shadow_cap : 64 * sizeof(Sym);
        shadow = realloc(shadow, shadow_cap);
        if (!shadow)
            acc_error("out of memory for symbols");
    }
    put24(shadow + local, get24(name_field(name)));
    put24(name_field(name), NAME_LOCAL + local);
}

/* The field under every local that has the name: the file-scope part. */
static unsigned char *global_field(NameRef ref)
{
    unsigned char *field = name_field(ref);

    while (field_local(field))
        field = shadow + (get24(field) - NAME_LOCAL);

    return field;
}

int name_global(NameRef ref)
{
    return get24(global_field(ref)) - 1;
}

/* The locals from `mark` on gone, the newest first: each name's field is
 * what it was before its local took it. */
void sym_scope_end(int mark)
{
    int local = sym_nbytes - sym_nglobal_bytes;

    while (local != mark) {
        local -= sizeof(Sym);
        put24(name_field(sym_at(sym_nglobal_bytes + local)->name),
              get24(shadow + local));
    }
    sym_nbytes = sym_nglobal_bytes + mark;
}

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
    sym_nbytes = sym_nglobal_bytes = 0;
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
static inline __attribute__((always_inline))
int push_local(NameRef name, int kind, int val)
{
    Sym *sym;
    int at;

    if (sym_nbytes == cap)
        syms_grow();
    at = sym_nbytes;
    sym = sym_at(at);
    sym->name = name;
    sym->kind = (unsigned char) kind;
    sym->val = val;
    sym->type = TY_INT;     /* until the declaration says otherwise */
    sym->ext = 0;
    sym->quals = 0;
    sym->first = 0;
    sym->count = 0;
    sym->flags = 0;
    sym_nbytes += sizeof *sym;
    name_bind(name, at - sym_nglobal_bytes);

    return at;
}

int sym_push_local(NameRef name, int kind, int val)
{
    return push_local(name, kind, val);
}

/* A parameter, local from `mark` on as each of its list is, and none of
 * the others' names: SYM_NONE when one before it has its name. */
int sym_push_param(NameRef name, int val, int mark)
{
    if (sym_find_in(name, mark) != SYM_NONE)
        return SYM_NONE;

    return push_local(name, SYM_LOCAL, val);
}

int sym_push(NameRef name, int kind, int val)
{
    Sym *sym;
    int at;

    if (sym_kind_local(kind))
        return push_local(name, kind, val);
    if (sym_nbytes == cap)
        syms_grow();

    /* File scope. It goes in below the locals, which keeps every index
     * already handed out for a file-scope symbol -- the ones the code
     * generator is holding in its fixups -- pointing at the same symbol.
     * Only the locals move, and nothing holds a local's index across a push.
     * A function has a handful of them, so the move is a few dozen bytes. */
    at = sym_nglobal_bytes;
    sym = sym_at(at);
    memmove(sym + 1, sym, (size_t) (sym_nbytes - at));
    sym->name = name;
    sym->kind = (unsigned char) kind;
    sym->val = val;
    sym->type = TY_INT;         /* a function called before it is defined is
                                 * assumed to return int, as C says */
    sym->ext = 0;
    sym->quals = 0;
    sym->first = 0;             /* and to take nothing known */
    sym->count = 0;
    sym->flags = 0;
    sym_nbytes += sizeof *sym;
    sym_nglobal_bytes += sizeof *sym;
    put24(global_field(name), at + 1);

    return at;
}

/* The file-scope region, in bytes: symbols [0, sym_nglobals()) stepping by
 * the size of a Sym. What an object file exports comes from this walk, and
 * it is the only thing outside sym.c that needs to know the table has two
 * regions. */
int sym_nglobals(void)
{
    return sym_nglobal_bytes;
}

int sym_find(NameRef name)
{
    const unsigned char *at = name_field(name);
    int field = get24(at);

    COUNT_PROBE();
    if (field_local(at))
        return sym_nglobal_bytes + (field - NAME_LOCAL);

    return field - 1;
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
 * The types are kept beside the symbols rather than in them: a function
 * takes any number, and each Sym says where its run starts. */
/* Two arrays of bytes rather than one of pairs: reaching the pair would be
 * a shift of the index, which is a call into the runtime on this target, on
 * every argument of every call. */
static Type          *param_type;
static unsigned char *param_ext;       /* a struct's, which says how big */
static unsigned param_used, param_cap;

void sym_set_params(int sym, int first, int count)
{
    sym_at(sym)->first = first;
    sym_at(sym)->count = (unsigned char) count;

    /* Whoever sets the parameters says the rest of what is known about the
     * declaration -- but not whether something has already wanted it. A
     * `static` called before it is defined is called all the same, and the
     * definition clearing that took it out of the image from under the
     * call. */
    sym_at(sym)->flags &= SYMF_USED;
}

void sym_set_flags(int sym, int flags)
{
    sym_at(sym)->flags |= (unsigned char) flags;
}

/* Taking one back, which only extern needs: a declaration that says it
 * leaves the defining to another file, and then one in the same file that
 * does not. */
void sym_clear_flags(int sym, int flags)
{
    sym_at(sym)->flags &= (unsigned char) ~flags;
}

unsigned char sym_flags(int sym)
{
    return sym_at(sym)->flags;
}

/* A symbol's signature is in its own record, so a read is a load. */
int sym_params_first(int sym)
{
    return sym_at(sym)->first;
}

int sym_nparams(int sym)
{
    return sym_at(sym)->count;
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
/* The local `name` in the scope from `mark` on, or SYM_NONE: the newest
 * local with the name, where it is in that scope -- none older can be
 * where the newest is not. */
int sym_find_in(NameRef name, int mark)
{
    const unsigned char *at = name_field(name);
    int field = get24(at);

    if (field_local(at) && (unsigned) (field - NAME_LOCAL) >= (unsigned) mark)
        return sym_nglobal_bytes + (field - NAME_LOCAL);

    return SYM_NONE;
}

int sym_declared_in(int sym, int mark)
{
    if (mark < 0)
        return sym < sym_nglobal_bytes;

    return sym >= sym_nglobal_bytes + mark;
}

void sym_set_count(int sym, int count)
{
    sym_at(sym)->first = count;
}

int sym_count(int sym)
{
    return sym_at(sym)->first;
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
       EXT_BITS = 8, EXT_FUNC = 16, EXT_FLEX = 32, EXT_VLA = 64 };

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
        if (!(ext_what[i] & (EXT_STRUCT | EXT_UNION | EXT_FUNC | EXT_VLA))
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

    /* An array of unknown size, count -1, `int (*p)[]` points at: its size
     * is nothing anyone may ask, and a negative one here would be read as
     * a VLA's frame slot. */
    ext_types[i].bytes = count > 0 ? count * type_bytes(elem, elem_x) : 0;

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

/* An array whose length the program works out as it runs: the row of `int
 * m[n][n]`, which is int[n], or a typedef of one. Its length and its size in
 * bytes are in two frame slots, where its declaration put them, rather than
 * here -- so each is its own, never one another declaration shares, and
 * ext_count and ext_bytes have nothing to say about it. */
int ext_vla(Type elem, int elem_x, int length_slot, int size_slot)
{
    int i = ext_new();

    ext_types[i].elem = elem;
    ext_types[i].elem_x = (unsigned char) elem_x;
    ext_what[i] = EXT_ARRAY | EXT_COMPLETE | EXT_VLA;
    ext_types[i].count = length_slot;
    ext_types[i].bytes = size_slot;

    return i;
}

/* The slot a VLA's size is in, or 0 for any other extension: a frame
 * slot is below the frame pointer, so 0 is never one. */
int ext_vla_size(int x)
{
    return ext_what[x] & EXT_VLA ? ext_types[x].bytes : 0;
}

/* Whether a type is variably modified (C99 6.7.5p3): a VLA, or made from
 * one -- an array of them, a pointer to one, a function returning one.
 * A pointer's levels are in its Type, so only the chain of extensions is
 * walked. A struct stops it, since a member cannot be one. */
int ext_variably_modified(int x)
{
    while (x) {
        if (ext_what[x] & EXT_VLA)
            return 1;
        if (ext_what[x] & (EXT_STRUCT | EXT_UNION))
            return 0;
        x = ext_types[x].elem_x;        /* an element, or a result */
    }

    return 0;
}

int ext_vla_length(int x)
{
    return ext_types[x].count;
}

/* A parameter's row whose length is worked out when its function is
 * entered: made by ext_vla with no slots while the parameters are read --
 * an array of unknown size till then -- and given them by ext_vla_fill
 * once the function has a frame to put them in. */
int ext_vla_pending(int x)
{
    return (ext_what[x] & EXT_VLA) && !ext_types[x].bytes;
}

void ext_vla_fill(int x, int length_slot, int size_slot)
{
    ext_types[x].count = length_slot;
    ext_types[x].bytes = size_slot;
}

/* Whether two extensions are of compatible types, as a function's
 * declarations are held to: the same, or arrays of the same element whose
 * length one of them does not say -- `int (*)[]` and `int (*)[3]`, C99
 * 6.7.5.2p6 -- or that the program works out. */
int ext_compatible(int a, int b)
{
    if (a == b)
        return 1;
    if (!a || !b || (ext_what[a] & (EXT_STRUCT | EXT_UNION | EXT_FUNC))
        || (ext_what[b] & (EXT_STRUCT | EXT_UNION | EXT_FUNC)))
        return 0;
    if (ext_types[a].elem != ext_types[b].elem
        || !ext_compatible(ext_types[a].elem_x, ext_types[b].elem_x))
        return 0;

    return ext_types[a].count < 0 || ext_types[b].count < 0
           || (ext_what[a] & EXT_VLA) || (ext_what[b] & EXT_VLA)
           || ext_types[a].count == ext_types[b].count;
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

/* In chunks that never move, as the names are (see names_chunk in names.c):
 * a member is its offset from `members`, which is the first chunk and stays
 * put, and a struct's members are a chain through their `next` offsets, so
 * nothing needs them side by side. Grown as one block by realloc, the
 * members of an Agon program's headers took 13 KB to 26 KB, holding both at
 * once while they did. On the Agon the first chunk is static, below every
 * chunk malloc hands out; on the host every chunk is cut, in order, from
 * one block reserved at the start. */
#define MEMBER_CHUNK (78 * (int) sizeof(Member))

static char *members;
static char *members_at, *members_end;  /* the current chunk's free part */

#ifdef AGONDEV
static char members_first[MEMBER_CHUNK];
static char *members_more;              /* the malloc'd chunks, newest first */
#else
#define MEMBER_RESERVE (16L * 1024 * 1024)
static char *members_reserve;
#endif

#define member_at(m)  ((Member *) ((char *) members + (m)))

__attribute__((noinline))
static void members_chunk(void)
{
    if (!members) {
#ifdef AGONDEV
        members = members_first;
#else
        members = malloc(MEMBER_RESERVE);
        if (!members)
            acc_error("out of memory for struct members");
        members_reserve = members + MEMBER_CHUNK;
#endif
        members_at = members;
    } else {
#ifdef AGONDEV
        /* Each starts with the one malloc'd before it, for
         * sym_members_free, and its members after that. */
        char *chunk = malloc(sizeof(char *) + MEMBER_CHUNK);

        if (!chunk)
            acc_error("out of memory for struct members");
        *(char **) chunk = members_more;
        members_more = chunk;
        members_at = chunk + sizeof(char *);
#else
        if (members_reserve + MEMBER_CHUNK > members + MEMBER_RESERVE)
            acc_error("more struct members than the host build reserved "
                      "room for");
        members_at = members_reserve;
        members_reserve += MEMBER_CHUNK;
#endif
    }
    members_end = members_at + MEMBER_CHUNK;
}

/* The members let go of, once the source has been read: nothing after it --
 * the object's writer, or a link -- asks about a struct's members. */
void sym_members_free(void)
{
#ifdef AGONDEV
    char *chunk;

    /* Every chunk but the first, which is static, through the list
     * members_chunk keeps. */
    while (members_more) {
        chunk = members_more;
        members_more = *(char **) chunk;
        free(chunk);
    }
#else
    free(members);
#endif
    members = members_at = members_end = NULL;
}

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
    int at;

    if (members_end - members_at < (int) sizeof *m)
        members_chunk();
    at = (int) (members_at - members);
    m = member_at(at);
    m->name = name;
    m->offset = offset;
    m->next = -1;
    m->type = type;
    m->ext = (unsigned char) ext;
    m->quals = (unsigned char) quals;
    m->bits = 0;
    members_at += sizeof *m;

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
    sym_scope_end(0);
}
