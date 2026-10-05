/*
 * Statements: blocks, if, the loops, switch and its cases, goto and
 * labels -- and a jump into a variable-length array's scope refused --
 * break, continue and return.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define ACC_FRONT       /* the parser: see gen.h */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc.h"
#include "ctype.h"
#include "fmt.h"
#include "timing.h"
#include "version.h"
#include "parse_int.h"

/* The room a goto back to a label gives up: the room taken, since the label
 * was reached, by the blocks the goto is leaving. `then` is the chain of
 * blocks open at the label, with their marks as they were.
 *
 * The two chains share their outer blocks and part at the innermost one
 * they have in common. That block's room counts only if its mark was taken
 * after the label; every block inside it on the goto's side opened after
 * the label, so all of theirs counts. What goes back is everything from the
 * outermost of those, whose mark is the highest the stack was. Answers that
 * mark, or NO_VLA_MARK for nothing. gcc's vla-dealloc-1 jumps to a label in
 * an `if (0)` block that closed before the array was declared -- the goto
 * is not leaving the label's block, but it is leaving the array's scope. */
static int vla_back_to(const VlaBlock *then, int nthen)
{
    int common = -1, i;

    for (i = 0; i < nvla_blocks && i < nthen; i++) {
        if (vla_blocks[i].serial != then[i].serial)
            break;
        common = i;
    }
    if (common < 0)
        return NO_VLA_MARK;
    if (then[common].mark == NO_VLA_MARK
        && vla_blocks[common].mark != NO_VLA_MARK)
        return vla_blocks[common].mark;
    for (i = common + 1; i < nvla_blocks; i++)
        if (vla_blocks[i].mark != NO_VLA_MARK)
            return vla_blocks[i].mark;

    return NO_VLA_MARK;
}

/* Whether a jump reaching the blocks open now goes into the scope of a
 * variably modified declaration, from a goto made when vla_serial was
 * `then`. A block open at both whose last one is newer was declared
 * between the two, and so was any in a block opened since. */
static int vm_forward_in(int then)
{
    VlaBlock *b;

    for (b = vla_blocks; b < vla_top; b++)
        if (vm_since(b, then))
            return 1;

    return 0;
}

/* And whether a jump back to a label, whose blocks were `to`, does: one
 * those blocks had declared by then is out of scope here only if its block
 * is not open now. */
static int vm_back_in(const VlaBlock *to, int nto)
{
    int i;

    for (i = 0; i < nvla_blocks && i < nto
                && vla_blocks[i].serial == to[i].serial; i++)
        ;
    for (; i < nto; i++)
        if (vm_since(&to[i], 0))
            return 1;

    return 0;
}

static void vm_jump_refused(int line, int col)
{
    acc_error_pos(line, col, "this goto jumps past the declaration of an "
                             "array whose length is worked out as it runs, "
                             "into its scope");
}

static void statement(void);

/* The body of a selection or an iteration statement, which is a block of
 * its own whether it has braces or not (C99 6.8.4p3, 6.8.5p5): a tag or a
 * compound literal it declares ends with it. c99-scope-2 defines a new
 * `struct foo` in each. */
static inline __attribute__((always_inline))
void substatement(void)
{
    int mark = sym_scope_begin(), outer = scope_mark;

    scope_mark = mark;
    statement();
    sym_scope_end(mark);
    scope_mark = outer;
}
static void condition(void);

#ifdef OPT_ACC
/* A body read in place of a call (inline.c): a statement of its own. */
void inline_statement(void)
{
    substatement();
}
#endif

/* ------------------------------------------------------------------ */
/* break, continue, and the cases of a switch                          */

/* Jumps waiting for an address that is not known yet: a `break` for the end
 * of the loop or switch it leaves, and a `continue` in a do-while for the
 * condition, which comes after the body. Each construct notes how many there
 * were when it began and fills in the ones above that when it ends, so
 * nesting takes care of itself: the inner one has always finished with its
 * own before the outer one looks. */
typedef struct {
    int *at;
    int  count, cap;
} Holes;

static Holes breaks, continues;

static void hole_push(Holes *h, int hole)
{
    if (h->count == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 16;
        h->at = realloc(h->at, (size_t) h->cap * sizeof *h->at);
        if (!h->at)
            acc_error("out of memory for jumps");
    }
    h->at[h->count++] = hole;
}

/* Every hole above `mark`, filled in with here. */
static void holes_land(Holes *h, int mark)
{
    while (h->count > mark)
        gen_label(h->at[--h->count]);
}

/* What `break` and `continue` mean where the parser is now. A loop sets all
 * three; a switch sets only where a break goes, which is why a `continue` in
 * a switch in a loop continues the loop. */
typedef struct {
    int break_mark;     /* breaks from here up are this construct's; -1: none */
    int continue_mark;  /* the same for continues; -1: not in a loop */
    int continue_to;    /* where a continue goes, when that is already known;
                         * -1 when it is further on, as in a do-while */
} Jumps;

static Jumps jumps = { -1, -1, -1 };

/* A switch the parser is inside, for the case labels in it -- which may be
 * anywhere in its body, inside loops and blocks, and in Duff's device are. */
typedef struct {
    int  case_mark;     /* its cases are the ones from here up; -1: none */
    int  default_at;    /* where `default:` is, -1 if there is none yet */
    Type type;          /* what the cases are converted to */
    int  blocks;        /* the blocks open at the switch, in bytes: an
                         * entry's index is a multiply by its size */
} Switch;

static Switch in_switch = { -1, -1, TY_INT, 0 };

/* That a case label is not in the scope of a variably modified declaration
 * the switch is not (C99 6.8.4.2p2): every block opened since the switch
 * is inside it, so none of them may have made one yet. */
__attribute__((noinline))
static void case_in_scope(int line, const char *spot)
{
    VlaBlock *b;

    for (b = (VlaBlock *) ((char *) vla_blocks + in_switch.blocks);
         b < vla_top; b++)
        if (vm_since(b, 0))
            acc_error_spot(line, spot, "the switch would jump to this label "
                                       "past the declaration of an array "
                                       "whose length is worked out as it "
                                       "runs, into its scope");
}

static long     *case_value;
static uint32_t *case_high;     /* the top four bytes, for a long long */
static int  *case_at;
static int   ncases, cases_cap;

/* The cases by value, for the check that one is not there twice, in a
 * switch with more than CASES_WALKED of them: open addressing over a table
 * at least twice as big as what it holds, an entry a case's index plus
 * one, or 0 for none. One left from a switch that is done may name an
 * index a later case has; it is a match only where that case is this
 * switch's and the same. Compared with every case before it, a switch of
 * 256 was 33,000 compares of a long. */
static int *case_slots, *case_slots_end;
static int  case_slots_cap, case_slots_used;

#define CASES_WALKED 16          /* compared with each before them, below */

/* The slot of a case with `value`, from index `mark` up to ncases, or the
 * empty one where it would go: hashed by the value's low bytes, cases being
 * mostly small and in a run, which the mask alone spreads. */
static int *case_slot(long value, uint32_t high, int mark)
{
    int *p = case_slots + ((unsigned) value & (unsigned) (case_slots_cap - 1));
    int k;

    while ((k = *p) != 0
           && !((unsigned) (k - 1 - mark) < (unsigned) (ncases - mark)
                && case_value[k - 1] == value && case_high[k - 1] == high))
        if (++p == case_slots_end)
            p = case_slots;

    return p;
}

/* Cases `from` to ncases put in the table -- all of them again, into one
 * twice the size, when it would be more than half full. */
__attribute__((noinline))
static void case_slots_add(int from)
{
    if ((unsigned) (2 * (case_slots_used + ncases - from)) >= (unsigned) case_slots_cap) {
        free(case_slots);
        case_slots_cap = 4 * (ncases + 8);
        case_slots = calloc((size_t) case_slots_cap, sizeof *case_slots);
        if (!case_slots)
            acc_error("out of memory for case labels");
        case_slots_end = case_slots + case_slots_cap;
        case_slots_used = 0;
        from = 0;
    }
    for (; from != ncases; from++, case_slots_used++)
        *case_slot(case_value[from], case_high[from], ncases) = from + 1;
}

/* The contexts of the loops and switches the parser is inside, outermost
 * first, three ints apiece on a stack walked by a pointer.
 *
 * Kept here rather than in the frames of the functions that parse them: one
 * in for_statement's frame made it nine bytes larger and a compile of a
 * program full of for loops 3% slower on the Agon. And kept as ints through
 * a pointer rather than as an array of Jumps, which cost a multiply by the
 * size of one -- a call on this target -- and a copy of it, each way: about
 * 1100 cycles a loop. */
static int *jump_stack, *jump_top, *jump_limit;

__attribute__((noinline))
static void jumps_grow(void)
{
    int used = (int) (jump_top - jump_stack);
    int cap = used ? used * 2 : 24;

    jump_stack = realloc(jump_stack, (size_t) cap * sizeof *jump_stack);
    if (!jump_stack)
        acc_error("out of memory for nested loops");
    jump_top = jump_stack + used;
    jump_limit = jump_stack + cap;
}

static inline __attribute__((always_inline)) void jumps_push(void)
{
    if (jump_top == jump_limit)
        jumps_grow();
    jump_top[0] = jumps.break_mark;
    jump_top[1] = jumps.continue_mark;
    jump_top[2] = jumps.continue_to;
    jump_top += 3;
}

static inline __attribute__((always_inline)) void jumps_pop(void)
{
    jump_top -= 3;
    jumps.break_mark = jump_top[0];
    jumps.continue_mark = jump_top[1];
    jumps.continue_to = jump_top[2];
}

static void loop_begin(int continue_to)
{
    jumps_push();
    jumps.break_mark = breaks.count;
    jumps.continue_mark = continues.count;
    jumps.continue_to = continue_to;
}

/* The end of a loop: its breaks land here, which is past everything in it. */
static void loop_end(void)
{
    holes_land(&breaks, jumps.break_mark);
    jumps_pop();
}

__attribute__((noinline))
static void break_statement(void)
{
    int line = tok_line;
    const char *spot = tok_at;

    next();
    expect(TK_SEMI, "';'");
    if (jumps.break_mark < 0)
        acc_error_spot(line, spot, "'break' is not inside a loop or a switch");
    if (vla_mark != NO_VLA_MARK)
        gen_stack_back(vla_mark);
    hole_push(&breaks, gen_jump());
}

__attribute__((noinline))
static void continue_statement(void)
{
    int line = tok_line;
    const char *spot = tok_at;

    next();
    expect(TK_SEMI, "';'");
    if (jumps.continue_mark < 0)
        acc_error_spot(line, spot, "'continue' is not inside a loop");
    if (vla_mark != NO_VLA_MARK)
        gen_stack_back(vla_mark);
    if (jumps.continue_to >= 0)
        gen_jump_to(jumps.continue_to);
    else
        hole_push(&continues, gen_jump());
}

/* `while (condition) body`, out of statement() for the reason for_statement
 * is: its saved jumps would otherwise be in the frame of every statement. */
__attribute__((noinline))
static void while_statement(void)
{
    int top, to_end, mark = sym_scope_begin();     /* see TK_KW_IF */
    int outer = scope_mark;

    scope_mark = mark;
    next();
    top = gen_here();
    condition();
    to_end = gen_jump_if_false();
    loop_begin(top);
    substatement();
    gen_jump_to(top);
    gen_label(to_end);
    loop_end();
    sym_scope_end(mark);
    scope_mark = outer;
}

/* `do body while (condition);` -- the body first, then the test, and back to
 * the top while it holds. A continue goes to the test, which is only reached
 * once the body has been read, so it waits in the list with the breaks. */
__attribute__((noinline))
static void do_statement(void)
{
    int top = gen_here(), mark = sym_scope_begin();    /* see TK_KW_IF */
    int outer = scope_mark;

    scope_mark = mark;
    next();
    loop_begin(-1);
    substatement();
    expect(TK_KW_WHILE, "'while' after the body of a do");
    holes_land(&continues, jumps.continue_mark);
    condition();
    expect(TK_SEMI, "';'");
    gen_jump_if_true_to(top);
    loop_end();
    sym_scope_end(mark);
    scope_mark = outer;
}

/* The constant a case label names, converted to the type the switch compares
 * at. A literal, with a sign or without, is read directly, which is the only
 * way to get one wider than an int -- the value stack holds constants at int
 * width; anything else has to fold to a constant, as an array's size does. */
static long case_constant(uint32_t *high)
{
    int line = tok_line;
    const char *spot = tok_at;
    long value;

    *high = 0;
    if (tok == TK_INT && type_wide(tok_type)) {
        value = tok_val;
        *high = type_eight(tok_type) ? tok_val_hi
                                     : (uint32_t) (value < 0 ? -1 : 0);
        next();
    } else if (tok == TK_MINUS) {
        /* A sign binds to what follows it and no further, so `-3 + 2` is
         * -1: a wide literal after it is negated here, and anything else
         * goes back into the expression the way a unary minus does. */
        int before = out_here();

        next();
        if (tok == TK_INT && type_wide(tok_type)) {
            uint64_t whole = (uint64_t) (uint32_t) tok_val;

            if (type_eight(tok_type))
                whole |= (uint64_t) tok_val_hi << 32;
            whole = 0 - whole;
            value = (long) (uint32_t) whole;
            *high = (uint32_t) (whole >> 32);
            next();
        } else {
            Type outer = narrow_dest;

            narrow_dest = 0;
            primary();
            vneg();
            binary_rest(PREC_LOWEST);
            narrow_dest = outer;
            value = constant_folded("a case label", line, spot, before);
            *high = (uint32_t) (value < 0 ? -1 : 0);
        }
    } else {
        value = constant_int("a case label", line);
        *high = (uint32_t) (value < 0 ? -1 : 0);
    }

    if (type_eight(in_switch.type))
        return (long) (uint32_t) value;
    if (type_wide(in_switch.type)) {
        *high = 0;

        return (long) (uint32_t) value;
    }
    *high = 0;

    return value & 0xffffff;
}

/* `case constant:` -- where the code for it starts, noted for the tests at
 * the end of the switch. The statement after it is parsed by the caller. */
__attribute__((noinline))
static void case_label(void)
{
    int line = tok_line, mark, dup, i;
    const char *spot = tok_at;
    long value;
    uint32_t high;

    next();
    if (in_switch.case_mark < 0)
        acc_error_spot(line, spot, "'case' is not inside a switch");
    value = case_constant(&high);
    expect(TK_COLON, "':' after a case");
    case_in_scope(line, spot);

    mark = in_switch.case_mark;
    dup = 0;
    if ((unsigned) (ncases - mark) < CASES_WALKED) {
        for (i = mark; i != ncases; i++)
            if (case_value[i] == value && case_high[i] == high)
                dup = 1;
    } else {
        if (ncases - mark == CASES_WALKED)
            case_slots_add(mark);       /* those walked so far */
        dup = *case_slot(value, high, mark) != 0;
    }
    if (dup)
        acc_error_spot(line, spot, "this switch already has a case "
                                   "for %ld",
                       type_unsigned(in_switch.type)
                       || type_wide(in_switch.type)
                       ? value : (long) ((value ^ 0x800000) - 0x800000));

    if (ncases == cases_cap) {
        cases_cap = cases_cap ? cases_cap * 2 : 16;
        case_value = realloc(case_value, (size_t) cases_cap * sizeof *case_value);
        case_high = realloc(case_high, (size_t) cases_cap * sizeof *case_high);
        case_at = realloc(case_at, (size_t) cases_cap * sizeof *case_at);
        if (!case_value || !case_high || !case_at)
            acc_error("out of memory for case labels");
    }
    case_value[ncases] = value;
    case_high[ncases] = high;
    case_at[ncases] = gen_here();
    ncases++;
    if ((unsigned) (ncases - mark) > CASES_WALKED)
        case_slots_add(ncases - 1);
}

__attribute__((noinline))
static void default_label(void)
{
    int line = tok_line;
    const char *spot = tok_at;

    next();
    expect(TK_COLON, "':' after default");
    if (in_switch.case_mark < 0)
        acc_error_spot(line, spot, "'default' is not inside a switch");
    if (in_switch.default_at >= 0)
        acc_error_spot(line, spot, "this switch already has a default");
    case_in_scope(line, spot);
    in_switch.default_at = gen_here();
}

/* `switch (value) body`.
 *
 * One pass, so the body is compiled where it is read, and a case label is
 * only an address noted on the way: which is what lets one sit anywhere in
 * the body -- inside a loop, as Duff's device has them -- and be jumped to
 * from outside. The tests come after the body, where every case is known:
 *
 *         value to a frame slot
 *         goto tests
 *         body, with the cases in it
 *         goto end
 *   tests: if value == case 1 goto it ... else goto default, or end
 *   end:
 *
 * The value is compared at its promoted type, as C says, and each case is
 * converted to that type. A char is kept as the byte it is and compared in
 * A: the int it becomes holds nothing more, and a case outside the char's
 * range, which it can never equal, has no test. */
__attribute__((noinline))
static void switch_statement(void)
{
    Switch saved_switch = in_switch;
    Type type, held;
    int line, slot, to_tests, i, mark = sym_scope_begin();     /* see TK_KW_IF */
    int outer = scope_mark;
    const char *spot;

    scope_mark = mark;
    next();
    expect(TK_LPAREN, "'('");
    line = tok_line;
    spot = tok_at;
    comma_expr();
    expect(TK_RPAREN, "')'");
    type = vtype();
    if (type_pointer(type) || type_float(type))
        acc_error_spot(line, spot, "a switch needs an integer, and this is %s",
                       type_pointer(type) ? "a pointer"
                                          : "a floating-point value");
    held = type_size(type) == 1 ? type : type_promote(type);
    type = type_promote(type);
    vconvert(held);
    slot = gen_local(type_scalar_bytes(held));
    vstore_local(slot, held);
    gen_discard();
    to_tests = gen_jump();

    in_switch.case_mark = ncases;
    in_switch.default_at = -1;
    in_switch.type = type;
    in_switch.blocks = (int) ((char *) vla_top - (char *) vla_blocks);
    jumps_push();
    jumps.break_mark = breaks.count;

    substatement();

    hole_push(&breaks, gen_jump());
    gen_label(to_tests);
    gen_stmt_end();
    gen_switch_load(slot, held);
    for (i = in_switch.case_mark; i < ncases; i++)
        gen_switch_case(case_value[i], case_high[i], held, case_at[i], slot);
    if (in_switch.default_at >= 0)
        gen_jump_to(in_switch.default_at);
    else
        hole_push(&breaks, gen_jump());

    ncases = in_switch.case_mark;
    holes_land(&breaks, jumps.break_mark);
    in_switch = saved_switch;
    sym_scope_end(mark);
    scope_mark = outer;
    jumps_pop();
}

/* ------------------------------------------------------------------ */
/* goto and labels                                                     */

/* The labels of the function being compiled. They are a namespace of their
 * own -- a label may share its name with a variable -- and they belong to the
 * whole function, so a goto may name one further down. That one is not known
 * yet, so its jump is left as a hole and filled in when the label is
 * reached; a label never reached is an error once the function ends, blamed
 * on the first goto that named it. */
typedef struct {
    NameRef name;
    int     at;             /* its address, or -1 until it is reached */
    VlaBlock *blocks;       /* the blocks open when it was reached, and */
    int       nblocks;      /* their marks then: see vla_back_to */
    int     line, col;      /* where it was first named */
} Label;

static Label *labels;
static int    nlabels, labels_cap;

/* The gotos still waiting for their label: the hole, which label, and --
 * for vm_forward_in, once the label is reached -- the goto's line, column
 * and vla_serial then. The column is worked out when the goto is read,
 * since the function may run on past where its window can still say. */
typedef struct {
    int hole, label, line, col, vm;
} Goto;

static Goto *gotos;
static int   ngotos, gotos_cap;

/* The blocks open now, as bytes rather than a count of entries, which
 * would be a multiply by the entry's size. */
static VlaBlock *vla_blocks_copy(void)
{
    size_t bytes = (size_t) ((char *) vla_top - (char *) vla_blocks);
    VlaBlock *copy = malloc(bytes + 1);

    if (!copy)
        acc_error("out of memory for labels");
    memcpy(copy, vla_blocks, bytes);

    return copy;
}

static int label_find(NameRef name, int line, int col)
{
    int i;

    for (i = 0; i != nlabels; i++)
        if (labels[i].name == name)
            return i;

    if (nlabels == labels_cap) {
        labels_cap = labels_cap ? labels_cap * 2 : 8;
        labels = realloc(labels, (size_t) labels_cap * sizeof *labels);
        if (!labels)
            acc_error("out of memory for labels");
    }
    labels[nlabels].name = name;
    labels[nlabels].at = -1;
    labels[nlabels].line = line;
    labels[nlabels].col = col;
    labels[nlabels].blocks = NULL;
    labels[nlabels].nblocks = 0;

    return nlabels++;
}

__attribute__((noinline))
static void goto_statement(void)
{
    int line = tok_line, col = lex_col(), label;

    next();
    if (tok != TK_IDENT)
        acc_error_at(tok_line, "'goto' needs a label, and this is %s",
                     tok_spelling(tok));
    label = label_find(tok_name, line, col);
    next();
    expect(TK_SEMI, "';'");

    /* Back to a label: an array whose length is worked out, declared since
     * the label, is left behind -- C99 ends its lifetime there -- and the
     * room it took has to go back, or a loop made of a goto takes it again
     * on every turn. gcc's 20040811-1 is a million turns of that. */
    if (labels[label].at >= 0) {
        int back = vla_back_to(labels[label].blocks, labels[label].nblocks);

        if (vm_back_in(labels[label].blocks, labels[label].nblocks))
            vm_jump_refused(line, col);

        if (back != NO_VLA_MARK)
            gen_stack_back(back);
        gen_jump_to(labels[label].at);

        return;
    }
    if (ngotos == gotos_cap) {
        gotos_cap = gotos_cap ? gotos_cap * 2 : 8;
        gotos = realloc(gotos, (size_t) gotos_cap * sizeof *gotos);
        if (!gotos)
            acc_error("out of memory for gotos");
    }
    gotos[ngotos].hole = gen_jump();
    gotos[ngotos].label = label;
    gotos[ngotos].line = line;
    gotos[ngotos].col = col;
    gotos[ngotos].vm = vla_serial;
    ngotos++;
}

/* `name: statement` -- the label is here, and every goto that was waiting for
 * it lands here too. */
__attribute__((noinline))
static void label_statement(void)
{
    int line = tok_line, col = lex_col(), i, kept = 0;
    int label = label_find(tok_name, line, col);

    if (labels[label].at >= 0)
        acc_error_pos(line, col, "the label '%s' is defined twice",
                      name_text(tok_name));
    next();
    expect(TK_COLON, "':'");
    labels[label].at = gen_here();
    labels[label].blocks = vla_blocks_copy();
    labels[label].nblocks = nvla_blocks;

    for (i = 0; i != ngotos; i++) {
        Goto *g = gotos + i;

        if (g->label == label) {
            if (vm_forward_in(g->vm))
                vm_jump_refused(g->line, g->col);
            gen_label(g->hole);
            continue;
        }
        gotos[kept++] = *g;
    }
    ngotos = kept;

    statement();
}

/* The end of a function: every goto has to have found its label. */
void labels_end(void)
{
    if (ngotos)
        acc_error_pos(labels[gotos[0].label].line, labels[gotos[0].label].col,
                      "the label '%s' is used but never defined",
                      name_text(labels[gotos[0].label].name));
    {
        Label *l, *end = labels + nlabels;

        for (l = labels; l < end; l++)          /* walked: see vla_top */
            free(l->blocks);
    }
    nlabels = 0;
}

/* A for loop's step, as text: see for_statement. */
#define STEP_TEXT_MAX 160
static char step_buf[STEP_TEXT_MAX];

/* The step's text, now that it has been read and the `)` after it is the
 * current token: a copy with a `;` after it, which the lexer frees, or NULL
 * if the loop is to be compiled the old way -- the text could not be kept
 * whole, or the body could make it mean something else, or it declares
 * something: a struct in the step is in scope in the body, and read in its
 * place it has been declared already. A macro could be
 * defined again in the body, and would then be expanded differently read
 * again after it; a string or character literal's bytes go where the
 * saved token after the body keeps its own. */
static char *step_kept(int *len)
{
    int n, i;
    char *end, *text;

    end = lex_record_take_paren();
    if (!end)
        return NULL;
    n = (int) (end - step_buf);
    for (i = 0; i < n; i++) {
        int c = (unsigned char) step_buf[i], j = i;

        if (c == '"' || c == '\'' || c == '#')
            return NULL;
        if (!(c == '_' || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z')))
            continue;
        while (j < n && (step_buf[j] == '_'
                         || ((step_buf[j] | 0x20) >= 'a'
                             && (step_buf[j] | 0x20) <= 'z')
                         || (step_buf[j] >= '0' && step_buf[j] <= '9')))
            j++;
        if (lex_macro_def(name_intern(step_buf + i, j - i)))
            return NULL;
        if ((j - i == 6 && (!memcmp(step_buf + i, "struct", 6)))
            || (j - i == 5 && !memcmp(step_buf + i, "union", 5))
            || (j - i == 4 && !memcmp(step_buf + i, "enum", 4)))
            return NULL;                /* a type the body can see */
        i = j - 1;
    }
    text = malloc((size_t) n + 2);
    if (!text)
        acc_error("out of memory for a loop's step");
    memcpy(text, step_buf, (size_t) n);
    text[n] = ';';
    text[n + 1] = '\0';                 /* the end a window is read to */
    *len = n + 1;

    return text;
}

/* The step read again, as a statement of its own, and the token that was
 * current when it began -- the one after the body -- put back after it,
 * with its window closed, which frees the text. */
static void step_again(char *text, int len)
{
    LexToken saved;

    (void) len;
    lex_token_save(&saved);
    lex_push_record_owned(text);
    next();
    gen_stmt_end();
    comma_expr();
    gen_discard();
    if (tok != TK_SEMI)
        acc_error_at(tok_line, "internal: a loop's step read again did not "
                               "end where it was kept");
    lex_pop_record();
    lex_token_restore(&saved);
}

/* `for (init; condition; step) body`.
 *
 * One pass, so the code comes out in the order it is read, and the step --
 * written before the body, run after it -- has to be jumped around to get
 * there:
 *
 *         init
 *   top:  if (!condition) goto end
 *         goto body
 *   step: step
 *         goto top
 *   body: body
 *         goto step
 *   end:
 *
 * Two jumps a trip where a compiler that could reorder would have one. A
 * missing step leaves out its block and the body goes straight back to the
 * top; a missing condition leaves out the test, which is C's `for (;;)`.
 *
 * The init may declare, as C99 lets it, and what it declares ends with the
 * loop: `for (int i = 0; ...)` twice in one function is two variables, and
 * neither is visible after its loop.
 *
 * Out of line: inlined into statement(), which every statement in the
 * program goes through, it gave that a larger frame and cost 1.5% of a
 * compile of programs with no for loop in them. */
__attribute__((noinline))
static void for_statement(void)
{
    int mark = sym_scope_begin(), outer = scope_mark;
    int top, to_end = -1, to_body, again, step_len;
    char *step;

    scope_mark = mark;
    next();
    expect(TK_LPAREN, "'('");

    if (starts_decl()) {
        declaration();                  /* and its semicolon */
    } else {
        if (tok != TK_SEMI) {
            comma_expr();
            gen_discard();
        }
        expect(TK_SEMI, "';'");
    }

    gen_stmt_end();
    top = gen_here();
    if (tok != TK_SEMI) {
        comma_expr();
        to_end = gen_jump_if_false();
    }

    /* The step is kept as text and read again after the body, so that the
     * loop is the condition, the body, the step and one jump back -- where
     * compiled in its place it had to be jumped over on the way in, and
     * jumped to from the body and back from itself: three jumps where one
     * will do, two of them taken every time round. */
    lex_record_from(step_buf, step_buf + STEP_TEXT_MAX);
    expect(TK_SEMI, "';'");

    /* Compiled in its place as ever, and kept as text as it is read: if the
     * text will do, the code is taken back and the loop laid out the other
     * way; if not -- it could not be kept, or it says something the body
     * could change -- the code stays. */
    again = top;
    to_body = -1;
    step = NULL;
    if (tok != TK_RPAREN) {
        GenMark before;

        gen_mark(&before);
        to_body = gen_jump();
        again = gen_here();
        gen_stmt_end();
        comma_expr();
        gen_discard();
        gen_jump_to(top);
        step = step_kept(&step_len);
        if (step)
            gen_rollback(&before);
    } else {
        lex_record_take_paren();        /* nothing to keep */
    }
    if (step) {
        expect(TK_RPAREN, "')'");
        loop_begin(-1);
        substatement();
        holes_land(&continues, jumps.continue_mark);
        step_again(step, step_len);
        gen_jump_to(top);
        if (to_end >= 0)
            gen_label(to_end);
        loop_end();
        sym_scope_end(mark);
        scope_mark = outer;

        return;
    }

    if (to_body >= 0)
        gen_label(to_body);
    expect(TK_RPAREN, "')'");

    loop_begin(again);
    substatement();
    gen_jump_to(again);
    if (to_end >= 0)
        gen_label(to_end);
    loop_end();

    sym_scope_end(mark);
    scope_mark = outer;
}

/* `{ ... }`: declarations and statements in any order, as C99 has them, and
 * what is declared in it ends with it -- an inner name shadows an outer one
 * of the same spelling until the brace closes. The frame bytes are not
 * reused; the scope is only which names mean what. The body of a function is
 * one of these too. */
void block(void)
{
    int mark = sym_scope_begin(), outer = scope_mark;
    int outer_vla = vla_mark;
#ifdef OPT_ACC
    int is_body = body_mark != -1;
#endif

    scope_mark = mark;
    if (body_mark != -1) {              /* equal, not less: see sym_find */
        scope_mark = body_mark;
        body_mark = -1;
    }
    vla_mark = NO_VLA_MARK;
    vla_block_open();
    while (tok != TK_RBRACE && tok != TK_EOF) {
        if (starts_decl())
            declaration();
        else
            statement();
    }
#ifdef OPT_ACC
    if (is_body)
        inline_body_end();
#endif
    expect(TK_RBRACE, "'}'");
    if (vla_mark != NO_VLA_MARK)
        gen_stack_back(vla_mark);       /* the room those arrays took */
    nvla_blocks--;
    vla_top--;
    vla_mark = outer_vla;
    sym_scope_end(mark);
    scope_mark = outer;
}

static void condition(void)
{
    /* narrow_dest is not cleared here, and does not need to be: it is only
     * set while an assignment's right-hand side is being parsed, and a
     * condition belongs to an if or a while, which are statements. There is
     * no way to reach one from inside an expression. The same goes for a
     * return. If `?:` or a statement expression ever arrives, both become
     * reachable and will need it. */
    expect(TK_LPAREN, "'('");
    comma_expr();
    expect(TK_RPAREN, "')'");
}

/* Dispatched on the token rather than tested against one keyword at a time.
 * Every statement in the program walks this, and a chain grows a comparison
 * for each form the language gains. */
static void statement(void)
{
    /* Whatever the last statement left in the frame's scratch area is done
     * with. This is the only place that is true: a long result lives there
     * and outlives the values it was computed from. */
    gen_stmt_end();

    /* Before anything else, because what is missing from acc is mostly a
     * statement -- goto, for one. Left to fall through they lex as names and
     * the complaint is that the name is not declared. */
    if (tok == TK_KW_RESERVED)
        reserved_word();

    switch (tok) {
    /* A selection statement is a block of its own, and so is an iteration
     * one (C99 6.8.4p3, 6.8.5p5): what its controlling expression declares
     * -- `if (sizeof (enum { T }))`, pr67784 -- ends with the statement,
     * and a name it hid is seen again after it. */
    case TK_KW_IF: {
        int to_else, mark = sym_scope_begin(), outer = scope_mark;

        scope_mark = mark;              /* so a tag may be defined again */
        next();
        condition();
        to_else = gen_jump_if_false();
        substatement();

        /* `else if` needs nothing of its own: the else branch is a statement,
         * and an if is a statement. A dangling else binds to the nearest if
         * for the same reason -- the inner if consumes it first. */
        if (accept(TK_KW_ELSE)) {
            int to_end = gen_jump();

            gen_label(to_else);
            substatement();
            gen_label(to_end);
        } else {
            gen_label(to_else);
        }
        sym_scope_end(mark);
        scope_mark = outer;

        return;
    }

    case TK_KW_WHILE:
        while_statement();

        return;

    case TK_KW_DO:
        do_statement();

        return;

    case TK_KW_SWITCH:
        switch_statement();

        return;

    case TK_KW_BREAK:
        break_statement();

        return;

    case TK_KW_CONTINUE:
        continue_statement();

        return;

    /* A label and then the statement it labels, which may be another. */
    case TK_KW_CASE:
        case_label();
        statement();

        return;

    case TK_KW_DEFAULT:
        default_label();
        statement();

        return;

    case TK_LBRACE:
        next();
        block();

        return;

    case TK_KW_FOR:
        for_statement();

        return;

    case TK_KW_RETURN: {
        int line = tok_line;
        const char *spot = tok_at;

#ifdef OPT_ACC
        if (inline_body_returning()) {
            inline_body_return();

            return;
        }
#endif
        if (inline_capture) {
            return_kept(line, spot);

            return;
        }
        next();
        if (tok != TK_SEMI)
            comma_expr();
        expect(TK_SEMI, "';'");
        gen_return(line, spot);

        return;
    }

    case TK_SEMI:
        next();

        return;

    case TK_KW_ELSE:
        acc_error_at(tok_line, "'else' without an 'if'");

        return;

    case TK_KW_GOTO:
        goto_statement();

        return;

    default:
        if (tok == TK_IDENT && lex_colon_follows()) {
            label_statement();

            return;
        }
        comma_expr();
        gen_discard();                /* the value of a statement is discarded */
        expect(TK_SEMI, "';'");

        return;
    }
}
