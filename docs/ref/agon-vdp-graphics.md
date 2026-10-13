# `agon/vdp/graphics.h`

The graphics colour, viewport and origin, the line style, and PLOT with
every shape it draws. It is part of [agon/vdp.h](agon-vdp.md), which is
what a program includes; text and the screen mode are in
[agon/vdp/screen.h](agon-vdp-screen.md), and bitmaps in
[agon/vdp/bitmap.h](agon-vdp-bitmap.md).

Coordinates are the VDP's: 16-bit, from the graphics origin, with y up
unless the program has turned logical coordinates off
([`vdp_set_pixel_coordinates`](agon-vdp-screen.md#vdp_logical_scr_dims-vdp_set_pixel_coordinates-vdp_set_logical_coordinates)).
`x;` is a 16-bit argument, sent low byte first.

Nearly all of it is PLOT, VDU 25, a code, `x;` `y;`. The VDP keeps the
last three points it was sent, and a shape takes its corners from them:
a filled triangle is two moves and a plot, and a circle a move to the
centre and a plot relative to it. Where libagon has a call, acc sends
the same commands in the same order.

    vdp_mode(8);
    vdp_set_pixel_coordinates();
    vdp_clg();
    vdp_gcol(GCOLMODE_COLOUR, BRIGHT_RED);
    vdp_filled_circle(160, 120, 40);

## `vdp_clear_graphics`, `vdp_clg`

    void vdp_clear_graphics(void);
    #define vdp_clg() vdp_clear_graphics()

VDU 16: clear the graphics viewport (CLG).

## `vdp_set_graphics_colour`, `vdp_set_graphics_fg_colour`, `vdp_set_graphics_bg_colour`, `vdp_gcol`

    void vdp_set_graphics_colour(uint8_t mode, uint8_t colour);
    void vdp_set_graphics_fg_colour(uint8_t mode, uint8_t colour);
    void vdp_set_graphics_bg_colour(uint8_t mode, uint8_t colour);
    #define vdp_gcol(M, C) vdp_set_graphics_colour(M, C)

VDU 18, `mode`, `colour`: the graphics colour, as GCOL `mode`, `colour`
sets it, painted in one of the `GCOLMODE_` modes below. A colour of 128
or more is the background colour.

`vdp_set_graphics_fg_colour` is the same as `vdp_set_graphics_colour`.
`vdp_set_graphics_bg_colour` takes a colour from 0 to 127 and sends it
with its top bit flipped (`colour ^ 128`), as libagon does: a colour
that is already 128 or more comes out a foreground one.

## `GCOLMODE_COLOUR`, `GCOLMODE_OR`, `GCOLMODE_AND`, `GCOLMODE_EOR`, `GCOLMODE_INVERT`, `GCOLMODE_NOP`, `GCOLMODE_AND_INV_COLOUR`, `GCOLMODE_OR_INV_COLOUR`

| Name                      | Value | The pixel                       |
|---------------------------|-------|---------------------------------|
| `GCOLMODE_COLOUR`         | 0     | set to the colour               |
| `GCOLMODE_OR`             | 1     | ORed with the colour            |
| `GCOLMODE_AND`            | 2     | ANDed with the colour           |
| `GCOLMODE_EOR`            | 3     | EORed with the colour           |
| `GCOLMODE_INVERT`         | 4     | inverted                        |
| `GCOLMODE_NOP`            | 5     | left alone                      |
| `GCOLMODE_AND_INV_COLOUR` | 6     | ANDed with the colour inverted  |
| `GCOLMODE_OR_INV_COLOUR`  | 7     | ORed with the colour inverted   |

## `vdp_set_graphics_viewport`

    void vdp_set_graphics_viewport(int left, int bottom, int right,
                                   int top);

VDU 24, `left;` `bottom;` `right;` `top;`: the graphics viewport.

## `vdp_graphics_origin`

    void vdp_graphics_origin(int x, int y);

VDU 29, `x;` `y;`: the graphics origin, which every later point counts
from.

## `vdp_set_line_thickness`

    void vdp_set_line_thickness(int pixels);

VDU 23, 23, `pixels`: lines `pixels` thick, a byte. libagon's sends
VDU 23, `pixels`, 0, which the VDP takes for some other VDU 23 command;
acc sends what the VDP reads.

## `vdp_set_dotdash_line_pattern`, `vdp_set_dotdash_pattern_length`

    void vdp_set_dotdash_line_pattern(uint8_t b0, uint8_t b1, uint8_t b2,
                                      uint8_t b3, uint8_t b4, uint8_t b5,
                                      uint8_t b6, uint8_t b7);
    void vdp_set_dotdash_pattern_length(int n);

`vdp_set_dotdash_line_pattern` sends VDU 23, 6, `b0`, ..., `b7`: the
pattern of the dot-dash lines, one bit a pixel.

`vdp_set_dotdash_pattern_length` sends VDU 23, 0, &F2, `n`: how many of
those bits are used, 1 to 64, or 0 for the default pattern back. An `n`
above 64 sends nothing, as in libagon, though the VDP takes up to 255.
A negative `n` is not refused: it is sent as its low byte.

## `vdp_plot`

    void vdp_plot(int plot_mode, int x, int y);

VDU 25, `plot_mode`, `x;` `y;`: PLOT. The code is an operation, a
`PLOT_` constant, ORed with a `PLOTMODE_`: what the point is relative
to, and what colour it is drawn in.

    vdp_plot(PLOT_POINT | PLOTMODE_FG_ABSOLUTE, 100, 100);  /* PLOT 69 */

## `PLOTMODE_MOVE_RELATIVE`, `PLOTMODE_FG_RELATIVE`, `PLOTMODE_INVERT_RELATIVE`, `PLOTMODE_BG_RELATIVE`, `PLOTMODE_MOVE_ABSOLUTE`, `PLOTMODE_FG_ABSOLUTE`, `PLOTMODE_INVERT_ABSOLUTE`, `PLOTMODE_BG_ABSOLUTE`

| Name                       | Value | The point is, and drawn in   |
|----------------------------|-------|------------------------------|
| `PLOTMODE_MOVE_RELATIVE`   | 0     | relative; a move             |
| `PLOTMODE_FG_RELATIVE`     | 1     | relative; the foreground     |
| `PLOTMODE_INVERT_RELATIVE` | 2     | relative; inverted           |
| `PLOTMODE_BG_RELATIVE`     | 3     | relative; the background     |
| `PLOTMODE_MOVE_ABSOLUTE`   | 4     | absolute; a move             |
| `PLOTMODE_FG_ABSOLUTE`     | 5     | absolute; the foreground     |
| `PLOTMODE_INVERT_ABSOLUTE` | 6     | absolute; inverted           |
| `PLOTMODE_BG_ABSOLUTE`     | 7     | absolute; the background     |

A relative point is from the last point plotted; an absolute one from
the graphics origin.

## `PLOT_LINE`, `PLOT_LINE_OMIT_LAST`, `PLOT_DOTDASH`, `PLOT_DOTDASH_OMIT_LAST`, `PLOT_LINE_OMIT_FIRST`, `PLOT_LINE_OMIT_BOTH`, `PLOT_DOTDASH_OMIT_FIRST`, `PLOT_DOTDASH_OMIT_BOTH`, `PLOT_POINT`

The line operations for `vdp_plot`, and the call that draws with each.

| Name                           | Hex | Used by                          |
|--------------------------------|-----|----------------------------------|
| `PLOT_LINE`                    | &00 | vdp_line_to                      |
| `PLOT_LINE_OMIT_LAST`          | &08 | vdp_line_to_omit_last            |
| `PLOT_DOTDASH`                 | &10 | vdp_dotdash_line_to              |
| `PLOT_DOTDASH_OMIT_LAST`       | &18 | vdp_dotdash_line_to_omit_last    |
| `PLOT_LINE_OMIT_FIRST`         | &20 | vdp_line_to_omit_first           |
| `PLOT_LINE_OMIT_BOTH`          | &28 | vdp_line_to_omit_both            |
| `PLOT_DOTDASH_OMIT_FIRST`      | &30 | vdp_dotdash_line_to_omit_first   |
| `PLOT_DOTDASH_OMIT_BOTH`       | &38 | vdp_dotdash_line_to_omit_both    |
| `PLOT_POINT`                   | &40 | vdp_point                        |

The omit-first and omit-both dot-dash lines carry the pattern on from
the line before.

## `PLOT_FILL_LEFTRIGHT_TO_NONBG`, `PLOT_FILL_RIGHT_TO_BG`, `PLOT_FILL_LEFTRIGHT_TO_FG`, `PLOT_FILL_RIGHT_TO_NONFG`, `PLOT_FLOOD_TO_NONBG`, `PLOT_FLOOD_TO_FG`

The fill operations for `vdp_plot`, and the call that draws with each.

| Name                           | Hex | Used by                          |
|--------------------------------|-----|----------------------------------|
| `PLOT_FILL_LEFTRIGHT_TO_NONBG` | &48 | vdp_line_fill_leftright_to_nonbg |
| `PLOT_FILL_RIGHT_TO_BG`        | &58 | vdp_line_fill_right_to_bg        |
| `PLOT_FILL_LEFTRIGHT_TO_FG`    | &68 | vdp_line_fill_leftright_to_fg    |
| `PLOT_FILL_RIGHT_TO_NONFG`     | &78 | vdp_line_fill_right_to_nonfg     |
| `PLOT_FLOOD_TO_NONBG`          | &80 | vdp_flood_fill_to_nonbg          |
| `PLOT_FLOOD_TO_FG`             | &88 | vdp_flood_fill_to_fg             |

## `PLOT_TRIANGLE`, `PLOT_RECTANGLE`, `PLOT_PARALLELOGRAM`, `PLOT_CIRCLE`, `PLOT_FILLED_CIRCLE`, `PLOT_ARC`, `PLOT_SEGMENT`, `PLOT_SECTOR`, `PLOT_COPY_MOVE`, `PLOT_ELLIPSE`, `PLOT_FILLED_ELLIPSE`, `PLOT_PATH`, `PLOT_BITMAP`

The shape operations for `vdp_plot`, and the call that draws with each.

| Name                           | Hex | Used by                          |
|--------------------------------|-----|----------------------------------|
| `PLOT_TRIANGLE`                | &50 | vdp_filled_triangle              |
| `PLOT_RECTANGLE`               | &60 | vdp_filled_rectangle             |
| `PLOT_PARALLELOGRAM`           | &70 | vdp_filled_parallelogram         |
| `PLOT_CIRCLE`                  | &90 | vdp_circle                       |
| `PLOT_FILLED_CIRCLE`           | &98 | vdp_filled_circle                |
| `PLOT_ARC`                     | &A0 | vdp_arc                          |
| `PLOT_SEGMENT`                 | &A8 | vdp_segment                      |
| `PLOT_SECTOR`                  | &B0 | vdp_sector                       |
| `PLOT_COPY_MOVE`               | &B8 | vdp_copy_rectangle               |
| `PLOT_ELLIPSE`                 | &C0 | vdp_ellipse                      |
| `PLOT_FILLED_ELLIPSE`          | &C8 | vdp_filled_ellipse               |
| `PLOT_PATH`                    | &D8 | vdp_path_point                   |
| `PLOT_BITMAP`                  | &E8 | vdp_plot_bitmap                  |

With `PLOT_COPY_MOVE`, modes 1 and 5 move the rectangle and the rest
copy it.

## `vdp_move_to`, `vdp_move_by`, `vdp_line_to`, `vdp_line_by`, `vdp_line`

    void vdp_move_to(int x, int y);
    void vdp_move_by(int dx, int dy);
    void vdp_line_to(int x, int y);
    void vdp_line_by(int dx, int dy);
    void vdp_line(int x1, int y1, int x2, int y2);

The graphics cursor moved, or a line drawn from it in the foreground
colour, to a point or by a distance:

| Call          | Sends                                   |
|---------------|-----------------------------------------|
| `vdp_move_to` | PLOT 4, `x;` `y;`                       |
| `vdp_move_by` | PLOT 0, `dx;` `dy;`                     |
| `vdp_line_to` | PLOT 5, `x;` `y;`                       |
| `vdp_line_by` | PLOT 1, `dx;` `dy;`                     |
| `vdp_line`    | PLOT 4, `x1;` `y1;`, then PLOT 5, `x2;` `y2;` |

`vdp_line` moves to its first point itself; it needs no `vdp_move_to`
first.

    vdp_move_to(0, 0);
    vdp_line_to(319, 239);
    vdp_line_by(-319, 0);

## `vdp_line_to_omit_last`, `vdp_line_to_omit_first`, `vdp_line_to_omit_both`

    void vdp_line_to_omit_last(int x, int y);
    void vdp_line_to_omit_first(int x, int y);
    void vdp_line_to_omit_both(int x, int y);

A line from the graphics cursor to (`x`, `y`) without its last point,
its first, or both: PLOT &0D, &25 or &2D, `x;` `y;`.

## `vdp_dotdash_line_to`, `vdp_dotdash_line`, `vdp_dotdash_line_to_omit_last`, `vdp_dotdash_line_to_omit_first`, `vdp_dotdash_line_to_omit_both`

    void vdp_dotdash_line_to(int x, int y);
    void vdp_dotdash_line(int x1, int y1, int x2, int y2);
    void vdp_dotdash_line_to_omit_last(int x, int y);
    void vdp_dotdash_line_to_omit_first(int x, int y);
    void vdp_dotdash_line_to_omit_both(int x, int y);

The same lines in the dot-dash pattern
([`vdp_set_dotdash_line_pattern`](#vdp_set_dotdash_line_pattern-vdp_set_dotdash_pattern_length)):
PLOT &15, &1D, &35 or &3D, `x;` `y;`. `vdp_dotdash_line` is PLOT 4 to
its first point, then PLOT &15 to its second. The omit-first and
omit-both lines carry the pattern on.

## `vdp_point`

    void vdp_point(int x, int y);

PLOT &45, `x;` `y;`: one pixel in the foreground colour.

## `vdp_line_fill_leftright_to_nonbg`, `vdp_line_fill_leftright_to_fg`, `vdp_line_fill_right_to_bg`, `vdp_line_fill_right_to_nonfg`, `vdp_flood_fill_to_nonbg`, `vdp_flood_fill_to_fg`

    void vdp_line_fill_leftright_to_nonbg(int x, int y);
    void vdp_line_fill_leftright_to_fg(int x, int y);
    void vdp_line_fill_right_to_bg(int x, int y);
    void vdp_line_fill_right_to_nonfg(int x, int y);
    void vdp_flood_fill_to_nonbg(int x, int y);
    void vdp_flood_fill_to_fg(int x, int y);

Fills in the foreground colour, one PLOT each. The line fills fill a
row from (`x`, `y`), left and right or right only, up to a pixel that
is or is not the foreground or the background colour; the flood fills
fill the whole area around (`x`, `y`) the same way.

| Call                               | Sends               |
|------------------------------------|---------------------|
| `vdp_line_fill_leftright_to_nonbg` | PLOT &4D, `x;` `y;` |
| `vdp_line_fill_right_to_bg`        | PLOT &5D, `x;` `y;` |
| `vdp_line_fill_leftright_to_fg`    | PLOT &6D, `x;` `y;` |
| `vdp_line_fill_right_to_nonfg`     | PLOT &7D, `x;` `y;` |
| `vdp_flood_fill_to_nonbg`          | PLOT &85, `x;` `y;` |
| `vdp_flood_fill_to_fg`             | PLOT &8D, `x;` `y;` |

## `vdp_triangle`, `vdp_filled_triangle`

    void vdp_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
    void vdp_filled_triangle(int x1, int y1, int x2, int y2, int x3,
                             int y3);

`vdp_triangle` draws the outline line by line, as libagon does: PLOT 4
to the first corner, then PLOT 5 to the second, the third and the first
again. `vdp_filled_triangle` is PLOT 4 to the first corner and the
second, then PLOT &55 to the third.

## `vdp_rectangle`, `vdp_filled_rectangle`

    void vdp_rectangle(int x1, int y1, int x2, int y2);
    void vdp_filled_rectangle(int x1, int y1, int x2, int y2);

The rectangle between two opposite corners. `vdp_rectangle` is PLOT 4
to (`x1`, `y1`), then four PLOT 5s round to (`x2`, `y1`), (`x2`, `y2`),
(`x1`, `y2`) and back. `vdp_filled_rectangle` is PLOT 4 to (`x1`,
`y1`), then PLOT &65 to (`x2`, `y2`).

## `vdp_parallelogram`, `vdp_filled_parallelogram`

    void vdp_parallelogram(int x1, int y1, int x2, int y2, int x3,
                           int y3);
    void vdp_filled_parallelogram(int x1, int y1, int x2, int y2,
                                  int x3, int y3);

Three corners in order. `vdp_filled_parallelogram` is PLOT 4 to the
first two and PLOT &75 to the third, and the VDP works out the fourth.

`vdp_parallelogram` draws the outline line by line: PLOT 4 to the first
corner, then PLOT 5 to the second, the third, the fourth
(`x1 - x2 + x3`, `y1 - y2 + y3`, the one the VDP would work out) and
the first again.

## `vdp_circle`, `vdp_filled_circle`

    void vdp_circle(int x, int y, int radius);
    void vdp_filled_circle(int x, int y, int radius);

A circle around (`x`, `y`): a move to the centre with the circle's code,
then a plot relative to it, whose distance is the radius.
`vdp_circle` sends PLOT &94, `x;` `y;` and PLOT &91, `radius;` 0;.
`vdp_filled_circle` sends PLOT &9C and PLOT &99 the same way.

## `vdp_arc`, `vdp_segment`, `vdp_sector`

    void vdp_arc(int centre_x, int centre_y, int x1, int y1, int x2,
                 int y2);
    void vdp_segment(int centre_x, int centre_y, int x1, int y1, int x2,
                     int y2);
    void vdp_sector(int centre_x, int centre_y, int x1, int y1, int x2,
                    int y2);

An arc, segment or sector of the circle around the centre, from the
first point on it to the second. Each is PLOT 4 to the centre and to
(`x1`, `y1`), then PLOT &A5 (arc), &AD (segment) or &B5 (sector) to
(`x2`, `y2`).

## `vdp_ellipse`, `vdp_filled_ellipse`

    void vdp_ellipse(int centre_x, int centre_y, int x1, int y1, int x2,
                     int y2);
    void vdp_filled_ellipse(int centre_x, int centre_y, int x1, int y1,
                            int x2, int y2);

An ellipse the way Acorn draws one: around the centre, as wide as `x1`
is from it (`y1` does not matter), with its top at (`x2`, `y2`). A top
not above the centre shears it. Each is PLOT 4 to the centre
and to (`x1`, `y1`), then PLOT &C5 (outline) or &CD (filled) to (`x2`,
`y2`).

## `vdp_copy_rectangle`, `vdp_move_rectangle`

    void vdp_copy_rectangle(int src_x1, int src_y1, int src_x2,
                            int src_y2, int dest_x, int dest_y);
    void vdp_move_rectangle(int src_x1, int src_y1, int src_x2,
                            int src_y2, int dest_x, int dest_y);

The rectangle between two corners, copied or moved so that its bottom
left is at (`dest_x`, `dest_y`); a move clears what it leaves behind.
Each is PLOT 4 to the two corners, then PLOT &BF (copy) or &BD (move)
to the destination.

## `vdp_fill_path`, `vdp_path_point`

    void vdp_fill_path(const int *path, int pathsize);
    void vdp_path_point(int x, int y);

A filled path. `vdp_path_point` sends PLOT &DD, `x;` `y;`: one point
added to the path that the last two points the cursor visited begin.
The VDP fills the path when the next thing it is sent is anything else.

`vdp_fill_path` takes `x`, `y` pairs and their size in bytes,
`sizeof path`. It sends PLOT 4 to the first two points and
`vdp_path_point` for each after. With fewer than three points
(`pathsize` under 18, an `int` being 3 bytes), or an odd number of
`int`s, it sends nothing. The size is counted unsigned, as in libagon,
so a negative one is a very large one.

    static const int star[] = {
        160, 200,  130, 40,  250, 140,  70, 140,  190, 40
    };

    vdp_fill_path(star, sizeof star);

## `vdp_plot_bitmap`

    void vdp_plot_bitmap(int x, int y);

PLOT &ED, `x;` `y;`: the current bitmap, at (`x`, `y`), in its own
colours; mode 7 (PLOT &EF) would draw it in the pen's.
Bitmaps are made and chosen with
[agon/vdp/bitmap.h](agon-vdp-bitmap.md).

