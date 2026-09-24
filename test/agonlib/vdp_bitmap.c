/* <agon/vdp/bitmap.h> against libagon: see capture.h. */
#include "capture.h"
#include <agon/vdp.h>

/* Sixteen bytes: a 2 by 2 RGBA8888 bitmap, none of its bytes zero. */
static uint8_t pixels[16] = {
    0xff, 0x01, 0x02, 0xff, 0x03, 0xff, 0x04, 0xff,
    0x05, 0x06, 0xff, 0xff, 0x07, 0x08, 0x09, 0x80
};

/* A tile: 64 bytes, one a pixel. */
static uint8_t tile[64];

/* A file on the card, through MOS, for the loaders to read. */
static void make_file(const char *name, const uint8_t *data, int n)
{
    uint8_t fh = mos_fopen(name, FA_WRITE | FA_CREATE_ALWAYS);

    mos_fwrite(fh, (char *) data, (uint24_t) n);
    mos_fclose(fh);
}

/* What a loader answered, on a line of its own. */
static void answer(int r)
{
    static char line[32];

    sprintf(line, "= %d\n", r);
    out(line);
}

static void bitmaps(void)
{
    int r;

    CALL(vdp_select_bitmap(3));
    CALL(vdp_select_bitmap(300));
    CALL(vdp_load_bitmap(2, 2, pixels));
    CALL(vdp_load_bitmap(1, 3, pixels));
    CALL(vdp_solid_bitmap(4, 3, 255, 128, 0, 255));
    CALL(vdp_solid_bitmap(300, 2, 1, 2, 3, 4));
    CALL(vdp_draw_bitmap(320, 240));
    CALL(vdp_draw_bitmap(-5, 1000));
    CALL(vdp_adv_select_bitmap(0xFA01));
    CALL(vdp_adv_select_bitmap(7));
    CALL(vdp_adv_bitmap_from_buffer(16, 8, 1));
    CALL(vdp_adv_bitmap_from_buffer(8, 8, 2));
    NEW(vdp_adv_capture_bitmap(10, 20, 50, 60, 0x1234),
        "19 04 14 00 0a 00 19 04 3c 00 32 00 17 1b 21 34 12 00 00");

#ifndef AGONDEV
    CALL(vdp_load_bitmap(0, 5, pixels));
    CALL(vdp_capture_bitmap(10, 20, 50, 60, 7));
    CALL(vdp_adv_bitmap_from_buffer(5, 0, 1));
#else
    /* A bitmap with no pixels: libagon asks MOS for a count of 0, which
     * MOS takes to mean up to a zero byte, and sends all sixteen of these
     * and whatever follows them. */
    expect("vdp_load_bitmap(0, 5, pixels)", "17 1b 01 00 00 05 00");
    /* libagon moves to the corners and then sends one byte, 0x21 -- the
     * first byte of its own code -- in place of VDU 23, 27, 1, n; 0; */
    expect("vdp_capture_bitmap(10, 20, 50, 60, 7)",
           "19 04 14 00 0a 00 19 04 3c 00 32 00 17 1b 01 07 00 00 00");
    /* A height of 0 is a capture, which takes no format byte: libagon
     * sends one, which the VDP reads as VDU 1, and so eats the byte after. */
    expect("vdp_adv_bitmap_from_buffer(5, 0, 1)", "17 1b 21 05 00 00 00");
#endif

    /* The file loaders. */
    make_file("bm.bin", pixels, 16);
    CALL(r = vdp_load_bitmap_file("bm.bin", 2, 2));
    answer(r);
    CALL(r = vdp_load_bitmap_file("nope.bin", 2, 2));
    answer(r);
    CALL(r = vdp_load_bitmap_file("bm.bin", 0, 2));
    answer(r);
#ifndef AGONDEV
    CALL(r = vdp_load_bitmap_file("bm.bin", 3, 2));
#else
    /* The file is 8 bytes short of the bitmap. Both answer -1 and send the
     * bitmap's full 24 bytes, which the VDP waits for; libagon's last 8 are
     * whatever was in its buffer, acc's are zeros. */
    expect("r = vdp_load_bitmap_file(\"bm.bin\", 3, 2)",
           "17 1b 01 03 00 02 00 ff 01 02 ff 03 ff 04 ff 05 06 ff ff 07 08 09 80"
           " 00 00 00 00 00 00 00 00");
    r = -1;
#endif
    answer(r);

    make_file("spr0.bin", pixels, 16);
    make_file("spr1.bin", pixels + 4, 12);
    make_file("spr2.bin", pixels, 16);
    make_file("frm0.bin", pixels, 16);
    make_file("frm1.bin", pixels, 16);
    CALL(r = vdp_load_sprite_bitmaps("frm", "%s%d.bin", 2, 2, 3, 10));
    answer(r);
    CALL(r = vdp_load_sprite_bitmaps("frm", "%s%d.bin", 2, 2, 2, 255));
    answer(r);
    CALL(r = vdp_load_sprite_bitmaps("frm", "%s%d.bin", 2, 2, 0, 10));
    answer(r);
    CALL(r = vdp_load_sprite_bitmaps("frm", "%s%d.bin", 2, 2, -1, 10));
    answer(r);
    CALL(r = vdp_load_sprite_bitmaps("spr", "%s%d.bin", 2, 2, 3, 10));
    answer(r);
    CALL(r = vdp_adv_load_sprite_bitmaps("frm", "%s%d.bin", 2, 2, 5, 0xFA00));
    answer(r);
    CALL(r = vdp_adv_load_sprite_bitmaps("frm", "%s%d.bin", 1, 1, 2, 0x1FF));
    answer(r);
}

static void sprites(void)
{
    CALL(vdp_select_sprite(1));
    CALL(vdp_select_sprite(256));
    CALL(vdp_clear_sprite());
    CALL(vdp_add_sprite_bitmap(3));
    CALL(vdp_adv_add_sprite_bitmap(0xFA03));
    CALL(vdp_activate_sprites(2));
    CALL(vdp_next_sprite_frame());
    CALL(vdp_prev_sprite_frame());
    CALL(vdp_nth_sprite_frame(1));
    CALL(vdp_replace_sprite_frame(4));
    NEW(vdp_adv_replace_sprite_frame(0xFA04), "17 1b 35 04 fa");
    CALL(vdp_show_sprite());
    CALL(vdp_hide_sprite());
    CALL(vdp_move_sprite_to(100, 50));
    CALL(vdp_move_sprite_to(-1, 0x1234));
    CALL(vdp_move_sprite_by(-3, 4));
    CALL(vdp_refresh_sprites());
    CALL(vdp_set_sprite_paint_mode(3));
    CALL(vdp_set_hardware_sprite());
    CALL(vdp_set_software_sprite());
    CALL(vdp_create_sprite(2, 10, 3));
    CALL(vdp_create_sprite(1, 254, 3));
    CALL(vdp_create_sprite(1, 5, 0));
    CALL(vdp_create_sprite(1, 5, -2));
    CALL(vdp_adv_create_sprite(1, 0xFA0A, 2));
    CALL(vdp_adv_create_sprite(0, 0xFFFF, 2));
    CALL(vdp_reset_sprites_only());
    CALL(vdp_reset_sprites());
}

static void mouse(void)
{
    CALL(vdp_mouse_enable());
    CALL(vdp_mouse_disable());
    CALL(vdp_mouse_reset());
    CALL(vdp_mouse_set_cursor(0x1234));
    CALL(vdp_mouse_set_position(640, 480));
    CALL(vdp_mouse_set_position(-1, 70000));
    NEW(vdp_mouse_set_area(0, 1, 639, 479), "17 00 89 05 00 00 01 00 7f 02 df 01");
    CALL(vdp_mouse_sample_rate(60));
    CALL(vdp_mouse_resolution(2));
    CALL(vdp_mouse_scaling(1));
    CALL(vdp_mouse_acceleration(180));
    CALL(vdp_mouse_acceleration(0x1ABCD));
    CALL(vdp_mouse_wheel_accel(60000));
    CALL(vdp_mouse_wheel_accel(0x123456));
    CALL(vdp_select_bitmap(1));
    CALL(vdp_solid_bitmap(4, 4, 255, 255, 255, 255));
    CALL(vdp_mouse_set_bitmap(1, 2));
    CALL(vdp_mouse_set_bitmap(300, -1));
}

static void fonts(void)
{
    CALL(vdp_map_char_to_bitmap(65, 0xFA01));
    CALL(vdp_map_char_to_bitmap(300, 1));
    CALL(vdp_font_copy(0xFA10));
    CALL(vdp_font_create(0x1234, 8, 16, 12, 0));
    CALL(vdp_font_create(0xFA10, 8, 8, 7, 0));
    CALL(vdp_font_select(0xFA10, 1));
    CALL(vdp_font_select(65535, 0));
    NEW(vdp_font_debug(0xFA10), "17 00 95 20 10 fa");
#ifndef AGONDEV
    CALL(vdp_font_adjust(0xFA10, 2, 0x0107));
    CALL(vdp_font_set_property(0xFA10, 1, 9));
#else
    /* The value is a word: libagon sends only its low byte, and the VDP
     * takes the next command's first byte for its high one. */
    expect("vdp_font_adjust(0xFA10, 2, 0x0107)", "17 00 95 02 10 fa 02 07 01");
    expect("vdp_font_set_property(0xFA10, 1, 9)", "17 00 95 02 10 fa 01 09 00");
#endif
    CALL(vdp_font_delete(0xFA10));
    CALL(vdp_font_delete(65535));
}

static void affine(void)
{
    CALL(vdp_set_variable(1, 1));
    CALL(vdp_adv_use_affine_matrix(1, 0x1234));
    CALL(vdp_adv_use_affine_matrix(0, 65535));
    CALL(vdp_clear_variable(1));
}

static void tiles(void)
{
    int i;

    for (i = 0; i < 64; i++)
        tile[i] = (uint8_t) (i + 1);

    CALL(vdp_set_variable(0x300, 1));
    NEW(vdp_tilebank_init(0, 0), "17 00 c2 00 00 00 00 00");
    NEW(vdp_tilebank_load(0, 1, tile),
        "17 00 c2 01 00 01"
        " 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10"
        " 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20"
        " 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30"
        " 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f 40");
    NEW(vdp_tilebank_draw(0, 1, 0, 2, 3, 4, 5, 1), "17 00 c2 06 00 01 00 02 03 04 05 01");
    NEW(vdp_tilemap_init(0, 0), "17 00 c2 10 00 00 00 00");
    NEW(vdp_tilemap_set_tile(0, 5, 6, 1, 2), "17 00 c2 11 00 05 06 01 02");
    NEW(vdp_tilelayer_init(0, 2, 0), "17 00 c2 18 00 02 00 00");
    NEW(vdp_tilelayer_set_scroll(0, 1, 2, 3, 4), "17 00 c2 1a 00 01 02 03 04");
    NEW(vdp_tilelayer_update_layerbuffer(0), "17 00 c2 1c 00");
    NEW(vdp_tilelayer_draw_layerbuffer(0), "17 00 c2 1d 00");
    NEW(vdp_tilelayer_draw(0), "17 00 c2 1e 00");
    NEW(vdp_tilelayer_free(0), "17 00 c2 1f 00");
    NEW(vdp_tilemap_free(0), "17 00 c2 17 00");
    NEW(vdp_tilebank_free(0), "17 00 c2 07 00");
    CALL(vdp_clear_variable(0x300));
}

static void copper(void)
{
    CALL(vdp_copper_enable());
    CALL(vdp_copper_create_palette(0x0102));
    CALL(vdp_copper_set_palette_entry(0x0102, 5, 255, 128, 1));
    CALL(vdp_copper_set_signal_list(0xABCD));
    CALL(vdp_copper_reset_signal_list());
    CALL(vdp_copper_delete_palette(0x0102));
    CALL(vdp_copper_disable());
}

static void contexts(void)
{
    CALL(vdp_context_save());
    CALL(vdp_context_restore());
    CALL(vdp_context_save_copy(2));
    CALL(vdp_context_restore_all());
    CALL(vdp_context_select(1));
    CALL(vdp_context_select(0));
    CALL(vdp_context_delete(1));
    CALL(vdp_context_delete(300));
    CALL(vdp_context_reset(0xFF));
    CALL(vdp_context_reset(0x21));
    CALL(vdp_context_clear_stack());
    NEW(vdp_context_debug(), "17 00 c8 80");
}

int main(void)
{
    bitmaps();
    sprites();
    mouse();
    fonts();
    affine();
    tiles();
    copper();
    contexts();
    done();
    return 0;
}
