/*
 * agon/timer.h -- a delay, as libagon names it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#ifndef ACC_AGON_TIMER_H
#define ACC_AGON_TIMER_H

/* Waits `ms` milliseconds, counted by the eZ80's timer 0. */
void delay(unsigned int ms);

#endif
