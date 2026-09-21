/*
 * <time.h>: the clock MOS keeps.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <time.h>
#include <agon/mos.h>

/* Four bytes of the system variables, lowest first: hundredths of a second
 * since the machine was turned on, which MOS adds two to every time the
 * screen finishes a frame.
 *
 * Read as four separate bytes rather than as a long, because the four are
 * not written at once: MOS updates them from its own interrupt, and a read
 * that happened to fall in the middle of a carry would see the low byte
 * after it wrapped and the high one before. Reading them one at a time does
 * not fix that, and nothing here can -- what it does is keep the answer to
 * within a tick, which is what the clock is worth anyway. */
clock_t clock(void)
{
    uint8_t *v = mos_sysvars();

    return (clock_t) v[0] | ((clock_t) v[1] << 8) | ((clock_t) v[2] << 16)
           | ((clock_t) v[3] << 24);
}
