/*
 * agon/vdp/graphics.h -- part of <agon/vdp.h>, which is what a program includes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The graphics colour, viewport and origin, the line style, and PLOT with
 * every shape it draws. Coordinates are the VDP's: 16-bit, from the
 * graphics origin, with y up unless the program has turned logical
 * coordinates off.
 */
#ifndef ACC_AGON_VDP_GRAPHICS_H
#define ACC_AGON_VDP_GRAPHICS_H

/* VDU 16: clear the graphics viewport (CLG). */
void vdp_clear_graphics(void);
#define vdp_clg() vdp_clear_graphics()

/* VDU 18, mode, colour: the graphics colour (GCOL mode, colour), and the
 * modes it is painted in. A colour of 128 or more is the background. */
#define GCOLMODE_COLOUR         0   /* set the pixel to the colour */
#define GCOLMODE_OR             1   /* OR the colour into the pixel */
#define GCOLMODE_AND            2   /* AND the colour into the pixel */
#define GCOLMODE_EOR            3   /* EOR the colour into the pixel */
#define GCOLMODE_INVERT         4   /* invert the pixel */
#define GCOLMODE_NOP            5   /* leave the pixel alone */
#define GCOLMODE_AND_INV_COLOUR 6   /* AND the inverse of the colour */
#define GCOLMODE_OR_INV_COLOUR  7   /* OR the inverse of the colour */
void vdp_set_graphics_colour(uint8_t mode, uint8_t colour);
void vdp_set_graphics_fg_colour(uint8_t mode, uint8_t colour);
void vdp_set_graphics_bg_colour(uint8_t mode, uint8_t colour); /* colour 0-127 */
#define vdp_gcol(M, C) vdp_set_graphics_colour(M, C)

/* VDU 24, left; bottom; right; top;: the graphics viewport. */
void vdp_set_graphics_viewport(int left, int bottom, int right, int top);

/* VDU 29, x; y;: the graphics origin. */
void vdp_graphics_origin(int x, int y);

/* The line style. VDU 23, 23, n: lines n pixels thick. VDU 23, 6, ...: the
 * dot-dash pattern, one bit a pixel. VDU 23, 0, &F2, n: how many of those
 * bits are used, 1-64, or 0 for the default pattern back; above 64 sends
 * nothing. */
void vdp_set_line_thickness(int pixels);
void vdp_set_dotdash_line_pattern(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                                  uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7);
void vdp_set_dotdash_pattern_length(int n);

/* VDU 25, code, x; y;: PLOT. The code is an operation below or'ed with a
 * mode: what the point is relative to, and what colour it is drawn in. */
#define PLOTMODE_MOVE_RELATIVE      0
#define PLOTMODE_FG_RELATIVE        1
#define PLOTMODE_INVERT_RELATIVE    2
#define PLOTMODE_BG_RELATIVE        3
#define PLOTMODE_MOVE_ABSOLUTE      4
#define PLOTMODE_FG_ABSOLUTE        5
#define PLOTMODE_INVERT_ABSOLUTE    6
#define PLOTMODE_BG_ABSOLUTE        7

#define PLOT_LINE                   0x00
#define PLOT_LINE_OMIT_LAST         0x08
#define PLOT_DOTDASH                0x10
#define PLOT_DOTDASH_OMIT_LAST      0x18
#define PLOT_LINE_OMIT_FIRST        0x20
#define PLOT_LINE_OMIT_BOTH         0x28
#define PLOT_DOTDASH_OMIT_FIRST     0x30    /* the pattern carries on */
#define PLOT_DOTDASH_OMIT_BOTH      0x38    /* the pattern carries on */
#define PLOT_POINT                  0x40
#define PLOT_FILL_LEFTRIGHT_TO_NONBG 0x48
#define PLOT_TRIANGLE               0x50
#define PLOT_FILL_RIGHT_TO_BG       0x58
#define PLOT_RECTANGLE              0x60
#define PLOT_FILL_LEFTRIGHT_TO_FG   0x68
#define PLOT_PARALLELOGRAM          0x70
#define PLOT_FILL_RIGHT_TO_NONFG    0x78
#define PLOT_FLOOD_TO_NONBG         0x80
#define PLOT_FLOOD_TO_FG            0x88
#define PLOT_CIRCLE                 0x90
#define PLOT_FILLED_CIRCLE          0x98
#define PLOT_ARC                    0xA0
#define PLOT_SEGMENT                0xA8
#define PLOT_SECTOR                 0xB0
#define PLOT_COPY_MOVE              0xB8    /* modes 1 and 5 move, the rest copy */
#define PLOT_ELLIPSE                0xC0
#define PLOT_FILLED_ELLIPSE         0xC8
#define PLOT_PATH                   0xD8
#define PLOT_BITMAP                 0xE8
void vdp_plot(int plot_mode, int x, int y);

/* Moving the graphics cursor, and lines from it. */
void vdp_move_to(int x, int y);
void vdp_move_by(int dx, int dy);
void vdp_line_to(int x, int y);
void vdp_line_by(int dx, int dy);
void vdp_line(int x1, int y1, int x2, int y2);   /* no vdp_move_to first */
void vdp_line_to_omit_last(int x, int y);
void vdp_line_to_omit_first(int x, int y);
void vdp_line_to_omit_both(int x, int y);
void vdp_dotdash_line_to(int x, int y);
void vdp_dotdash_line(int x1, int y1, int x2, int y2);
void vdp_dotdash_line_to_omit_last(int x, int y);
void vdp_dotdash_line_to_omit_first(int x, int y);   /* the pattern carries on */
void vdp_dotdash_line_to_omit_both(int x, int y);    /* the pattern carries on */
void vdp_point(int x, int y);

/* Filling a row from (x,y), left and right or right only, up to a pixel
 * that is or is not the foreground or the background colour; and flooding
 * the whole area around (x,y) the same way. */
void vdp_line_fill_leftright_to_nonbg(int x, int y);
void vdp_line_fill_leftright_to_fg(int x, int y);
void vdp_line_fill_right_to_bg(int x, int y);
void vdp_line_fill_right_to_nonfg(int x, int y);
void vdp_flood_fill_to_nonbg(int x, int y);
void vdp_flood_fill_to_fg(int x, int y);

/* Shapes. A parallelogram is three corners in order; the VDP works out
 * the fourth. */
void vdp_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void vdp_filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void vdp_rectangle(int x1, int y1, int x2, int y2);
void vdp_filled_rectangle(int x1, int y1, int x2, int y2);
void vdp_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3);
void vdp_filled_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3);
void vdp_circle(int x, int y, int radius);
void vdp_filled_circle(int x, int y, int radius);

/* Arcs, segments and sectors of the circle around the centre, from the
 * first point on it to the second. */
void vdp_arc(int centre_x, int centre_y, int x1, int y1, int x2, int y2);
void vdp_segment(int centre_x, int centre_y, int x1, int y1, int x2, int y2);
void vdp_sector(int centre_x, int centre_y, int x1, int y1, int x2, int y2);

/* An ellipse the way Acorn draws one: around the centre, as wide as x1 is
 * from it (y1 does not matter), with its top at (x2, y2) -- a top not
 * above the centre shears it. */
void vdp_ellipse(int centre_x, int centre_y, int x1, int y1, int x2, int y2);
void vdp_filled_ellipse(int centre_x, int centre_y, int x1, int y1, int x2, int y2);

/* The rectangle between two corners, copied or moved so that its bottom
 * left is at dest; a move clears what it leaves behind. */
void vdp_copy_rectangle(int src_x1, int src_y1, int src_x2, int src_y2, int dest_x, int dest_y);
void vdp_move_rectangle(int src_x1, int src_y1, int src_x2, int src_y2, int dest_x, int dest_y);

/* A filled path. vdp_fill_path takes x, y pairs and their size in bytes
 * (sizeof path) -- three points at least, and whole pairs, or it sends
 * nothing. vdp_path_point adds one point to the path the last two points
 * the cursor visited begin; the VDP fills it when the next thing it is
 * sent is anything else. */
void vdp_fill_path(const int *path, int pathsize);
void vdp_path_point(int x, int y);

/* VDU 25, &ED, x; y;: the current bitmap, at (x,y). */
void vdp_plot_bitmap(int x, int y);

#endif
