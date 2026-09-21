/*
 * The VDP, as libagon names it: VDU codes sent to the console.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <agon/vdp.h>
#include <agon/mos.h>

void vdp_clear_screen(void)
{
    putch(12);
}

void vdp_cursor_home(void)
{
    putch(30);
}

void vdp_cursor_left(void)
{
    putch(8);
}

void vdp_cursor_right(void)
{
    putch(9);
}

void vdp_cursor_up(void)
{
    putch(11);
}

void vdp_cursor_down(void)
{
    putch(10);
}

/* VDU 31 takes the column and the row after it. */
void vdp_cursor_tab(int xpos, int ypos)
{
    putch(31);
    putch(xpos);
    putch(ypos);
}

/* VDU 23, 1, flag: one of the VDP's own commands rather than a plain code. */
void vdp_cursor_enable(bool flag)
{
    putch(23);
    putch(1);
    putch(flag ? 1 : 0);
}
