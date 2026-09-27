/*
 * agon/vdp/screen.h -- text, colour, modes, viewports, the cursor,
 * characters, the keyboard and the VDP's system commands. Part of
 * <agon/vdp.h>, which is what a program includes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#ifndef ACC_AGON_VDP_SCREEN_H
#define ACC_AGON_VDP_SCREEN_H

/* libagon's pointer to the system variables; sys_vars is the same. */
volatile SYSVAR *vdp_vdu_init(void);

/* VDU 1 to 31 and 127, one byte each or with their arguments. */
void vdp_send_to_printer(char ch);
void vdp_enable_printer(void);
void vdp_disable_printer(void);
void vdp_write_at_text_cursor(void);
void vdp_write_at_graphics_cursor(void);
void vdp_enable_screen(void);
void vdp_disable_screen(void);
void vdp_bell(void);
void vdp_cursor_left(void);
void vdp_cursor_right(void);
void vdp_cursor_down(void);
void vdp_cursor_up(void);
void vdp_clear_screen(void);
#define vdp_cls() vdp_clear_screen()
void vdp_carriage_return(void);
void vdp_page_mode_on(void);
void vdp_page_mode_off(void);
void vdp_set_text_colour(int colour);
void vdp_set_text_bg_colour(int colour);        /* adds BACKGROUND_COL_OFFSET */
void vdp_define_colour(int logical, int physical, int red, int green, int blue);
void vdp_reset_graphics(void);
int  vdp_mode(int mode);                        /* -1, and nothing sent, past 254 */
void vdp_reset_viewports(void);
void vdp_set_text_viewport(int left, int bottom, int right, int top);
void vdp_cursor_home(void);
void vdp_cursor_tab(int xpos, int ypos);
void vdp_backspace(void);                       /* VDU 127 */
void vdp_outchar(uint8_t c);                    /* VDU 27: c drawn, whatever it is */
void vdp_send_to_screen(char c);                /* the same */

/* VDU 23: the cursor, scrolling, and redefining characters. */
void vdp_cursor_enable(bool flag);
void vdp_scroll_screen(int direction, int speed);
void vdp_scroll_screen_extent(int extent, int direction, int speed);  /* 0 text, 1 screen, 2 graphics */
void vdp_cursor_behaviour(int setting, int mask);
void vdp_redefine_character(int char_num, uint8_t b0, uint8_t b1, uint8_t b2,
                            uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6,
                            uint8_t b7);
void vdp_redefine_character_special(int char_num, uint8_t b0, uint8_t b1,
                                    uint8_t b2, uint8_t b3, uint8_t b4,
                                    uint8_t b5, uint8_t b6, uint8_t b7);
void vdp_define_character(int char_num, uint8_t *data);
void vdp_reset_system_font(void);

/* VDU 23,0: the cursor's shape and movement. */
void vdp_set_cursor_start_line(int n);
void vdp_set_cursor_end_line(int n);
void vdp_set_cursor_start_column(int n);
void vdp_set_cursor_end_column(int n);
void vdp_move_cursor_relative(int x, int y);

/* Viewports and the origin from the last two graphics points. */
void vdp_set_text_viewport_via_plot(void);
void vdp_set_graphics_viewport_via_plot(void);
void vdp_set_graphics_origin_via_plot(void);
void vdp_move_graphics_origin_and_viewport(void);

/* Asking the VDP, and what it answers, which MOS keeps in the system
 * variables. A request waits for its answer when `wait` is true; a
 * return_ call always waits. */
void     vdp_request_text_cursor_position(bool wait);
void     vdp_return_text_cursor_position(uint8_t *return_x, uint8_t *return_y);
void     vdp_request_ascii_code_at_position(int x, int y, bool wait);
uint8_t  vdp_return_ascii_code_at_position(int x, int y);
void     vdp_request_ascii_code_at_graphics_position(int x, int y, bool wait);
uint8_t  vdp_return_ascii_code_at_graphics_position(int x, int y);
void     vdp_request_pixel_colour(int x, int y, bool wait);
uint24_t vdp_return_pixel_colour(int x, int y);
void     vdp_request_palette_entry(int n, bool wait);
uint24_t vdp_return_palette_entry_colour(int n);
uint8_t  vdp_return_palette_entry_index(int n);
void     vdp_get_scr_dims(bool wait);
void     vdp_request_rtc(bool wait);
void     vdp_set_rtc(int year, int month, int day, int hour, int minute, int second);
void     vdp_general_poll(int n);

/* The keyboard: its layout, repeat and lights, and what is held. */
void    vdp_set_keyboard_locale(int locale);
void    vdp_keyboard_control(int delay, int rate, int led);
void    vdp_control_keys(bool on);
void    vdp_check_key(int vkey);                /* a fresh packet for this key */
uint8_t vdp_getKeyMap(uint8_t index);
uint8_t vdp_getKeyCode(void);                   /* 0 while no key is down */
void    vdp_waitKeyUp(void);
void    vdp_waitKeyDown(void);

/* The rest of VDU 23,0. */
void vdp_logical_scr_dims(bool flag);
void vdp_set_pixel_coordinates(void);
void vdp_set_logical_coordinates(void);
void vdp_legacy_modes(bool on);
void vdp_swap(void);
void vdp_flush_drawing_commands(void);
void vdp_console_mode(bool on);
void vdp_terminal_mode(void);
void vdp_temp_paged_mode(void);
void vdp_print_buffer(int bufferId);
void vdp_set_variable(uint16_t variableId, uint16_t value);
void vdp_clear_variable(uint16_t variableId);

#endif
