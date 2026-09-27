/*
 * signal.h -- signal handling (C99 7.14).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Nothing on the Agon sends a program a signal, so the only signals there
 * are the ones it raises itself, with raise or abort. What a handler does
 * then is what C says: it is called with the signal's number, after the
 * signal has been put back to SIG_DFL. The default for every signal is to
 * end the program, with 128 and the signal's number as its status -- which
 * is what a shell reports for a program a signal ended, and so what abort
 * has always exited with.
 *
 * The numbers are Linux's.
 */
#pragma once
#ifndef ACC_SIGNAL_H
#define ACC_SIGNAL_H

typedef int sig_atomic_t;

#define SIG_DFL ((void (*)(int)) 0)
#define SIG_ERR ((void (*)(int)) -1)
#define SIG_IGN ((void (*)(int)) 1)

#define SIGINT  2
#define SIGILL  4
#define SIGABRT 6
#define SIGFPE  8
#define SIGSEGV 11
#define SIGTERM 15

void (*signal(int sig, void (*func)(int)))(int);
int raise(int sig);

#endif
