/*
 * agon/vdp/buffer.h -- part of <agon/vdp.h>, which is what a program includes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The buffered commands: VDU 23, 0, &A0, bufferId; command, [arguments].
 * A buffer is a list of blocks of bytes the VDP keeps under a 16-bit ID; it
 * can hold VDU commands to call later, bitmap or sample data, or a matrix.
 *
 * The first part is libagon's, name for name. The rest is every other form
 * VDP 2.16.0 takes, named the way libagon names its neighbours. Many of the
 * commands take arguments whose layout depends on flags in their first
 * argument byte; for each of those there is a function that takes the flag
 * byte and the rest of the arguments as bytes (the _args ones), alongside
 * typed functions for the common forms, and vdp_adv_command sends any
 * command at all.
 *
 * An "advanced" offset, which the jump and call offsets always are and the
 * others are when their ADVANCED_OFFSETS flag is set, is 24 bits; when its
 * top bit (VDP_OFFSET_BLOCK) is set a 16-bit block number follows it and the
 * offset counts from the start of that block.
 */
#pragma once
#ifndef ACC_AGON_VDP_BUFFER_H
#define ACC_AGON_VDP_BUFFER_H

/* The commands: the byte after bufferId;. */
#define VDP_BUFFERED_WRITE                  0x00
#define VDP_BUFFERED_CALL                   0x01
#define VDP_BUFFERED_CLEAR                  0x02
#define VDP_BUFFERED_CREATE                 0x03
#define VDP_BUFFERED_SET_OUTPUT             0x04
#define VDP_BUFFERED_ADJUST                 0x05
#define VDP_BUFFERED_COND_CALL              0x06
#define VDP_BUFFERED_JUMP                   0x07
#define VDP_BUFFERED_COND_JUMP              0x08
#define VDP_BUFFERED_OFFSET_JUMP            0x09
#define VDP_BUFFERED_OFFSET_COND_JUMP       0x0a
#define VDP_BUFFERED_OFFSET_CALL            0x0b
#define VDP_BUFFERED_OFFSET_COND_CALL       0x0c
#define VDP_BUFFERED_COPY                   0x0d
#define VDP_BUFFERED_CONSOLIDATE            0x0e
#define VDP_BUFFERED_SPLIT                  0x0f
#define VDP_BUFFERED_SPLIT_INTO             0x10
#define VDP_BUFFERED_SPLIT_FROM             0x11
#define VDP_BUFFERED_SPLIT_BY               0x12
#define VDP_BUFFERED_SPLIT_BY_INTO          0x13
#define VDP_BUFFERED_SPLIT_BY_FROM          0x14
#define VDP_BUFFERED_SPREAD_INTO            0x15
#define VDP_BUFFERED_SPREAD_FROM            0x16
#define VDP_BUFFERED_REVERSE_BLOCKS         0x17
#define VDP_BUFFERED_REVERSE                0x18
#define VDP_BUFFERED_COPY_REF               0x19
#define VDP_BUFFERED_COPY_AND_CONSOLIDATE   0x1a
#define VDP_BUFFERED_AFFINE_TRANSFORM       0x20
#define VDP_BUFFERED_AFFINE_TRANSFORM_3D    0x21
#define VDP_BUFFERED_MATRIX                 0x22
#define VDP_BUFFERED_TRANSFORM_BITMAP       0x28
#define VDP_BUFFERED_TRANSFORM_DATA         0x29
#define VDP_BUFFERED_READ_VARIABLE          0x30
#define VDP_BUFFERED_COMPRESS               0x40
#define VDP_BUFFERED_DECOMPRESS             0x41
#define VDP_BUFFERED_EXPAND_BITMAP          0x48
#define VDP_BUFFERED_ADD_CALLBACK           0x50
#define VDP_BUFFERED_REMOVE_CALLBACK        0x51
#define VDP_BUFFERED_DEBUG_INFO             0x80

/* The top bit of an advanced offset: a block number follows it. */
#define VDP_OFFSET_BLOCK                    0x800000

/* Adjust (command 5): an operation in the low four bits, flags above. */
#define VDP_ADJUST_NOT                      0x00
#define VDP_ADJUST_NEG                      0x01
#define VDP_ADJUST_SET                      0x02
#define VDP_ADJUST_ADD                      0x03
#define VDP_ADJUST_ADD_CARRY                0x04
#define VDP_ADJUST_AND                      0x05
#define VDP_ADJUST_OR                       0x06
#define VDP_ADJUST_XOR                      0x07
#define VDP_ADJUST_OP_MASK                  0x0f
#define VDP_ADJUST_ADVANCED_OFFSETS         0x10
#define VDP_ADJUST_BUFFER_VALUE             0x20
#define VDP_ADJUST_MULTI_TARGET             0x40
#define VDP_ADJUST_MULTI_OPERAND            0x80

/* The conditions of commands 6, 8, &0A and &0C. */
#define VDP_COND_EXISTS                     0x00
#define VDP_COND_NOT_EXISTS                 0x01
#define VDP_COND_EQUAL                      0x02
#define VDP_COND_NOT_EQUAL                  0x03
#define VDP_COND_LESS                       0x04
#define VDP_COND_GREATER                    0x05
#define VDP_COND_LESS_EQUAL                 0x06
#define VDP_COND_GREATER_EQUAL              0x07
#define VDP_COND_AND                        0x08
#define VDP_COND_OR                         0x09
#define VDP_COND_OP_MASK                    0x0f
#define VDP_COND_ADVANCED_OFFSETS           0x10
#define VDP_COND_BUFFER_VALUE               0x20
#define VDP_COND_VAR_VALUE                  0x40
#define VDP_COND_16BIT                      0x80

/* Reverse (command &18) options. */
#define VDP_REVERSE_16BIT                   0x01
#define VDP_REVERSE_32BIT                   0x02
#define VDP_REVERSE_SIZE                    0x03
#define VDP_REVERSE_CHUNKED                 0x04
#define VDP_REVERSE_BLOCK                   0x08
#define VDP_REVERSE_UNUSED_BITS             0xf0

/* Affine transforms (commands &20 and &21). */
#define VDP_AFFINE_IDENTITY                 0
#define VDP_AFFINE_INVERT                   1
#define VDP_AFFINE_ROTATE                   2
#define VDP_AFFINE_ROTATE_RAD               3
#define VDP_AFFINE_MULTIPLY                 4
#define VDP_AFFINE_SCALE                    5
#define VDP_AFFINE_TRANSLATE                6
#define VDP_AFFINE_TRANSLATE_OS_COORDS      7
#define VDP_AFFINE_SHEAR                    8
#define VDP_AFFINE_SKEW                     9
#define VDP_AFFINE_SKEW_RAD                 10
#define VDP_AFFINE_TRANSFORM                11
#define VDP_AFFINE_TRANSLATE_BITMAP         12
#define VDP_AFFINE_OP_MASK                  0x0f
#define VDP_AFFINE_OP_ADVANCED_OFFSETS      0x10
#define VDP_AFFINE_OP_BUFFER_VALUE          0x20
#define VDP_AFFINE_OP_MULTI_FORMAT          0x40

/* The format byte before a run of number arguments: a 32-bit float when
 * zero; otherwise 16-bit and/or fixed point, the low five bits a signed
 * shift of the binary point. */
#define VDP_FLOAT_FORMAT_SHIFT_MASK         0x1f
#define VDP_FLOAT_FORMAT_SHIFT_TOPBIT       0x10
#define VDP_FLOAT_FORMAT_FLAGS              0xe0
#define VDP_FLOAT_FORMAT_FIXED              0x40
#define VDP_FLOAT_FORMAT_16BIT              0x80

/* Matrices (command &22). */
#define VDP_MATRIX_SET                      0
#define VDP_MATRIX_SET_VALUE                1
#define VDP_MATRIX_FILL                     2
#define VDP_MATRIX_DIAGONAL                 3
#define VDP_MATRIX_ADD                      4
#define VDP_MATRIX_SUBTRACT                 5
#define VDP_MATRIX_MULTIPLY                 6
#define VDP_MATRIX_SCALAR_MULTIPLY          7
#define VDP_MATRIX_SUBMATRIX                8
#define VDP_MATRIX_INSERT_ROW               9
#define VDP_MATRIX_INSERT_COLUMN            10
#define VDP_MATRIX_DELETE_ROW               11
#define VDP_MATRIX_DELETE_COLUMN            12
#define VDP_MATRIX_OP_MASK                  0x0f
#define VDP_MATRIX_OP_ADVANCED_OFFSETS      0x10
#define VDP_MATRIX_OP_BUFFER_VALUE          0x20

/* Transform a bitmap (command &28). */
#define VDP_TRANSFORM_BITMAP_RESIZE         0x01
#define VDP_TRANSFORM_BITMAP_EXPLICIT_SIZE  0x02
#define VDP_TRANSFORM_BITMAP_TRANSLATE      0x04

/* Transform data (command &29). */
#define VDP_TRANSFORM_DATA_HAS_SIZE         0x01
#define VDP_TRANSFORM_DATA_HAS_OFFSET       0x02
#define VDP_TRANSFORM_DATA_HAS_STRIDE       0x04
#define VDP_TRANSFORM_DATA_HAS_LIMIT        0x08
#define VDP_TRANSFORM_DATA_ADVANCED         0x10
#define VDP_TRANSFORM_DATA_BUFFER_ARGS      0x20
#define VDP_TRANSFORM_DATA_PER_BLOCK        0x40

/* Read a VDP variable into a buffer (command &30). */
#define VDP_READ_VAR_BIG_ENDIAN             0x01
#define VDP_READ_VAR_ADVANCED_OFFSETS       0x10
#define VDP_READ_VAR_USE_DEFAULT            0x40
#define VDP_READ_VAR_16BIT                  0x80

/* Expand a bitmap (command &48): bits a pixel in the low three, 0 for 8. */
#define VDP_EXPAND_BITMAP_SIZE              0x07
#define VDP_EXPAND_BITMAP_ALIGNED           0x08
#define VDP_EXPAND_BITMAP_USEBUFFER         0x10

/* What a callback (commands &50 and &51) is called on. */
#define VDP_CALLBACK_VSYNC                  0
#define VDP_CALLBACK_MODE_CHANGE            1
#define VDP_CALLBACK_KEYBOARD               2
#define VDP_CALLBACK_MOUSE                  3
#define VDP_CALLBACK_PALETTE                4
#define VDP_CALLBACK_READPIXEL              5
#define VDP_CALLBACK_SENDING_VDPP           0x0100
#define VDP_CALLBACK_SENT_VDPP              0x0180

/* libagon's. */
void vdp_adv_write_block(int bufferID, int length);
void vdp_adv_write_block_data(int bufferID, int length, char *data);
void vdp_adv_call_buffer(int bufferID);
void vdp_adv_clear_buffer(int bufferID);
void vdp_adv_create(int bufferID, int length);
void vdp_adv_stream(int bufferID);
void vdp_adv_adjust(int bufferID, int operation, int offset);
void vdp_adv_call_conditional(int bufferId, int operation, int checkBufferId, int checkOffset);
void vdp_adv_jump_buffer(int bufferID);
void vdp_adv_jump_conditional(int bufferId, int operation, int checkBufferId, int checkOffset);
void vdp_adv_jump_offset(int bufferId, int offset);
void vdp_adv_jump_offset_block(int bufferId, int offset, int block);
void vdp_adv_jump_offset_conditional(int bufferId, int offset);
void vdp_adv_jump_offset_block_conditional(int bufferId, int offset, int block);
void vdp_adv_call_offset(int bufferId, int offset);
void vdp_adv_call_offset_block(int bufferId, int offset, int block);
void vdp_adv_call_offset_conditional(int bufferId, int offset);
void vdp_adv_call_offset_block_conditional(int bufferId, int offset, int block);
void vdp_adv_copy_multiple(int bufferId, int num_buffers, ...);
void vdp_adv_consolidate(int bufferID);
void vdp_adv_split(int bufferID, int blockSize);
void vdp_adv_split_multiple(int bufferId, int blockSize, int num_buffers, ...);
void vdp_adv_split_multiple_from(int bufferID, int blockSize, int targetBufferID);
void vdp_adv_split_by_width(int bufferID, int width, int blockCount);
void vdp_adv_split_by_width_multiple(int bufferId, int width, int num_buffers, ...);
void vdp_adv_split_by_width_multiple_from(int bufferID, int width, int blockCount, int targetBufferID);
void vdp_adv_spread_multiple(int bufferId, int num_buffers, ...);
void vdp_adv_spread_multiple_from(int bufferID, int targetBufferID);
void vdp_adv_reverse_block_order(int bufferID);
void vdp_adv_reverse_block_data(int bufferID, int options, int valueSize, int chunkSize);
void vdp_adv_copy_multiple_by_reference(int bufferId, int num_buffers, ...);
void vdp_adv_copy_multiple_consolidate(int bufferId, int num_buffers, ...);
void vdp_adv_create_rotated_buffer(uint16_t transformID, uint16_t angle);
void vdp_adv_create_scaled_buffer(uint16_t transformID, float scaleX, float scaleY);
void vdp_adv_create_translated_buffer(uint16_t transformID, int16_t x, int16_t y);
void vdp_adv_create_skewed_buffer(uint16_t transformID, float scaleX, float scaleY);
void vdp_adv_apply_transformation_with_stride(uint16_t sourceID, uint16_t destID, uint16_t transformID,
                                              uint16_t offset, uint16_t stride);
void vdp_adv_compress_buffer(int targetBufferID, int sourceBufferId);
void vdp_adv_decompress_buffer(int targetBufferID, int sourceBufferId);

/* Any command, with its arguments as bytes. */
void vdp_adv_command(int bufferId, int command, const void *args, int length);

/* Adjust. The typed forms send the offset (and count) in 16 or 24 bits as
 * the operation's VDP_ADJUST_ADVANCED_OFFSETS flag says, and an operand only
 * for the operations that take one (SET and above). */
void vdp_adv_adjust_args(int bufferId, int operation, const void *args, int length);
void vdp_adv_adjust_value(int bufferId, int operation, int offset, int value);
void vdp_adv_adjust_multi(int bufferId, int operation, int offset, int count, int value);
void vdp_adv_adjust_multi_data(int bufferId, int operation, int offset, int count, const void *operands);
void vdp_adv_adjust_buffer(int bufferId, int operation, int offset, int operandBufferId, int operandOffset);

/* Conditional call and jump, whole. The typed forms send the check offset
 * in 16 or 24 bits as VDP_COND_ADVANCED_OFFSETS says, none for a
 * VDP_COND_VAR_VALUE check, and an operand -- a byte, or 16 bits with
 * VDP_COND_16BIT -- for the conditions that take one. An operand from a
 * buffer, or an offset with a block number, goes through the _args forms. */
void vdp_adv_call_if(int bufferId, int operation, int checkBufferId, int checkOffset, int operand);
void vdp_adv_jump_if(int bufferId, int operation, int checkBufferId, int checkOffset, int operand);
void vdp_adv_call_offset_if(int bufferId, int offset, int operation, int checkBufferId, int checkOffset,
                            int operand);
void vdp_adv_jump_offset_if(int bufferId, int offset, int operation, int checkBufferId, int checkOffset,
                            int operand);
void vdp_adv_call_if_args(int bufferId, int operation, const void *args, int length);
void vdp_adv_jump_if_args(int bufferId, int operation, const void *args, int length);

/* Affine transforms, 2d and 3d. The _float forms send their values as
 * 32-bit floats; the _args forms take the format bytes and values as they
 * are to be sent. */
void vdp_adv_affine_identity(int bufferId);
void vdp_adv_affine_invert(int bufferId);
void vdp_adv_affine_float(int bufferId, int operation, int count, const float *values);
void vdp_adv_affine_args(int bufferId, int operation, const void *args, int length);
void vdp_adv_affine_3d_identity(int bufferId);
void vdp_adv_affine_3d_invert(int bufferId);
void vdp_adv_affine_3d_float(int bufferId, int operation, int count, const float *values);
void vdp_adv_affine_3d_args(int bufferId, int operation, const void *args, int length);

/* Matrices of any size. */
void vdp_adv_matrix_float(int bufferId, int operation, int rows, int columns, int count, const float *values);
void vdp_adv_matrix_combine(int bufferId, int operation, int rows, int columns, int sourceId1, int sourceId2);
void vdp_adv_matrix_args(int bufferId, int operation, int rows, int columns, const void *args, int length);

/* Transforming a bitmap, and data. The width and height, and each of the
 * data transform's size, offset, stride and limit, are sent only when the
 * options say they are there. */
void vdp_adv_transform_bitmap(int bufferId, int options, int transformBufferId, int bitmapId,
                              int width, int height);
void vdp_adv_transform_data(int bufferId, int options, int format, int transformBufferId,
                            int sourceBufferId, int size, int offset, int stride, int limit);
void vdp_adv_transform_data_args(int bufferId, int options, int format, int transformBufferId,
                                 int sourceBufferId, const void *args, int length);

/* The rest. */
void vdp_adv_read_variable(int bufferId, int options, int offset, int variableId, int defaultValue);
void vdp_adv_expand_bitmap(int bufferId, int options, int sourceBufferId, int width, const void *map);
void vdp_adv_expand_bitmap_mapped(int bufferId, int options, int sourceBufferId, int width, int mapBufferId);
void vdp_adv_add_callback(int bufferId, int type);
void vdp_adv_remove_callback(int bufferId, int type);
void vdp_adv_debug_info(int bufferId);

#endif
