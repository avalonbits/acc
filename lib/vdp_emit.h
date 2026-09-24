/*
 * What every VDP call in the library is made of: the bytes of one VDU
 * command, sent in one go.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Private to lib/vdp_*.c. SEND puts its bytes in an array on the stack and
 * hands the whole of it to MOS in one RST 18h -- one call into MOS a
 * command rather than one a byte, which is what libagon does too. A 16-bit
 * argument is two bytes, low first, as the VDP reads every one of them: the
 * VDU documentation writes such an argument `x;`.
 */
#ifndef ACC_LIB_VDP_EMIT_H
#define ACC_LIB_VDP_EMIT_H

#include <agon/mos.h>

#define LO(v)   ((unsigned char) (v))
#define HI(v)   ((unsigned char) ((unsigned) (v) >> 8))
#define W(v)    LO(v), HI(v)                    /* a 16-bit argument */
#define U24(v)  LO(v), HI(v), ((unsigned char) ((unsigned long) (v) >> 16))

#define SEND(...) do {                                                   \
        const unsigned char send_[] = { __VA_ARGS__ };                   \
        mos_puts((const char *) send_, sizeof send_, 0);                 \
    } while (0)

/* A run of bytes that is not a command of its own: the data after one. */
#define SEND_BYTES(p, n) mos_puts((const char *) (p), (uint24_t) (n), 0)

#endif
