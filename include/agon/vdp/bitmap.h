/*
 * agon/vdp/bitmap.h -- part of <agon/vdp.h>, which is what a program includes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Bitmaps and sprites (VDU 23, 27), the mouse (VDU 23, 0, &89), mapping a
 * character to a bitmap (&92), fonts (&95), the affine transform (&96),
 * the tile engine (&C2), the copper (&C4) and contexts (&C8).
 *
 * An 8-bit bitmap number n is the bitmap in buffer 64000 + n; the calls
 * named vdp_adv_ take the 16-bit buffer ID itself.
 */
#ifndef ACC_AGON_VDP_BITMAP_H
#define ACC_AGON_VDP_BITMAP_H

/* libagon's name for the most it hands MOS in one call. */
#define LOAD_BMAP_BLOCK 65535

/* The formats of a bitmap made from a buffer, vdp_adv_bitmap_from_buffer. */
#define VDP_BITMAP_RGBA8888     0
#define VDP_BITMAP_RGBA2222     1
#define VDP_BITMAP_MONO         2       /* 1 bit a pixel, in the graphics colour */
#define VDP_BITMAP_NATIVE       3

/* The VDP variables that turn on a part of the VDP that is off until a
 * program sets them (VDU 23, 0, &F8, id; value;): the affine transform, the
 * tile engine and the copper take no bytes at all while theirs is unset. */
#define VDP_VAR_AFFINE_TRANSFORM    0x0001
#define VDP_VAR_HW_SPRITES          0x0002
#define VDP_VAR_TILE_ENGINE         0x0300
#define VDP_VAR_COPPER              0x0310

/* Bitmaps, by 8-bit number. */
void vdp_select_bitmap(int n);
void vdp_load_bitmap(int width, int height, uint8_t *data);    /* RGBA8888 */
int vdp_load_bitmap_file(const char *fname, int width, int height);
void vdp_solid_bitmap(int width, int height, int r, int g, int b, int a);
void vdp_draw_bitmap(int x, int y);

/* Capture the screen between (left, top) and (right, bottom) into a
 * bitmap: two graphics moves, then the capture. */
void vdp_capture_bitmap(uint16_t top, uint16_t left, uint16_t bottom, uint16_t right,
                        uint8_t bitmapID);
void vdp_adv_capture_bitmap(uint16_t top, uint16_t left, uint16_t bottom, uint16_t right,
                            int bufferId);

/* Bitmaps, by 16-bit buffer ID. */
void vdp_adv_select_bitmap(int bufferId);
void vdp_adv_bitmap_from_buffer(int width, int height, int format);

/* Sprites. */
void vdp_select_sprite(int n);
void vdp_clear_sprite(void);
void vdp_add_sprite_bitmap(int n);
void vdp_adv_add_sprite_bitmap(int bitmap_num);
void vdp_activate_sprites(int n);
void vdp_next_sprite_frame(void);
void vdp_prev_sprite_frame(void);
void vdp_nth_sprite_frame(int n);
void vdp_replace_sprite_frame(uint8_t bitmapID);
void vdp_adv_replace_sprite_frame(int bufferId);
void vdp_show_sprite(void);
void vdp_hide_sprite(void);
void vdp_move_sprite_to(int x, int y);
void vdp_move_sprite_by(int x, int y);
void vdp_refresh_sprites(void);
void vdp_reset_sprites(void);          /* sprites, bitmaps and the screen */
void vdp_reset_sprites_only(void);
void vdp_set_sprite_paint_mode(int n); /* a GCOL mode */
void vdp_set_hardware_sprite(void);    /* only once VDP_VAR_HW_SPRITES is set */
void vdp_set_software_sprite(void);

/* Load num bitmaps from the files format names, with the prefix and the
 * frame's number: sprintf(name, fname_format, fname_prefix, i). The
 * bitmaps are bitmap_num onwards; the answer is how many were loaded. */
int vdp_load_sprite_bitmaps(const char *fname_prefix, const char *fname_format,
                            int width, int height, int num, int bitmap_num);
int vdp_adv_load_sprite_bitmaps(const char *fname_prefix, const char *fname_format,
                                int width, int height, int num, int bitmap_num);

/* Select a sprite, give it frames bitmap_num onwards, and activate as
 * many sprites as its number -- libagon's arithmetic, which leaves the
 * sprite itself out: activate sprite + 1 afterwards to show it. */
void vdp_create_sprite(int sprite, int bitmap_num, int frames);
void vdp_adv_create_sprite(int sprite, int bitmap_num, int frames);

/* The mouse. */
void vdp_mouse_enable(void);
void vdp_mouse_disable(void);
void vdp_mouse_reset(void);
void vdp_mouse_set_cursor(int cursorId);
void vdp_mouse_set_position(int X, int Y);
void vdp_mouse_set_area(int x1, int y1, int x2, int y2);   /* VDP 2.16.0 ignores it */
void vdp_mouse_sample_rate(int sampleRate);
void vdp_mouse_resolution(int resolution);
void vdp_mouse_scaling(int scaling);
void vdp_mouse_acceleration(int acceleration);
void vdp_mouse_wheel_accel(int wheelAccel);                 /* 24 bits */
void vdp_mouse_set_bitmap(int hotx, int hoty);  /* the selected bitmap, hot spot */

/* Draw the bitmap in buffer bitmap_num in place of a character. */
void vdp_map_char_to_bitmap(int char_num, int bitmap_num);

/* Fonts, each in the buffer that holds its glyphs; 65535 is the system
 * font. */
#define VDP_FONT_SELECT_ADJUSTBASE  0x01    /* vdp_font_select's flags */

#define VDP_FONT_INFO_WIDTH         0       /* vdp_font_adjust's fields */
#define VDP_FONT_INFO_HEIGHT        1
#define VDP_FONT_INFO_ASCENT        2
#define VDP_FONT_INFO_FLAGS         3
#define VDP_FONT_INFO_CHARPTRS      4
#define VDP_FONT_INFO_POINTSIZE     5
#define VDP_FONT_INFO_INLEADING     6
#define VDP_FONT_INFO_EXLEADING     7
#define VDP_FONT_INFO_WEIGHT        8
#define VDP_FONT_INFO_CHARSET       9
#define VDP_FONT_INFO_CODEPAGE      10

void vdp_font_select(int buffer_id, int flags);
void vdp_font_create(int buffer_id, int width, int height, int ascent, int flags);
void vdp_font_adjust(int buffer_id, int field, int value);
void vdp_font_set_property(int buffer_id, int field, int value);
void vdp_font_delete(int buffer_id);
void vdp_font_copy(int buffer_id);
void vdp_font_debug(int buffer_id);    /* to the VDP's debug log */

/* Draw bitmaps through the affine matrix in buffer affineID, when flags
 * has bit 0 set; only once VDP_VAR_AFFINE_TRANSFORM is set. */
void vdp_adv_use_affine_matrix(uint8_t flags, uint16_t affineID);

/* The tile engine, only once VDP_VAR_TILE_ENGINE is set. Tiles are 8 by
 * 8, a byte a pixel; a tile bank has to be made before tiles are loaded
 * into it, or the VDP takes the pixels for commands. */
void vdp_tilebank_init(int bank, int bit_depth);
void vdp_tilebank_load(int bank, int tile_id, const uint8_t *pixels);  /* 64 bytes */
void vdp_tilebank_draw(int bank, int tile_id, int palette, int x, int y,
                       int x_offset, int y_offset, int attribute);
void vdp_tilebank_free(int bank);
void vdp_tilemap_init(int layer, int size);
void vdp_tilemap_set_tile(int layer, int x, int y, int tile_id, int attribute);
void vdp_tilemap_free(int layer);
void vdp_tilelayer_init(int layer, int layer_size, int tile_size);
void vdp_tilelayer_set_scroll(int layer, int x, int y, int x_offset, int y_offset);
void vdp_tilelayer_update_layerbuffer(int layer);
void vdp_tilelayer_draw_layerbuffer(int layer);
void vdp_tilelayer_draw(int layer);
void vdp_tilelayer_free(int layer);

/* The copper: palettes changed as the screen is drawn. The rest take no
 * bytes until vdp_copper_enable. */
void vdp_copper_enable(void);
void vdp_copper_disable(void);
void vdp_copper_create_palette(uint16_t paletteID);
void vdp_copper_delete_palette(uint16_t paletteID);
void vdp_copper_set_palette_entry(uint16_t paletteID, uint8_t indx, uint8_t red,
                                  uint8_t green, uint8_t blue);
void vdp_copper_set_signal_list(uint16_t bufferID);
void vdp_copper_reset_signal_list(void);

/* Contexts: stacks of the VDP's drawing state. */
#define VDP_CONTEXT_RESET_GPAINT        0x01    /* vdp_context_reset's flags */
#define VDP_CONTEXT_RESET_GPOS          0x02
#define VDP_CONTEXT_RESET_TPAINT        0x04
#define VDP_CONTEXT_RESET_TCURSOR       0x08
#define VDP_CONTEXT_RESET_TBEHAVIOUR    0x10
#define VDP_CONTEXT_RESET_FONTS         0x20
#define VDP_CONTEXT_RESET_CHAR2BITMAP   0x40

void vdp_context_select(int context_id);
void vdp_context_delete(int context_id);
void vdp_context_reset(int flags);
void vdp_context_save(void);
void vdp_context_restore(void);
void vdp_context_save_copy(int context_id);
void vdp_context_restore_all(void);
void vdp_context_clear_stack(void);
void vdp_context_debug(void);          /* to the VDP's debug log */

#endif
