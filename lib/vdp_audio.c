/*
 * <agon/vdp/audio.h>'s calls.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Every one is VDU 23, 0, &85, channel, command and the command's
 * arguments, as video/vdu_audio.h in agon-vdp 2.16.0 reads them. The
 * libagon calls send what libagon sends, byte for byte, except where
 * libagon sends what the VDP does not take; each of those says so.
 */
#include <agon/vdp.h>

#include "vdp_emit.h"

#define AUDIO(channel, cmd)  23, 0, 0x85, LO(channel), (cmd)

/* The most RST 18h sends in one go: MOS counts down BC and stops when its
 * low 16 bits are zero, so a count of 0 -- or of 65536 -- sends up to a
 * delimiter instead. Data is sent in pieces no larger than this, and a
 * piece of none is not sent at all. */
#define MOS_PUTS_MAX 0xffff

static void send_data(const uint8_t *data, int length)
{
    while (length > MOS_PUTS_MAX) {
        SEND_BYTES(data, MOS_PUTS_MAX);
        data += MOS_PUTS_MAX;
        length -= MOS_PUTS_MAX;
    }
    if (length > 0)
        SEND_BYTES(data, length);
}

/* ------------------------------------------------------------------ */
/* channels                                                            */

void vdp_audio_play_note(int channel, int volume, int frequency, int duration)
{
    SEND(AUDIO(channel, 0), LO(volume), W(frequency), W(duration));
}

/* A note of no frequency and no duration: a channel set to a sample plays
 * the whole of it. */
void vdp_audio_play_sample(int channel, int volume)
{
    SEND(AUDIO(channel, 0), LO(volume), W(0), W(0));
}

/* The VDP answers with a status packet; this does not wait for it. */
void vdp_audio_status(int channel)
{
    SEND(AUDIO(channel, 1));
}

void vdp_audio_set_volume(int channel, int volume)
{
    SEND(AUDIO(channel, 2), LO(volume));
}

void vdp_audio_set_frequency(int channel, int frequency)
{
    SEND(AUDIO(channel, 3), W(frequency));
}

/* A negative waveform is a sample number. Waveform 8 takes a buffer ID
 * after it, which only vdp_audio_set_sample sends: as libagon does, this
 * sends the one byte whatever the waveform is. */
void vdp_audio_set_waveform(int channel, int waveform)
{
    SEND(AUDIO(channel, 4), LO(waveform));
}

void vdp_audio_set_sample(int channel, int bufferID)
{
    SEND(AUDIO(channel, 4), VDP_AUDIO_WAVE_SAMPLE, W(bufferID));
}

/* ------------------------------------------------------------------ */
/* samples                                                             */

/* The length is 24 bits, and that many bytes of data follow. libagon ends
 * with a call to mos_puts for whatever is left after its 65535-byte pieces,
 * even when that is nothing -- and a count of 0 has MOS send on up to a zero
 * byte, past the end of the data, which the VDP then reads as commands. */
void vdp_audio_load_sample(int sample, int length, uint8_t *data)
{
    vdp_audio_load_sample_header(sample, length);
    send_data(data, length);
}

/* libagon sends 8 bytes for this 6-byte command: the two after it are
 * whatever lies past its template in memory. */
void vdp_audio_clear_sample(int sample)
{
    SEND(AUDIO(sample, 5), 1);
}

/* With VDP_AUDIO_FORMAT_WITH_RATE in the format the VDP waits for a rate
 * this does not send, as libagon's does not: that is what
 * vdp_audio_create_sample_from_buffer_rate is for. */
void vdp_audio_create_sample_from_buffer(int channel, int bufferID, int format)
{
    SEND(AUDIO(channel, 5), 2, W(bufferID), LO(format));
}

void vdp_audio_set_sample_frequency(int sample, int frequency)
{
    SEND(AUDIO(sample, 5), 3, W(frequency));
}

void vdp_audio_set_buffer_frequency(int channel, int bufferID, int frequency)
{
    SEND(AUDIO(channel, 5), 4, W(bufferID), W(frequency));
}

void vdp_audio_set_sample_repeat_start(int sample, int repeatStart)
{
    SEND(AUDIO(sample, 5), 5, U24(repeatStart));
}

void vdp_audio_set_buffer_repeat_start(int channel, int bufferID, int repeatStart)
{
    SEND(AUDIO(channel, 5), 6, W(bufferID), U24(repeatStart));
}

/* A length of &FFFFFF (-1) repeats to the end of the sample. */
void vdp_audio_set_sample_repeat_length(int sample, int repeatLength)
{
    SEND(AUDIO(sample, 5), 7, U24(repeatLength));
}

void vdp_audio_set_buffer_repeat_length(int channel, int bufferID, int repeatLength)
{
    SEND(AUDIO(channel, 5), 8, W(bufferID), U24(repeatLength));
}

/* ------------------------------------------------------------------ */
/* envelopes                                                           */

/* The VDP reads an envelope's arguments only for a channel that is
 * enabled: sent to any other, they are taken for VDU commands. */

void vdp_audio_volume_envelope_disable(int channel)
{
    SEND(AUDIO(channel, 6), VDP_AUDIO_ENVELOPE_NONE);
}

void vdp_audio_volume_envelope_ADSR(int channel, int attack, int decay, int sustain,
                                    int release)
{
    SEND(AUDIO(channel, 6), VDP_AUDIO_ENVELOPE_ADSR, W(attack), W(decay), LO(sustain),
         W(release));
}

/* The phases of one part of the envelope: a count, then for each a level
 * byte -- the low byte of its entry -- and a 16-bit duration. */
static const int16_t *send_phases(uint8_t count, const int16_t *d)
{
    uint8_t i;

    SEND(count);
    for (i = 0; i < count; i++, d += 2)
        SEND(LO(d[0]), W(d[1]));

    return d;
}

/* libagon counts its way through envelopeData in a byte, so with more than
 * 127 phases in all it wraps back to the start of it. */
void vdp_audio_volume_envelope_multiphase_ADSR(uint8_t channel, uint8_t numAttack,
                                               uint8_t numSustain, uint8_t numRelease,
                                               int16_t *envelopeData)
{
    const int16_t *d = envelopeData;

    SEND(AUDIO(channel, 6), VDP_AUDIO_ENVELOPE_MULTIPHASE_ADSR);
    d = send_phases(numAttack, d);
    d = send_phases(numSustain, d);
    send_phases(numRelease, d);
}

void vdp_audio_frequency_envelope_disable(int channel)
{
    SEND(AUDIO(channel, 7), VDP_AUDIO_ENVELOPE_NONE);
}

/* Each phase is two 16-bit words, which lie in memory as the VDP reads
 * them. The VDP takes the phase count as a byte; libagon counts the words
 * in a byte as well, and so never finishes with 128 phases or more. */
void vdp_audio_frequency_envelope_stepped(int channel, int phaseCount, int controlByte,
                                          int stepLength, int16_t *envelopeData)
{
    SEND(AUDIO(channel, 7), VDP_AUDIO_FREQUENCY_ENVELOPE_STEPPED, LO(phaseCount),
         LO(controlByte), W(stepLength));
    send_data((const uint8_t *) envelopeData, LO(phaseCount) * 4);
}

/* ------------------------------------------------------------------ */
/* the channel itself                                                  */

void vdp_audio_enable_channel(int channel)
{
    SEND(AUDIO(channel, 8));
}

/* The VDP idles the channel rather than removing it: it stays enabled. */
void vdp_audio_disable_channel(int channel)
{
    SEND(AUDIO(channel, 9));
}

void vdp_audio_reset_channel(int channel)
{
    SEND(AUDIO(channel, 10));
}

void vdp_audio_sample_seek(int channel, int position)
{
    SEND(AUDIO(channel, 11), U24(position));
}

void vdp_audio_sample_duration(int channel, int duration)
{
    SEND(AUDIO(channel, 12), U24(duration));
}

void vdp_audio_sample_rate(int channel, int rate)
{
    SEND(AUDIO(channel, 13), W(rate));
}

/* The parameter's top bit says whether the value is a word or a byte. */
void vdp_audio_set_waveform_parameter(int channel, int parameter, int value)
{
    if (parameter & VDP_AUDIO_PARAM_16BIT)
        SEND(AUDIO(channel, 14), LO(parameter), W(value));
    else
        SEND(AUDIO(channel, 14), LO(parameter), LO(value));
}

/* ------------------------------------------------------------------ */
/* past libagon                                                        */

void vdp_audio_set_system_volume(int volume)
{
    vdp_audio_set_volume(255, volume);
}

void vdp_audio_system_sample_rate(int rate)
{
    vdp_audio_sample_rate(255, rate);
}

void vdp_audio_load_sample_header(int sample, int length)
{
    SEND(AUDIO(sample, 5), 0, U24(length));
}

void vdp_audio_create_sample_from_buffer_rate(int channel, int bufferID, int format,
                                              int rate)
{
    SEND(AUDIO(channel, 5), 2, W(bufferID), LO(format | VDP_AUDIO_FORMAT_WITH_RATE),
         W(rate));
}

void vdp_audio_sample_debug_info(int channel, int bufferID)
{
    SEND(AUDIO(channel, 5), VDP_AUDIO_SAMPLE_DEBUG_INFO, W(bufferID));
}
