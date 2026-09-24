/* <agon/vdp/buffer.h> against libagon: see capture.h.
 *
 * The commands are played into the VDP afterwards in this order, so they
 * are built to leave it sane: every buffer a command reads or adjusts
 * exists first (the VDP stops reading some commands when a buffer is
 * missing, and the rest of their bytes would run as VDU codes), the
 * buffers that get called hold only VDU 0, which does nothing, and a jump
 * at the top level is a call. Numbers are written out rather than named,
 * since libagon's build does not have the names. */
#include "capture.h"
#include <agon/vdp.h>

#include <stdarg.h>

#ifdef AGONDEV
/* In libagon's library but not its header. */
void vdp_adv_jump_conditional(int bufferId, int operation, int checkBufferId, int checkOffset);
#endif

/* Where libagon sends what the VDP does not take: acc's build sends what
 * the VDP does, and libagon's build writes that, worked out by hand. */
#define LIBAGON_BUG(x, hex) NEW(x, hex)

/* Bytes of a command's that the call leaves to its caller. */
static void raw(int n, ...)
{
    va_list ap;

    va_start(ap, n);
    while (n-- > 0)
        putch(va_arg(ap, int));
    va_end(ap);
}

static char zeros[8];
static const unsigned char adj[] = { 0, 0, 2, 0, 9, 9 };
static const unsigned char ops[] = { 1, 2, 3 };
static const unsigned char cond[] = { 1, 0, 0, 0, 1, 0, 3, 0 };
static const unsigned char cond2[] = { 1, 0, 0, 0 };
static const unsigned char blk[] = { 1, 0, 0x80, 0, 0, 0, 1, 0, 0, 0 };
static const float xy[] = { 1.0f, 2.0f };
static const float xyz[] = { 1.0f, 2.0f, 0.5f };
static const unsigned char rot[] = { 0xc0, 90, 0 };
static const unsigned char rot3[] = { 0xc0, 90, 0, 0, 0, 45, 0 };
static const unsigned char smul[] = { 120, 0, 0xc0, 3, 0 };
static const unsigned char targs[] = { 1, 0, 0, 0 };
static const unsigned char map2[] = { 0x00, 0xff };
static const unsigned char map8[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

int main(void)
{
    /* The buffers the rest use: 1 and 4660 writable and zero, 2 four VDU
     * 0s to call. */
    CALL(vdp_adv_clear_buffer(65535));
    CALL(vdp_adv_create(1, 16));
    CALL(vdp_adv_create(4660, 8));
    CALL(vdp_adv_write_block_data(2, 4, zeros));
    CALL(vdp_adv_write_block(300, 3); raw(3, 0, 0, 0));
    CALL(vdp_adv_write_block_data(65535, 2, zeros));
    CALL(vdp_adv_call_buffer(2));
    CALL(vdp_adv_call_buffer(4660));
    CALL(vdp_adv_call_buffer(9999));
    CALL(vdp_adv_clear_buffer(300));
    CALL(vdp_adv_stream(0));
    CALL(vdp_adv_stream(1); vdp_adv_stream(0));

    /* Adjust, and the operand libagon leaves to the caller. */
    CALL(vdp_adv_adjust(1, 0, 3));
    CALL(vdp_adv_adjust(1, 1, 4660));
    CALL(vdp_adv_adjust(1, 3, 2); raw(1, 5));

    /* Conditions, whole for EXISTS and NOT_EXISTS. */
    CALL(vdp_adv_call_conditional(2, 0, 1, 0));
    CALL(vdp_adv_call_conditional(4660, 1, 65535, 300));
    CALL(vdp_adv_call_conditional(2, 2, 1, 3); raw(1, 255));
    CALL(vdp_adv_jump_conditional(2, 1, 1, 0));
    CALL(vdp_adv_jump_buffer(2));
    CALL(vdp_adv_jump_buffer(65535));

    /* Offsets: 24 bits, and a block number after one with the top bit. */
    CALL(vdp_adv_jump_offset(2, 1));
    CALL(vdp_adv_jump_offset(4660, 65540));
    CALL(vdp_adv_jump_offset(2, -1));
    CALL(vdp_adv_jump_offset_block(2, 0x800001, 0));
    CALL(vdp_adv_jump_offset_conditional(2, 1); raw(5, 0, 1, 0, 0, 0));
    CALL(vdp_adv_call_offset(2, 2));
    CALL(vdp_adv_call_offset(2, 0x800000));
    CALL(vdp_adv_call_offset_conditional(2, 0); raw(5, 1, 1, 0, 0, 0));
    /* libagon's block calls send nothing unless the caller has set the
     * offset's top bit already... */
    LIBAGON_BUG(vdp_adv_jump_offset_block(2, 1, 0), "17 00 a0 02 00 09 01 00 80 00 00");
    /* ...and send command 9, a jump, whichever they are. */
    LIBAGON_BUG(vdp_adv_jump_offset_block_conditional(2, 0x800001, 0); raw(5, 0, 1, 0, 0, 0),
                "17 00 a0 02 00 0a 01 00 80 00 00 00 01 00 00 00");
    LIBAGON_BUG(vdp_adv_call_offset_block(2, 0x800002, 0), "17 00 a0 02 00 0b 02 00 80 00 00");
    LIBAGON_BUG(vdp_adv_call_offset_block(2, 3, 0), "17 00 a0 02 00 0b 03 00 80 00 00");
    LIBAGON_BUG(vdp_adv_call_offset_block_conditional(2, 0, 0); raw(5, 1, 1, 0, 0, 0),
                "17 00 a0 02 00 0c 00 00 80 00 00 01 01 00 00 00");

    /* Copying, splitting, spreading; lists of buffers end with 65535. */
    CALL(vdp_adv_copy_multiple(5, 2, 2, 4660));
    CALL(vdp_adv_copy_multiple(6, 1, 2));
    CALL(vdp_adv_copy_multiple(7, 0));
    CALL(vdp_adv_consolidate(5));
    CALL(vdp_adv_split(5, 4));
    CALL(vdp_adv_split_multiple(5, 2, 2, 8, 9));
    CALL(vdp_adv_split_multiple_from(5, 2, 10));
    CALL(vdp_adv_split_by_width(5, 2, 2));
    CALL(vdp_adv_split_by_width_multiple(5, 2, 2, 20, 21));
    CALL(vdp_adv_split_by_width_multiple_from(5, 2, 2, 30));
    CALL(vdp_adv_spread_multiple(5, 2, 40, 41));
    CALL(vdp_adv_spread_multiple_from(5, 50));
    CALL(vdp_adv_copy_multiple(5, 2, 2, 4660));
    CALL(vdp_adv_reverse_block_order(5));
    CALL(vdp_adv_reverse_block_data(5, 3, 1, 0));
    CALL(vdp_adv_reverse_block_data(5, 4, 0, 2));
    CALL(vdp_adv_reverse_block_data(5, 8, 0, 0));
    /* libagon sends a value size for 1 and 2 and both sizes for 0, which
     * take neither, and no sizes for anything above 4. */
    LIBAGON_BUG(vdp_adv_reverse_block_data(5, 0, 1, 2), "17 00 a0 05 00 18 00");
    LIBAGON_BUG(vdp_adv_reverse_block_data(5, 1, 1, 2), "17 00 a0 05 00 18 01");
    LIBAGON_BUG(vdp_adv_reverse_block_data(5, 7, 1, 2), "17 00 a0 05 00 18 07 01 00 02 00");
    LIBAGON_BUG(vdp_adv_reverse_block_data(5, 12, 1, 2), "17 00 a0 05 00 18 0c 02 00");
    CALL(vdp_adv_copy_multiple_by_reference(60, 2, 2, 4660));
    CALL(vdp_adv_copy_multiple_consolidate(61, 2, 2, 4660));
    CALL(vdp_adv_compress_buffer(70, 61));
    CALL(vdp_adv_decompress_buffer(71, 70));

    /* The VDP takes affine transforms only with its test flag 1 set. */
    CALL(raw(7, 23, 0, 248, 1, 0, 1, 0));
    CALL(vdp_adv_create_rotated_buffer(80, 45));
    CALL(vdp_adv_create_rotated_buffer(80, 65535));
    CALL(vdp_adv_create_scaled_buffer(81, 1.5, -0.5));
    CALL(vdp_adv_create_scaled_buffer(81, 0.3, -0.3));
    CALL(vdp_adv_create_translated_buffer(82, 5, 7));
    CALL(vdp_adv_create_translated_buffer(82, -5, -7));
    /* libagon sends x's high byte for y's. */
    LIBAGON_BUG(vdp_adv_create_translated_buffer(82, 5, -3), "17 00 a0 52 00 20 06 c0 05 00 fd ff");
    CALL(vdp_adv_create_skewed_buffer(83, 0.25, 2.0));
    CALL(vdp_adv_apply_transformation_with_stride(4660, 90, 81, 0, 4));

    /* What libagon does not have. */
    NEW(vdp_adv_command(2, 128, 0, 0), "17 00 a0 02 00 80");
    NEW(vdp_adv_command(2, 12, blk, 10), "17 00 a0 02 00 0c 01 00 80 00 00 00 01 00 00 00");

    NEW(vdp_adv_adjust_args(1, 195, adj, 6), "17 00 a0 01 00 05 c3 00 00 02 00 09 09");
    NEW(vdp_adv_adjust_value(1, 3, 2, 5), "17 00 a0 01 00 05 03 02 00 05");
    NEW(vdp_adv_adjust_value(1, 19, 2, 5), "17 00 a0 01 00 05 13 02 00 00 05");
    NEW(vdp_adv_adjust_value(1, 0, 1, 99), "17 00 a0 01 00 05 00 01 00");
    NEW(vdp_adv_adjust_multi(1, 5, 0, 4, 15), "17 00 a0 01 00 05 45 00 00 04 00 0f");
    NEW(vdp_adv_adjust_multi(1, 16, 0, 4, 0), "17 00 a0 01 00 05 50 00 00 00 04 00 00");
    NEW(vdp_adv_adjust_multi_data(1, 3, 0, 3, ops), "17 00 a0 01 00 05 83 00 00 03 00 01 02 03");
    NEW(vdp_adv_adjust_multi_data(1, 0x46, 4, 3, ops), "17 00 a0 01 00 05 c6 04 00 03 00 01 02 03");
    NEW(vdp_adv_adjust_buffer(1, 4, 8, 1, 0), "17 00 a0 01 00 05 24 08 00 01 00 00 00");
    NEW(vdp_adv_adjust_buffer(1, 18, 8, 4660, 1), "17 00 a0 01 00 05 32 08 00 00 34 12 01 00 00");

    NEW(vdp_adv_call_if(2, 130, 1, 0, 4660), "17 00 a0 02 00 06 82 01 00 00 00 34 12");
    NEW(vdp_adv_call_if(2, 64, 1, 0, 0), "17 00 a0 02 00 06 40 01 00");
    NEW(vdp_adv_call_if(2, 71, 1, 0, 1), "17 00 a0 02 00 06 47 01 00 01");
    NEW(vdp_adv_jump_if(2, 19, 1, 2, 7), "17 00 a0 02 00 08 13 01 00 02 00 00 07");
    NEW(vdp_adv_call_if_args(2, 34, cond, 8), "17 00 a0 02 00 06 22 01 00 00 00 01 00 03 00");
    NEW(vdp_adv_jump_if_args(2, 0, cond2, 4), "17 00 a0 02 00 08 00 01 00 00 00");
    NEW(vdp_adv_call_offset_if(2, 1, 0, 1, 0, 0), "17 00 a0 02 00 0c 01 00 00 00 01 00 00 00");
    NEW(vdp_adv_jump_offset_if(2, 2, 2, 1, 3, 5), "17 00 a0 02 00 0a 02 00 00 02 01 00 03 00 05");

    NEW(vdp_adv_affine_identity(130), "17 00 a0 82 00 20 00");
    NEW(vdp_adv_affine_float(130, 6, 2, xy), "17 00 a0 82 00 20 06 00 00 00 80 3f 00 00 00 40");
    NEW(vdp_adv_affine_float(130, 70, 2, xy), "17 00 a0 82 00 20 46 00 00 00 80 3f 00 00 00 00 40");
    NEW(vdp_adv_affine_invert(130), "17 00 a0 82 00 20 01");
    NEW(vdp_adv_affine_args(130, 2, rot, 3), "17 00 a0 82 00 20 02 c0 5a 00");
    NEW(vdp_adv_affine_3d_identity(131), "17 00 a0 83 00 21 00");
    NEW(vdp_adv_affine_3d_float(131, 5, 3, xyz),
        "17 00 a0 83 00 21 05 00 00 00 80 3f 00 00 00 40 00 00 00 3f");
    NEW(vdp_adv_affine_3d_invert(131), "17 00 a0 83 00 21 01");
    NEW(vdp_adv_affine_3d_args(131, 2, rot3, 7), "17 00 a0 83 00 21 02 c0 5a 00 00 00 2d 00");

    NEW(vdp_adv_matrix_float(120, 3, 2, 2, 2, xy), "17 00 a0 78 00 22 03 02 02 00 00 00 80 3f 00 00 00 40");
    NEW(vdp_adv_matrix_combine(121, 4, 2, 2, 120, 120), "17 00 a0 79 00 22 04 02 02 78 00 78 00");
    NEW(vdp_adv_matrix_args(122, 7, 2, 2, smul, 5), "17 00 a0 7a 00 22 07 02 02 78 00 c0 03 00");

    NEW(vdp_adv_transform_bitmap(110, 0, 80, 2, 0, 0), "17 00 a0 6e 00 28 00 50 00 02 00");
    NEW(vdp_adv_transform_bitmap(110, 3, 80, 2, 16, 8), "17 00 a0 6e 00 28 03 50 00 02 00 10 00 08 00");
    NEW(vdp_adv_transform_data(111, 15, 192, 81, 4660, 2, 0, 4, 1),
        "17 00 a0 6f 00 29 0f c0 51 00 34 12 02 00 00 04 00 01 00");
    NEW(vdp_adv_transform_data(111, 18, 0, 81, 4660, 0, 65540, 0, 0),
        "17 00 a0 6f 00 29 12 00 51 00 34 12 04 00 01");
    NEW(vdp_adv_transform_data_args(111, 34, 192, 81, 4660, targs, 4),
        "17 00 a0 6f 00 29 22 c0 51 00 34 12 01 00 00 00");

    NEW(vdp_adv_read_variable(1, 0, 4, 1, 0), "17 00 a0 01 00 30 00 04 00 01 00");
    NEW(vdp_adv_read_variable(1, 192, 6, 512, 4660), "17 00 a0 01 00 30 c0 06 00 00 02 34 12");
    NEW(vdp_adv_read_variable(1, 80, 8, 2, 7), "17 00 a0 01 00 30 50 08 00 00 02 00 07");

    NEW(vdp_adv_expand_bitmap(100, 1, 2, 0, map2), "17 00 a0 64 00 48 01 02 00 00 ff");
    NEW(vdp_adv_expand_bitmap(100, 25, 2, 8, map2), "17 00 a0 64 00 48 09 02 00 08 00 00 ff");
    NEW(vdp_adv_expand_bitmap(100, 3, 2, 0, map8), "17 00 a0 64 00 48 03 02 00 00 01 02 03 04 05 06 07");
    NEW(vdp_adv_expand_bitmap_mapped(101, 1, 2, 0, 1), "17 00 a0 65 00 48 11 02 00 01 00");
    NEW(vdp_adv_expand_bitmap_mapped(101, 9, 2, 8, 1), "17 00 a0 65 00 48 19 02 00 08 00 01 00");

    NEW(vdp_adv_add_callback(2, 1), "17 00 a0 02 00 50 01 00");
    NEW(vdp_adv_remove_callback(2, 1), "17 00 a0 02 00 51 01 00");
    NEW(vdp_adv_add_callback(4660, 1), "17 00 a0 34 12 50 01 00");
    NEW(vdp_adv_remove_callback(4660, 65535), "17 00 a0 34 12 51 ff ff");
    NEW(vdp_adv_remove_callback(65535, 1), "17 00 a0 ff ff 51 01 00");
    /* Not played: VDP 2.16.0 crashes on it once any callback has been
     * added. Type 65535 has bufferRemoveCallback walk the map of types,
     * and buffer 65535 has it erase each type from that map as it goes. */
    NEW_UNREPLAYED(vdp_adv_remove_callback(65535, 65535), "17 00 a0 ff ff 51 ff ff");
    NEW(vdp_adv_debug_info(1), "17 00 a0 01 00 80");

    done();
    return 0;
}
