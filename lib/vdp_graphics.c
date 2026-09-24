/*
 * <agon/vdp/graphics.h>'s calls.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Nearly all of it is PLOT, VDU 25: a code, then a point. The VDP keeps
 * the last three points it was sent, and a shape takes its corners from
 * them -- so a triangle is two moves and a plot, and a circle a move to
 * the centre and a plot relative to it. Where libagon has a call, it is
 * built out of the same commands in the same order as libagon's, since
 * test/agonlib.sh holds each to the bytes libagon sends.
 */
#include <agon/vdp.h>

#include "vdp_emit.h"

/* PLOT code, x; y;: the one command almost everything here sends. */
static void plot(int code, int x, int y)
{
    SEND(25, code, W(x), W(y));
}

/* The shapes of three points: the first two are moves, and the third
 * draws the shape out of all three. */
static void plot3(int code, int x1, int y1, int x2, int y2, int x3, int y3)
{
    vdp_move_to(x1, y1);
    vdp_move_to(x2, y2);
    plot(code, x3, y3);
}

/* VDU 16 and 18: CLG and GCOL. libagon's background call flips the top
 * bit rather than setting it, so a colour it is given that is already a
 * background one comes out a foreground one; kept, as the bytes are the
 * same for every colour it is meant to take. */

void vdp_clear_graphics(void)
{
    SEND(16);
}

void vdp_set_graphics_colour(uint8_t mode, uint8_t colour)
{
    SEND(18, mode, colour);
}

void vdp_set_graphics_fg_colour(uint8_t mode, uint8_t colour)
{
    SEND(18, mode, colour);
}

void vdp_set_graphics_bg_colour(uint8_t mode, uint8_t colour)
{
    SEND(18, mode, colour ^ BACKGROUND_COL_OFFSET);
}

/* VDU 24 and 29: the viewport and the origin, in the order the VDP reads
 * them. */

void vdp_set_graphics_viewport(int left, int bottom, int right, int top)
{
    SEND(24, W(left), W(bottom), W(right), W(top));
}

void vdp_graphics_origin(int x, int y)
{
    SEND(29, W(x), W(y));
}

/* The line style: VDU 23, 23 and 23, 6, and VDU 23, 0, &F2 for the
 * pattern's length. The VDP takes any length up to 255, but libagon sends
 * nothing above 64, the most bits the pattern has. libagon's thickness
 * call sends 23, n, 0 -- the thickness written over the second 23 --
 * which the VDP takes for some other VDU 23 command; this sends the
 * 23, 23, n the VDP reads. */

void vdp_set_line_thickness(int pixels)
{
    SEND(23, 23, pixels);
}

void vdp_set_dotdash_line_pattern(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                                  uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
{
    SEND(23, 6, b0, b1, b2, b3, b4, b5, b6, b7);
}

void vdp_set_dotdash_pattern_length(int n)
{
    if (n > 64)
        return;

    SEND(23, 0, 0xF2, n);
}

/* PLOT itself, and the cursor moves and lines, one PLOT each. */

void vdp_plot(int plot_mode, int x, int y)
{
    plot(plot_mode, x, y);
}

void vdp_move_to(int x, int y)
{
    plot(PLOT_LINE | PLOTMODE_MOVE_ABSOLUTE, x, y);
}

void vdp_move_by(int dx, int dy)
{
    plot(PLOT_LINE | PLOTMODE_MOVE_RELATIVE, dx, dy);
}

void vdp_line_to(int x, int y)
{
    plot(PLOT_LINE | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_line_by(int dx, int dy)
{
    plot(PLOT_LINE | PLOTMODE_FG_RELATIVE, dx, dy);
}

void vdp_line(int x1, int y1, int x2, int y2)
{
    vdp_move_to(x1, y1);
    vdp_line_to(x2, y2);
}

void vdp_line_to_omit_last(int x, int y)
{
    plot(PLOT_LINE_OMIT_LAST | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_line_to_omit_first(int x, int y)
{
    plot(PLOT_LINE_OMIT_FIRST | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_line_to_omit_both(int x, int y)
{
    plot(PLOT_LINE_OMIT_BOTH | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_dotdash_line_to(int x, int y)
{
    plot(PLOT_DOTDASH | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_dotdash_line(int x1, int y1, int x2, int y2)
{
    vdp_move_to(x1, y1);
    vdp_dotdash_line_to(x2, y2);
}

void vdp_dotdash_line_to_omit_last(int x, int y)
{
    plot(PLOT_DOTDASH_OMIT_LAST | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_dotdash_line_to_omit_first(int x, int y)
{
    plot(PLOT_DOTDASH_OMIT_FIRST | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_dotdash_line_to_omit_both(int x, int y)
{
    plot(PLOT_DOTDASH_OMIT_BOTH | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_point(int x, int y)
{
    plot(PLOT_POINT | PLOTMODE_FG_ABSOLUTE, x, y);
}

/* The fills from a point: one PLOT each, in the foreground colour. */

void vdp_line_fill_leftright_to_nonbg(int x, int y)
{
    plot(PLOT_FILL_LEFTRIGHT_TO_NONBG | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_line_fill_leftright_to_fg(int x, int y)
{
    plot(PLOT_FILL_LEFTRIGHT_TO_FG | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_line_fill_right_to_bg(int x, int y)
{
    plot(PLOT_FILL_RIGHT_TO_BG | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_line_fill_right_to_nonfg(int x, int y)
{
    plot(PLOT_FILL_RIGHT_TO_NONFG | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_flood_fill_to_nonbg(int x, int y)
{
    plot(PLOT_FLOOD_TO_NONBG | PLOTMODE_FG_ABSOLUTE, x, y);
}

void vdp_flood_fill_to_fg(int x, int y)
{
    plot(PLOT_FLOOD_TO_FG | PLOTMODE_FG_ABSOLUTE, x, y);
}

/* Outlines are drawn line by line, as libagon draws them, rather than
 * with a filled shape's code: the VDP has no outline triangle, rectangle
 * or parallelogram. The fourth corner of the parallelogram is the one the
 * VDP would work out for the filled one. */

void vdp_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    vdp_move_to(x1, y1);
    vdp_line_to(x2, y2);
    vdp_line_to(x3, y3);
    vdp_line_to(x1, y1);
}

void vdp_filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3)
{
    plot3(PLOT_TRIANGLE | PLOTMODE_FG_ABSOLUTE, x1, y1, x2, y2, x3, y3);
}

void vdp_rectangle(int x1, int y1, int x2, int y2)
{
    vdp_move_to(x1, y1);
    vdp_line_to(x2, y1);
    vdp_line_to(x2, y2);
    vdp_line_to(x1, y2);
    vdp_line_to(x1, y1);
}

void vdp_filled_rectangle(int x1, int y1, int x2, int y2)
{
    vdp_move_to(x1, y1);
    plot(PLOT_RECTANGLE | PLOTMODE_FG_ABSOLUTE, x2, y2);
}

void vdp_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3)
{
    vdp_move_to(x1, y1);
    vdp_line_to(x2, y2);
    vdp_line_to(x3, y3);
    vdp_line_to(x1 - x2 + x3, y1 - y2 + y3);
    vdp_line_to(x1, y1);
}

void vdp_filled_parallelogram(int x1, int y1, int x2, int y2, int x3, int y3)
{
    plot3(PLOT_PARALLELOGRAM | PLOTMODE_FG_ABSOLUTE, x1, y1, x2, y2, x3, y3);
}

/* A circle is a move to the centre and a plot relative to it: the VDP
 * takes the radius as the distance of that point. */

void vdp_circle(int x, int y, int radius)
{
    plot(PLOT_CIRCLE | PLOTMODE_MOVE_ABSOLUTE, x, y);
    plot(PLOT_CIRCLE | PLOTMODE_FG_RELATIVE, radius, 0);
}

void vdp_filled_circle(int x, int y, int radius)
{
    plot(PLOT_FILLED_CIRCLE | PLOTMODE_MOVE_ABSOLUTE, x, y);
    plot(PLOT_FILLED_CIRCLE | PLOTMODE_FG_RELATIVE, radius, 0);
}

/* The three-point shapes: centre first, and the shape drawn at the last. */

void vdp_arc(int centre_x, int centre_y, int x1, int y1, int x2, int y2)
{
    plot3(PLOT_ARC | PLOTMODE_FG_ABSOLUTE, centre_x, centre_y, x1, y1, x2, y2);
}

void vdp_segment(int centre_x, int centre_y, int x1, int y1, int x2, int y2)
{
    plot3(PLOT_SEGMENT | PLOTMODE_FG_ABSOLUTE, centre_x, centre_y, x1, y1, x2, y2);
}

void vdp_sector(int centre_x, int centre_y, int x1, int y1, int x2, int y2)
{
    plot3(PLOT_SECTOR | PLOTMODE_FG_ABSOLUTE, centre_x, centre_y, x1, y1, x2, y2);
}

void vdp_ellipse(int centre_x, int centre_y, int x1, int y1, int x2, int y2)
{
    plot3(PLOT_ELLIPSE | PLOTMODE_FG_ABSOLUTE, centre_x, centre_y, x1, y1, x2, y2);
}

void vdp_filled_ellipse(int centre_x, int centre_y, int x1, int y1, int x2, int y2)
{
    plot3(PLOT_FILLED_ELLIPSE | PLOTMODE_FG_ABSOLUTE, centre_x, centre_y, x1, y1, x2, y2);
}

/* Copy and move: the source's corners are the two moves, and the mode of
 * the plot says which -- 5 moves, 7 copies, as libagon sends them. */

void vdp_copy_rectangle(int src_x1, int src_y1, int src_x2, int src_y2, int dest_x, int dest_y)
{
    plot3(PLOT_COPY_MOVE | PLOTMODE_BG_ABSOLUTE, src_x1, src_y1, src_x2, src_y2, dest_x, dest_y);
}

void vdp_move_rectangle(int src_x1, int src_y1, int src_x2, int src_y2, int dest_x, int dest_y)
{
    plot3(PLOT_COPY_MOVE | PLOTMODE_FG_ABSOLUTE, src_x1, src_y1, src_x2, src_y2, dest_x, dest_y);
}

/* A path begins with the last two points the cursor visited, so the first
 * two are moves and each point after is a path PLOT. libagon counts the
 * size unsigned: a negative one is a very large one. */

void vdp_fill_path(const int *path, int pathsize)
{
    unsigned n = (unsigned) pathsize / sizeof *path;
    unsigned i;

    if ((unsigned) pathsize < 6 * sizeof *path || n & 1)
        return;

    vdp_move_to(path[0], path[1]);
    vdp_move_to(path[2], path[3]);
    for (i = 4; i < n; i += 2)
        vdp_path_point(path[i], path[i + 1]);
}

void vdp_path_point(int x, int y)
{
    plot(PLOT_PATH | PLOTMODE_FG_ABSOLUTE, x, y);
}

/* The bitmap in its own colours; mode 7 would draw it in the pen's. */
void vdp_plot_bitmap(int x, int y)
{
    plot(PLOT_BITMAP | PLOTMODE_FG_ABSOLUTE, x, y);
}
