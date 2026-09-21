/* The keyboard, as libagon names it.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_AGON_KEYBOARD_H
#define ACC_AGON_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/* One key going down or coming up: what it means, which modifiers were held,
 * which physical key it was, and which way it went. Every field is a byte,
 * so how a struct is laid out does not come into it. */
struct keyboard_event_t {
    uint8_t ascii;
    uint8_t kmod;
    uint8_t vkey;
    uint8_t isdown;
};

void kbuf_init(uint8_t buf_len);
bool kbuf_poll_event(struct keyboard_event_t *e);
void kbuf_clear(void);
void kbuf_deinit(void);

#endif
