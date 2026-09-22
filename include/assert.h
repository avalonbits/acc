/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* C99 7.2. Deliberately without an include guard round the macro: the
 * standard says assert is redefined to what NDEBUG says every time this
 * header is included, so a file that turns NDEBUG on and includes it again
 * gets the other one. */
#undef assert

#ifdef NDEBUG
#define assert(e)   ((void) 0)
#else
#define assert(e)   ((e) ? (void) 0 \
                         : __acc_assert_failed(#e, __FILE__, __LINE__, __func__))
#endif

#ifndef ACC_ASSERT_H
#define ACC_ASSERT_H

__attribute__((noreturn))
void __acc_assert_failed(const char *expr, const char *file, int line,
                         const char *fn);

#endif
