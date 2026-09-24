/*
 * agon/vdp/audio.h -- part of <agon/vdp.h>, which is what a program includes.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Sound: VDU 23, 0, &85, channel, command, [arguments]. The first byte
 * after &85 is a channel, 0 to 31, or -- for the sample commands that say
 * "sample" -- a negative sample number, -1 for the sample kept in buffer
 * &FB00, -2 for &FB01 and so on. The VDP answers every command but the
 * sample debug dump with a status packet, which MOS keeps in the audio
 * system variables; nothing here waits for it.
 *
 * The names down to vdp_audio_set_waveform_parameter are libagon's; the
 * ones after are the rest of what VDP 2.16.0 takes, named the same way.
 */
#ifndef ACC_AGON_VDP_AUDIO_H
#define ACC_AGON_VDP_AUDIO_H

/* libagon's names for waveforms, sample formats and frequency envelope
 * control bits. VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE is 16, as the VDP
 * reads it: libagon has 8, which is the rate-follows bit, and a sample
 * created with it has the VDP wait for a rate libagon never sends. */
#define VDP_AUDIO_WAVEFORM_SQUARE                   0
#define VDP_AUDIO_WAVEFORM_TRIANGLE                 1
#define VDP_AUDIO_WAVEFORM_SAWTOOTH                 2
#define VDP_AUDIO_WAVEFORM_SINEWAVE                 3
#define VDP_AUDIO_WAVEFORM_NOISE                    4
#define VDP_AUDIO_WAVEFORM_VICNOISE                 5
#define VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED         0
#define VDP_AUDIO_SAMPLE_FORMAT_8BIT_UNSIGNED       1
#define VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_RATE_FOLLOWS 8
#define VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE     16
#define VDP_AUDIO_FREQ_ENVELOPE_CONTROL_REPEATS     1
#define VDP_AUDIO_FREQ_ENVELOPE_CONTROL_CUMULATIVE  2
#define VDP_AUDIO_FREQ_ENVELOPE_CONTROL_RESTRICT    4

/* The VDP's own names (video/agon.h), each with VDP_ in front. */
#define VDP_AUDIO_CHANNELS              3       /* enabled at boot */
#define VDP_AUDIO_MAX_CHANNELS          32
#define VDP_AUDIO_DEFAULT_SAMPLE_RATE   16384
#define VDP_AUDIO_DEFAULT_FREQUENCY     523     /* a tuneable sample's, C5 */
#define VDP_AUDIO_BUFFERED_SAMPLE_BASEID 0xFB00 /* sample -1's buffer */

/* Commands: the byte after the channel. */
#define VDP_AUDIO_CMD_PLAY              0
#define VDP_AUDIO_CMD_STATUS            1
#define VDP_AUDIO_CMD_VOLUME            2
#define VDP_AUDIO_CMD_FREQUENCY         3
#define VDP_AUDIO_CMD_WAVEFORM          4
#define VDP_AUDIO_CMD_SAMPLE            5
#define VDP_AUDIO_CMD_ENV_VOLUME        6
#define VDP_AUDIO_CMD_ENV_FREQUENCY     7
#define VDP_AUDIO_CMD_ENABLE            8
#define VDP_AUDIO_CMD_DISABLE           9
#define VDP_AUDIO_CMD_RESET             10
#define VDP_AUDIO_CMD_SEEK              11
#define VDP_AUDIO_CMD_DURATION          12
#define VDP_AUDIO_CMD_SAMPLERATE        13
#define VDP_AUDIO_CMD_SET_PARAM         14

/* Waveforms. A negative one is a sample number; 8 takes a buffer ID. */
#define VDP_AUDIO_WAVE_DEFAULT          0
#define VDP_AUDIO_WAVE_SQUARE           0
#define VDP_AUDIO_WAVE_TRIANGLE         1
#define VDP_AUDIO_WAVE_SAWTOOTH         2
#define VDP_AUDIO_WAVE_SINE             3
#define VDP_AUDIO_WAVE_NOISE            4
#define VDP_AUDIO_WAVE_VICNOISE         5
#define VDP_AUDIO_WAVE_SAMPLE           8

/* Command 5's sub-commands. */
#define VDP_AUDIO_SAMPLE_LOAD                   0
#define VDP_AUDIO_SAMPLE_CLEAR                  1
#define VDP_AUDIO_SAMPLE_FROM_BUFFER            2
#define VDP_AUDIO_SAMPLE_SET_FREQUENCY          3
#define VDP_AUDIO_SAMPLE_BUFFER_SET_FREQUENCY   4
#define VDP_AUDIO_SAMPLE_SET_REPEAT_START       5
#define VDP_AUDIO_SAMPLE_BUFFER_SET_REPEAT_START 6
#define VDP_AUDIO_SAMPLE_SET_REPEAT_LENGTH      7
#define VDP_AUDIO_SAMPLE_BUFFER_SET_REPEAT_LENGTH 8
#define VDP_AUDIO_SAMPLE_DEBUG_INFO             0x10

/* A sample's format: the data in the low three bits, and two flags. */
#define VDP_AUDIO_FORMAT_8BIT_SIGNED    0
#define VDP_AUDIO_FORMAT_8BIT_UNSIGNED  1
#define VDP_AUDIO_FORMAT_DATA_MASK      7
#define VDP_AUDIO_FORMAT_WITH_RATE      8       /* a 16-bit rate follows */
#define VDP_AUDIO_FORMAT_TUNEABLE       16

/* Envelope types, and the stepped frequency envelope's control bits. */
#define VDP_AUDIO_ENVELOPE_NONE             0
#define VDP_AUDIO_ENVELOPE_ADSR             1
#define VDP_AUDIO_ENVELOPE_MULTIPHASE_ADSR  2
#define VDP_AUDIO_FREQUENCY_ENVELOPE_STEPPED 1
#define VDP_AUDIO_FREQUENCY_REPEATS         0x01
#define VDP_AUDIO_FREQUENCY_CUMULATIVE      0x02
#define VDP_AUDIO_FREQUENCY_RESTRICT        0x04

/* Command 14's parameters. With the 16-bit bit set the value is a word,
 * otherwise a byte -- and a byte frequency changes only the low byte. */
#define VDP_AUDIO_PARAM_DUTY_CYCLE      0
#define VDP_AUDIO_PARAM_VOLUME          2
#define VDP_AUDIO_PARAM_FREQUENCY       3
#define VDP_AUDIO_PARAM_16BIT           0x80
#define VDP_AUDIO_PARAM_MASK            0x0F

/* The status command's answer. */
#define VDP_AUDIO_STATUS_ACTIVE                 0x01
#define VDP_AUDIO_STATUS_PLAYING                0x02
#define VDP_AUDIO_STATUS_INDEFINITE             0x04
#define VDP_AUDIO_STATUS_HAS_VOLUME_ENVELOPE    0x08
#define VDP_AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE 0x10

/* Channels: play, status, volume, frequency, waveform. */
void vdp_audio_play_note(int channel, int volume, int frequency, int duration);
void vdp_audio_play_sample(int channel, int volume);
void vdp_audio_status(int channel);
void vdp_audio_set_volume(int channel, int volume);
void vdp_audio_set_frequency(int channel, int frequency);
void vdp_audio_set_waveform(int channel, int waveform);
void vdp_audio_set_sample(int channel, int bufferID);

/* Samples, by sample number or by buffer ID. load_sample sends the data
 * after the command; envelopeData below is [level, duration] pairs for the
 * multi-phase envelope, attack then sustain then release, and [adjustment,
 * steps] pairs for the stepped one. */
void vdp_audio_load_sample(int sample, int length, uint8_t *data);
void vdp_audio_clear_sample(int sample);
void vdp_audio_create_sample_from_buffer(int channel, int bufferID, int format);
void vdp_audio_set_sample_frequency(int sample, int frequency);
void vdp_audio_set_buffer_frequency(int channel, int bufferID, int frequency);
void vdp_audio_set_sample_repeat_start(int sample, int repeatStart);
void vdp_audio_set_buffer_repeat_start(int channel, int bufferID, int repeatStart);
void vdp_audio_set_sample_repeat_length(int sample, int repeatLength);
void vdp_audio_set_buffer_repeat_length(int channel, int bufferID, int repeatLength);

/* Envelopes. */
void vdp_audio_volume_envelope_disable(int channel);
void vdp_audio_volume_envelope_ADSR(int channel, int attack, int decay, int sustain,
                                    int release);
void vdp_audio_volume_envelope_multiphase_ADSR(uint8_t channel, uint8_t numAttack,
                                               uint8_t numSustain, uint8_t numRelease,
                                               int16_t *envelopeData);
void vdp_audio_frequency_envelope_disable(int channel);
void vdp_audio_frequency_envelope_stepped(int channel, int phaseCount, int controlByte,
                                          int stepLength, int16_t *envelopeData);

/* The channel itself. */
void vdp_audio_enable_channel(int channel);
void vdp_audio_disable_channel(int channel);
void vdp_audio_reset_channel(int channel);
void vdp_audio_sample_seek(int channel, int position);
void vdp_audio_sample_duration(int channel, int duration);
void vdp_audio_sample_rate(int channel, int rate);
void vdp_audio_set_waveform_parameter(int channel, int parameter, int value);

/* Past libagon. */

/* Channel 255 is the sound system as a whole: its volume, 0 to 127 (255
 * only asks for it), and its sample rate (65535 for the default). */
void vdp_audio_set_system_volume(int volume);
void vdp_audio_system_sample_rate(int rate);

/* Command 5, 0 without the data: the caller sends the length bytes it
 * announces itself, straight after, in as many pieces as it likes. */
void vdp_audio_load_sample_header(int sample, int length);

/* Command 5, 2 with the sample rate after the format; the rate-follows bit
 * is set in the format here. */
void vdp_audio_create_sample_from_buffer_rate(int channel, int bufferID, int format,
                                              int rate);

/* Command 5, &10: the VDP writes what it knows of a sample to its debug
 * log. The channel is not used and there is no answer. */
void vdp_audio_sample_debug_info(int channel, int bufferID);

#endif
