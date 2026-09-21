/*
 * The keyboard, as libagon names it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <agon/keyboard.h>
#include <agon/mos.h>

/* Read from the system variables rather than caught as it happens.
 *
 * MOS counts key packets in one of them, so a program can tell that
 * something arrived by watching that count -- which is what this does. What
 * it cannot do is queue them: two keys between one poll and the next look
 * like one, and the first is lost. libagon's is a handler MOS calls, which
 * does not lose them, and which needs a vector installed and a ring buffer
 * behind it.
 *
 * The queue is what buf_len is for, and there is none, so it is ignored. */
static uint8_t last_count;
static int     started;

void kbuf_init(uint8_t buf_len)
{
    (void) buf_len;
    last_count = mos_sysvars()[sysvar_vkeycount];
    started = 1;
}

void kbuf_deinit(void)
{
    started = 0;
}

void kbuf_clear(void)
{
    last_count = mos_sysvars()[sysvar_vkeycount];
}

bool kbuf_poll_event(struct keyboard_event_t *e)
{
    uint8_t *v = mos_sysvars();
    uint8_t now = v[sysvar_vkeycount];

    if (!started)
        kbuf_init(0);
    if (now == last_count)
        return false;

    last_count = now;
    e->ascii = v[sysvar_keyascii];
    e->kmod = v[sysvar_keymods];
    e->vkey = v[sysvar_vkeycode];
    e->isdown = v[sysvar_vkeydown];

    return true;
}
