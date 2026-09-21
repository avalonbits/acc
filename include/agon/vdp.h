/* The VDP, as libagon names it.
 *
 * Almost all of it is byte sequences sent to the console -- VDU codes, which
 * the ESP32 on the other side of the serial link reads -- so almost all of
 * it is putch and nothing else.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_AGON_VDP_H
#define ACC_AGON_VDP_H

#include <stdint.h>
#include <stdbool.h>
#include <agon/mos.h>

void vdp_clear_screen(void);
void vdp_cursor_home(void);
void vdp_cursor_left(void);
void vdp_cursor_right(void);
void vdp_cursor_up(void);
void vdp_cursor_down(void);
void vdp_cursor_tab(int xpos, int ypos);
void vdp_cursor_enable(bool flag);

#define vdp_cls() vdp_clear_screen()

/* A run of VDU bytes held in an object -- an array or a struct of them --
 * sent as it stands. The size comes from the object, so what is sent is
 * exactly what is there and nothing has to say how long it is twice. */
#define VDP_PUTS(S) mos_puts((char *) &(S), sizeof(S), 0)

#endif
