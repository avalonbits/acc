/*
 * acc -- a C compiler for the Agon Light.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The version acc reports with -v. It matches the tag a release is cut
 * from, and mkrelease.sh refuses to build a zip that says otherwise.
 */
#ifndef ACC_VERSION_H
#define ACC_VERSION_H

#define ACC_VERSION "0.2.1"

/* What the compiler calls itself: acc, or opt-acc, its optimising build for
 * the host (docs/optimizer-plan.md). */
#ifdef OPT_ACC
#define ACC_NAME "opt-acc"
#define ACC_FOR  "an optimising C compiler for the Agon, run on the host"
#else
#define ACC_NAME "acc"
#define ACC_FOR  "a C compiler for the Agon"
#endif

#endif
