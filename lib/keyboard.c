/*
 * The keyboard, as libagon names it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>

#include <agon/keyboard.h>
#include <agon/mos.h>

/* A ring of key events, filled by a handler MOS calls on every key packet,
 * so that a program busy for a while between polls still gets every key it
 * was sent, in order -- up to buf_len of them, past which the newest are
 * dropped. The handler, and the ring's state it reads and writes, are
 * acc's runtime: see the end of src/rt/helpers.s. The ring is here. */
void acc_rt_kbuf_handler(void);
unsigned char *acc_rt_kbuf_state(void);

#define KB_SLOTS 0              /* the state's bytes: see helpers.s */
#define KB_START 1
#define KB_END   2
#define KB_RING  3
#define KB_EVENT 4              /* bytes an event takes in the ring */

static uint8_t *ring;

void kbuf_init(uint8_t buf_len)
{
    volatile uint8_t *s = acc_rt_kbuf_state();

    if (ring)
        kbuf_deinit();
    ring = malloc(((size_t) buf_len + 1) * KB_EVENT);
    if (!ring)
        return;

    s[KB_SLOTS] = (uint8_t) (buf_len + 1);
    s[KB_START] = 0;
    s[KB_END] = 0;
    memcpy((void *) (s + KB_RING), &ring, 3);
    mos_setkbvector(acc_rt_kbuf_handler, 0);
}

void kbuf_deinit(void)
{
    if (!ring)
        return;

    mos_setkbvector(NULL, 0);
    free(ring);
    ring = NULL;
}

void kbuf_clear(void)
{
    volatile uint8_t *s = acc_rt_kbuf_state();

    s[KB_START] = s[KB_END];
}

bool kbuf_poll_event(struct keyboard_event_t *e)
{
    volatile uint8_t *s = acc_rt_kbuf_state();
    uint8_t start = s[KB_START];

    if (!ring || start == s[KB_END])
        return false;

    memcpy(e, ring + start * KB_EVENT, KB_EVENT);
    start++;
    if (start == s[KB_SLOTS])
        start = 0;
    s[KB_START] = start;

    return true;
}
