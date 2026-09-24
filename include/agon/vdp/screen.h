/*
 * agon/vdp/screen.h -- part of <agon/vdp.h>, which is what a program includes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_AGON_VDP_SCREEN_H
#define ACC_AGON_VDP_SCREEN_H

void vdp_clear_screen(void);
void vdp_cursor_home(void);
void vdp_cursor_left(void);
void vdp_cursor_right(void);
void vdp_cursor_up(void);
void vdp_cursor_down(void);
void vdp_cursor_tab(int xpos, int ypos);
void vdp_cursor_enable(bool flag);

#define vdp_cls() vdp_clear_screen()

#endif
