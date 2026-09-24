/* <agon/vdp/bitmap.h>'s bitmaps, against the VDP: none of its calls reads
 * an answer back, so each bitmap is drawn and the pixels it should have
 * put on the screen are read back with <agon/vdp/screen.h>'s calls. That
 * holds each call to what it does, not only to how many bytes it is. */
#include "result.h"
#include <agon/vdp.h>

/* A 2 by 2 bitmap, RGBA8888, a row at a time: red, green; blue, white. */
static uint8_t quad[16] = {
    0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static void make_file(const char *name, const uint8_t *data, int n)
{
    uint8_t fh = mos_fopen(name, FA_WRITE | FA_CREATE_ALWAYS);

    mos_fwrite(fh, (char *) data, (uint24_t) n);
    mos_fclose(fh);
}

/* The four pixels of a 2 by 2 drawn at x, y. A colour comes back with red
 * in its low byte, so red prints as 0000ff and blue as ff0000. */
static void quad_at(const char *what, int x, int y)
{
    vdp_flush_drawing_commands();
    say("%s %06x %06x %06x %06x\n", what,
        (int) vdp_return_pixel_colour(x, y), (int) vdp_return_pixel_colour(x + 1, y),
        (int) vdp_return_pixel_colour(x, y + 1), (int) vdp_return_pixel_colour(x + 1, y + 1));
}

int main(void)
{
    int r;

    say("mode %d\n", vdp_mode(8));
    vdp_set_pixel_coordinates();

    vdp_select_bitmap(1);
    vdp_solid_bitmap(4, 4, 0xff, 0x00, 0xff, 0xff);
    vdp_draw_bitmap(10, 10);
    vdp_flush_drawing_commands();
    say("solid %06x %06x %06x\n", (int) vdp_return_pixel_colour(10, 10),
        (int) vdp_return_pixel_colour(13, 13), (int) vdp_return_pixel_colour(14, 13));

    vdp_select_bitmap(2);
    vdp_load_bitmap(2, 2, quad);
    vdp_draw_bitmap(20, 20);
    quad_at("loaded", 20, 20);

    make_file("quad.bin", quad, 16);
    vdp_adv_select_bitmap(0x1000);
    r = vdp_load_bitmap_file("quad.bin", 2, 2);
    vdp_draw_bitmap(30, 20);
    quad_at("from file", 30, 20);
    say("answered %d\n", r);

    vdp_capture_bitmap(20, 20, 21, 21, 3);
    vdp_select_bitmap(3);
    vdp_draw_bitmap(40, 20);
    quad_at("captured", 40, 20);

    vdp_adv_capture_bitmap(20, 20, 21, 21, 0x1001);
    vdp_adv_select_bitmap(0x1001);
    vdp_draw_bitmap(50, 20);
    quad_at("captured by ID", 50, 20);

    make_file("f0.bin", quad, 16);
    make_file("f1.bin", quad, 16);
    say("frames %d\n", vdp_load_sprite_bitmaps("f", "%s%d.bin", 2, 2, 3, 5));
    vdp_select_bitmap(6);
    vdp_draw_bitmap(60, 20);
    quad_at("frame 1", 60, 20);
    say("frames by ID %d\n", vdp_adv_load_sprite_bitmaps("f", "%s%d.bin", 2, 2, 2, 0x2000));
    vdp_adv_select_bitmap(0x2001);
    vdp_draw_bitmap(70, 20);
    quad_at("frame by ID 1", 70, 20);

    return finish();
}
