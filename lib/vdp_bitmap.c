/*
 * <agon/vdp/bitmap.h>'s calls.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Each call sends the bytes libagon sends for it, which test/agonlib.sh
 * holds it to, except where libagon sends what VDP 2.16.0 does not take:
 * those are said where they are. The calls libagon does not have send
 * what the VDP's own source reads (video/vdu_sprites.h, vdu_sys.h,
 * vdu_fonts.h, vdu_layers.h and vdu_context.h at v2.16.0).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <agon/vdp.h>

#include "vdp_emit.h"

/* How much of a bitmap file is read at a time. libagon reads 64K at a
 * time; the bytes sent are the same whatever the size, and a smaller
 * buffer leaves the heap to the program. */
#define FILE_CHUNK 4096

/* What vdp_load_sprite_bitmaps makes a file's name in. */
static char sprite_fname[256];

/* A run of pixels, at most LOAD_BMAP_BLOCK bytes to a call as libagon
 * hands them to MOS. None at all when there are none: a count of zero
 * would have MOS send up to a zero byte instead, which libagon does. */
static void send_pixels(const uint8_t *data, int n)
{
    while (n > LOAD_BMAP_BLOCK) {
        SEND_BYTES(data, LOAD_BMAP_BLOCK);
        data += LOAD_BMAP_BLOCK;
        n -= LOAD_BMAP_BLOCK;
    }
    if (n > 0)
        SEND_BYTES(data, n);
}

/* ------------------------------------------------------------------ */
/* Bitmaps: VDU 23, 27, command. Bitmap n is the one in buffer 64000 + n. */

void vdp_select_bitmap(int n)
{
    SEND(23, 27, 0, LO(n));
}

/* Command 1 with a nonzero height is width * height RGBA8888 pixels. */
void vdp_load_bitmap(int width, int height, uint8_t *data)
{
    SEND(23, 27, 1, W(width), W(height));
    send_pixels(data, (width << 2) * height);
}

/* Command 1 again, its pixels read from a file. -1 when the file cannot
 * be opened -- nothing is sent then -- or holds fewer pixels than the
 * bitmap: the bytes it lacks are sent as zeros all the same, since the
 * VDP waits for every one. */
int vdp_load_bitmap_file(const char *fname, int width, int height)
{
    uint8_t *buf;
    FILE *fp;
    int n, chunk, got;
    int ret = 0;

    buf = malloc(FILE_CHUNK);
    if (!buf)
        return -1;

    fp = fopen(fname, "rb");
    if (!fp) {
        free(buf);

        return -1;
    }

    SEND(23, 27, 1, W(width), W(height));
    for (n = (width << 2) * height; n > 0; n -= chunk) {
        chunk = n < FILE_CHUNK ? n : FILE_CHUNK;
        got = (int) fread(buf, 1, (size_t) chunk, fp);
        if (got != chunk) {
            memset(buf + got, 0, (size_t) (chunk - got));
            ret = -1;
        }
        SEND_BYTES(buf, chunk);
    }
    fclose(fp);
    free(buf);

    return ret;
}

/* Command 2: the colour is the four bytes of one RGBA8888 pixel. */
void vdp_solid_bitmap(int width, int height, int r, int g, int b, int a)
{
    SEND(23, 27, 2, W(width), W(height), LO(r), LO(g), LO(b), LO(a));
}

void vdp_draw_bitmap(int x, int y)
{
    SEND(23, 27, 3, W(x), W(y));
}

/* The VDP captures the rectangle between the last two graphics positions,
 * so both corners are moved to first (PLOT 4, x; y;). Command 1 with a
 * height of 0 is the capture, into the bitmap its width names.
 *
 * libagon moves to the corners and then sends a single byte, 0x21, in
 * place of the command; this sends the command. */
void vdp_capture_bitmap(uint16_t top, uint16_t left, uint16_t bottom, uint16_t right,
                        uint8_t bitmapID)
{
    SEND(25, 4, W(left), W(top));
    SEND(25, 4, W(right), W(bottom));
    SEND(23, 27, 1, bitmapID, 0, W(0));
}

/* The same into a buffer by its 16-bit ID: command &21 with a height of
 * 0, which then takes no format byte. */
void vdp_adv_capture_bitmap(uint16_t top, uint16_t left, uint16_t bottom, uint16_t right,
                            int bufferId)
{
    SEND(25, 4, W(left), W(top));
    SEND(25, 4, W(right), W(bottom));
    SEND(23, 27, 0x21, W(bufferId), W(0));
}

void vdp_adv_select_bitmap(int bufferId)
{
    SEND(23, 27, 0x20, W(bufferId));
}

/* Command &21 makes the selected buffer a bitmap. A height of 0 makes it
 * a capture instead, which reads no format byte: libagon sends one all
 * the same, and the VDP takes it for a command of its own. */
void vdp_adv_bitmap_from_buffer(int width, int height, int format)
{
    if (!(uint16_t) height) {
        SEND(23, 27, 0x21, W(width), W(height));

        return;
    }
    SEND(23, 27, 0x21, W(width), W(height), LO(format));
}

/* ------------------------------------------------------------------ */
/* Sprites: VDU 23, 27 too. */

void vdp_select_sprite(int n)
{
    SEND(23, 27, 4, LO(n));
}

void vdp_clear_sprite(void)
{
    SEND(23, 27, 5);
}

void vdp_add_sprite_bitmap(int n)
{
    SEND(23, 27, 6, LO(n));
}

void vdp_adv_add_sprite_bitmap(int bitmap_num)
{
    SEND(23, 27, 0x26, W(bitmap_num));
}

void vdp_activate_sprites(int n)
{
    SEND(23, 27, 7, LO(n));
}

void vdp_next_sprite_frame(void)
{
    SEND(23, 27, 8);
}

void vdp_prev_sprite_frame(void)
{
    SEND(23, 27, 9);
}

void vdp_nth_sprite_frame(int n)
{
    SEND(23, 27, 10, LO(n));
}

void vdp_show_sprite(void)
{
    SEND(23, 27, 11);
}

void vdp_hide_sprite(void)
{
    SEND(23, 27, 12);
}

void vdp_move_sprite_to(int x, int y)
{
    SEND(23, 27, 13, W(x), W(y));
}

void vdp_move_sprite_by(int x, int y)
{
    SEND(23, 27, 14, W(x), W(y));
}

void vdp_refresh_sprites(void)
{
    SEND(23, 27, 15);
}

void vdp_reset_sprites(void)
{
    SEND(23, 27, 16);
}

void vdp_reset_sprites_only(void)
{
    SEND(23, 27, 17);
}

void vdp_set_sprite_paint_mode(int n)
{
    SEND(23, 27, 18, LO(n));
}

void vdp_set_hardware_sprite(void)
{
    SEND(23, 27, 19);
}

void vdp_set_software_sprite(void)
{
    SEND(23, 27, 20);
}

void vdp_replace_sprite_frame(uint8_t bitmapID)
{
    SEND(23, 27, 21, bitmapID);
}

void vdp_adv_replace_sprite_frame(int bufferId)
{
    SEND(23, 27, 0x35, W(bufferId));
}

/* Load each frame's file into a bitmap of its own. The count stops at the
 * first file that cannot be opened or is short, as libagon's does. */
static int load_frames(const char *prefix, const char *format, int width, int height,
                       int num, int bitmap_num, void (*select)(int))
{
    uint8_t *buf;
    FILE *fp;
    int size, got, i;

    size = width * height;
    buf = malloc((size_t) (size << 2));
    if (!buf)
        return 0;

    for (i = 0; i < num; i++) {
        snprintf(sprite_fname, sizeof sprite_fname, format, prefix, i);
        fp = fopen(sprite_fname, "rb");
        if (!fp)
            break;

        got = (int) fread(buf, 4, (size_t) size, fp);
        fclose(fp);
        if (got != size)
            break;

        select(bitmap_num + i);
        vdp_load_bitmap(width, height, buf);
    }
    free(buf);

    return i;
}

int vdp_load_sprite_bitmaps(const char *fname_prefix, const char *fname_format,
                            int width, int height, int num, int bitmap_num)
{
    return load_frames(fname_prefix, fname_format, width, height, num, bitmap_num,
                       vdp_select_bitmap);
}

int vdp_adv_load_sprite_bitmaps(const char *fname_prefix, const char *fname_format,
                                int width, int height, int num, int bitmap_num)
{
    return load_frames(fname_prefix, fname_format, width, height, num, bitmap_num,
                       vdp_adv_select_bitmap);
}

/* libagon activates `sprite` sprites at the end, not sprite + 1: the
 * bytes are its. */
void vdp_create_sprite(int sprite, int bitmap_num, int frames)
{
    int i;

    vdp_select_sprite(sprite);
    vdp_clear_sprite();
    for (i = 0; i < frames; i++)
        vdp_add_sprite_bitmap(bitmap_num + i);
    vdp_activate_sprites(sprite);
}

void vdp_adv_create_sprite(int sprite, int bitmap_num, int frames)
{
    int i;

    vdp_select_sprite(sprite);
    vdp_clear_sprite();
    for (i = 0; i < frames; i++)
        vdp_adv_add_sprite_bitmap(bitmap_num + i);
    vdp_activate_sprites(sprite);
}

/* ------------------------------------------------------------------ */
/* The mouse: VDU 23, 0, &89, command. */

void vdp_mouse_enable(void)
{
    SEND(23, 0, 0x89, 0);
}

void vdp_mouse_disable(void)
{
    SEND(23, 0, 0x89, 1);
}

void vdp_mouse_reset(void)
{
    SEND(23, 0, 0x89, 2);
}

void vdp_mouse_set_cursor(int cursorId)
{
    SEND(23, 0, 0x89, 3, W(cursorId));
}

void vdp_mouse_set_position(int X, int Y)
{
    SEND(23, 0, 0x89, 4, W(X), W(Y));
}

/* The VDP reads the four corners and does nothing with them yet. */
void vdp_mouse_set_area(int x1, int y1, int x2, int y2)
{
    SEND(23, 0, 0x89, 5, W(x1), W(y1), W(x2), W(y2));
}

void vdp_mouse_sample_rate(int sampleRate)
{
    SEND(23, 0, 0x89, 6, LO(sampleRate));
}

void vdp_mouse_resolution(int resolution)
{
    SEND(23, 0, 0x89, 7, LO(resolution));
}

void vdp_mouse_scaling(int scaling)
{
    SEND(23, 0, 0x89, 8, LO(scaling));
}

void vdp_mouse_acceleration(int acceleration)
{
    SEND(23, 0, 0x89, 9, W(acceleration));
}

void vdp_mouse_wheel_accel(int wheelAccel)
{
    SEND(23, 0, 0x89, 10, U24(wheelAccel));
}

/* A 23, 27 command, but a mouse one: the cursor is the selected bitmap. */
void vdp_mouse_set_bitmap(int hotx, int hoty)
{
    SEND(23, 27, 0x40, LO(hotx), LO(hoty));
}

/* ------------------------------------------------------------------ */
/* VDU 23, 0, &92, char, bitmapId; */

void vdp_map_char_to_bitmap(int char_num, int bitmap_num)
{
    SEND(23, 0, 0x92, LO(char_num), W(bitmap_num));
}

/* ------------------------------------------------------------------ */
/* Fonts: VDU 23, 0, &95, command, bufferId; */

void vdp_font_select(int buffer_id, int flags)
{
    SEND(23, 0, 0x95, 0, W(buffer_id), LO(flags));
}

void vdp_font_create(int buffer_id, int width, int height, int ascent, int flags)
{
    SEND(23, 0, 0x95, 1, W(buffer_id), LO(width), LO(height), LO(ascent), LO(flags));
}

/* The value is 16 bits. libagon sends only its low byte, and the VDP
 * takes the next command's first byte for the high one. */
void vdp_font_adjust(int buffer_id, int field, int value)
{
    SEND(23, 0, 0x95, 2, W(buffer_id), LO(field), W(value));
}

/* libagon's other name for vdp_font_adjust, which its header leaves out. */
void vdp_font_set_property(int buffer_id, int field, int value)
{
    vdp_font_adjust(buffer_id, field, value);
}

/* Buffer 65535 clears every font. */
void vdp_font_delete(int buffer_id)
{
    SEND(23, 0, 0x95, 4, W(buffer_id));
}

void vdp_font_copy(int buffer_id)
{
    SEND(23, 0, 0x95, 5, W(buffer_id));
}

void vdp_font_debug(int buffer_id)
{
    SEND(23, 0, 0x95, 0x20, W(buffer_id));
}

/* ------------------------------------------------------------------ */
/* VDU 23, 0, &96, flags, bufferId; */

void vdp_adv_use_affine_matrix(uint8_t flags, uint16_t affineID)
{
    SEND(23, 0, 0x96, flags, W(affineID));
}

/* ------------------------------------------------------------------ */
/* The tile engine: VDU 23, 0, &C2, command. The zeros are the VDP's
 * reserved arguments, which it reads and ignores. */

void vdp_tilebank_init(int bank, int bit_depth)
{
    SEND(23, 0, 0xC2, 0x00, LO(bank), LO(bit_depth), 0, 0);
}

void vdp_tilebank_load(int bank, int tile_id, const uint8_t *pixels)
{
    SEND(23, 0, 0xC2, 0x01, LO(bank), LO(tile_id));
    SEND_BYTES(pixels, 64);
}

void vdp_tilebank_draw(int bank, int tile_id, int palette, int x, int y,
                       int x_offset, int y_offset, int attribute)
{
    SEND(23, 0, 0xC2, 0x06, LO(bank), LO(tile_id), LO(palette), LO(x), LO(y),
         LO(x_offset), LO(y_offset), LO(attribute));
}

void vdp_tilebank_free(int bank)
{
    SEND(23, 0, 0xC2, 0x07, LO(bank));
}

void vdp_tilemap_init(int layer, int size)
{
    SEND(23, 0, 0xC2, 0x10, LO(layer), LO(size), 0, 0);
}

void vdp_tilemap_set_tile(int layer, int x, int y, int tile_id, int attribute)
{
    SEND(23, 0, 0xC2, 0x11, LO(layer), LO(x), LO(y), LO(tile_id), LO(attribute));
}

void vdp_tilemap_free(int layer)
{
    SEND(23, 0, 0xC2, 0x17, LO(layer));
}

void vdp_tilelayer_init(int layer, int layer_size, int tile_size)
{
    SEND(23, 0, 0xC2, 0x18, LO(layer), LO(layer_size), LO(tile_size), 0);
}

void vdp_tilelayer_set_scroll(int layer, int x, int y, int x_offset, int y_offset)
{
    SEND(23, 0, 0xC2, 0x1A, LO(layer), LO(x), LO(y), LO(x_offset), LO(y_offset));
}

void vdp_tilelayer_update_layerbuffer(int layer)
{
    SEND(23, 0, 0xC2, 0x1C, LO(layer));
}

void vdp_tilelayer_draw_layerbuffer(int layer)
{
    SEND(23, 0, 0xC2, 0x1D, LO(layer));
}

void vdp_tilelayer_draw(int layer)
{
    SEND(23, 0, 0xC2, 0x1E, LO(layer));
}

void vdp_tilelayer_free(int layer)
{
    SEND(23, 0, 0xC2, 0x1F, LO(layer));
}

/* ------------------------------------------------------------------ */
/* The copper: VDU 23, 0, &C4, command, once its VDP variable is set --
 * to anything: libagon sets it to 0, and the VDP asks only that it be
 * there. */

void vdp_copper_enable(void)
{
    SEND(23, 0, 0xF8, W(VDP_VAR_COPPER), W(0));
}

void vdp_copper_disable(void)
{
    SEND(23, 0, 0xF9, W(VDP_VAR_COPPER));
}

void vdp_copper_create_palette(uint16_t paletteID)
{
    SEND(23, 0, 0xC4, 0, W(paletteID));
}

void vdp_copper_delete_palette(uint16_t paletteID)
{
    SEND(23, 0, 0xC4, 1, W(paletteID));
}

void vdp_copper_set_palette_entry(uint16_t paletteID, uint8_t indx, uint8_t red,
                                  uint8_t green, uint8_t blue)
{
    SEND(23, 0, 0xC4, 2, W(paletteID), indx, red, green, blue);
}

void vdp_copper_set_signal_list(uint16_t bufferID)
{
    SEND(23, 0, 0xC4, 3, W(bufferID));
}

void vdp_copper_reset_signal_list(void)
{
    SEND(23, 0, 0xC4, 4);
}

/* ------------------------------------------------------------------ */
/* Contexts: VDU 23, 0, &C8, command. */

void vdp_context_select(int context_id)
{
    SEND(23, 0, 0xC8, 0, LO(context_id));
}

void vdp_context_delete(int context_id)
{
    SEND(23, 0, 0xC8, 1, LO(context_id));
}

void vdp_context_reset(int flags)
{
    SEND(23, 0, 0xC8, 2, LO(flags));
}

void vdp_context_save(void)
{
    SEND(23, 0, 0xC8, 3);
}

void vdp_context_restore(void)
{
    SEND(23, 0, 0xC8, 4);
}

void vdp_context_save_copy(int context_id)
{
    SEND(23, 0, 0xC8, 5, LO(context_id));
}

void vdp_context_restore_all(void)
{
    SEND(23, 0, 0xC8, 6);
}

void vdp_context_clear_stack(void)
{
    SEND(23, 0, 0xC8, 7);
}

void vdp_context_debug(void)
{
    SEND(23, 0, 0xC8, 0x80);
}
