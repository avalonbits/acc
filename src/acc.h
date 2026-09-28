/*
 * acc -- a C compiler for the Agon Light.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * One pass, no syntax tree, no intermediate representation. The parser emits
 * eZ80 machine code as it reads, with a small stack of *descriptions* of
 * values -- a constant, a local at a frame offset, something already in a
 * register -- that are only turned into instructions when something forces
 * them. That is tinycc's model and it is the right one: it is the fastest
 * way to compile C, and more to the point here it is the smallest.
 *
 * The language is C99. What is here is a subset of it, grown a feature at a
 * time with a test for each; every gap is something not written yet rather
 * than something ruled out, and the ones that are are named where the
 * compiler refuses them.
 *
 * What is not tinycc's is the memory. The Agon gives a program 448 KB for
 * code, data, heap and stack together, and measured on that machine tinycc
 * spends 31 bytes on every symbol and 75 KB on machinery for a preprocessor
 * this compiler does not have yet. Everything below is sized for the target
 * rather than for a workstation: symbols are 7 bytes, names live in one arena
 * that is never freed, and the output is written as it is produced.
 */
#ifndef ACC_H
#define ACC_H

/* The compiler's parts, each in a header of its own, in the order each
 * can rely on the ones before it. A file that includes this sees all of
 * them; what a part's own files share among themselves, and no one else
 * needs, is in its *_int.h. */
#include "types.h"
#include "diag.h"
#include "names.h"
#include "lex.h"
#include "sym.h"
#include "gen.h"
#include "out.h"
#include "obj.h"

#endif /* ACC_H */
