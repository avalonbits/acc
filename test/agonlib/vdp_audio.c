/* <agon/vdp/audio.h> against libagon: see capture.h.
 *
 * The order matters to the replay into the VDP (test/vdpsync.sh): a sample
 * is loaded before anything names it, a cleared sample is never named
 * again -- the VDP keeps an empty entry for it and would follow it -- and
 * the envelopes go to channels 0 to 2, which the VDP enables at boot and
 * which are the only ones it reads an envelope's arguments for. */
#include "capture.h"
#include <agon/vdp.h>

static uint8_t wave[] = { 0x10, 0x80, 0xf0, 0 };
static uint8_t longwave[260];

/* Attack 2 phases, sustain 1, release 1: [level, duration] each. */
static int16_t phases[] = { 255, 10, 300, 300, 100, -1, 0, 1000 };

/* [adjustment, steps] for 2 phases. */
static int16_t steps[] = { -10, 5, 300, -1 };

int main(void)
{
    int i;

    for (i = 0; i < (int) sizeof longwave; i++)
        longwave[i] = (uint8_t) (i * 7 + 1);

    CALL(vdp_audio_play_note(0, 127, 440, 1000));
    CALL(vdp_audio_play_note(1, 200, 65535, 65535));
    CALL(vdp_audio_play_note(2, 0, 255, 256));
    CALL(vdp_audio_play_sample(1, 64));
    CALL(vdp_audio_set_volume(0, 100));
    CALL(vdp_audio_set_volume(2, 255));
    CALL(vdp_audio_set_frequency(0, 65535));
    CALL(vdp_audio_set_frequency(1, 300));
    CALL(vdp_audio_set_waveform(0, VDP_AUDIO_WAVEFORM_SINEWAVE));
    CALL(vdp_audio_set_waveform(1, VDP_AUDIO_WAVEFORM_VICNOISE));

    CALL(vdp_audio_load_sample(-1, 3, wave));
    CALL(vdp_audio_load_sample(-2, 100, longwave));

    /* A length past 255, so all three of its bytes count. */
    CALL(vdp_audio_load_sample(-5, 260, longwave));
    CALL(vdp_audio_set_sample_frequency(-1, 523));
    CALL(vdp_audio_set_sample_frequency(-2, 65535));
    CALL(vdp_audio_set_sample_repeat_start(-1, 70000));
    CALL(vdp_audio_set_sample_repeat_start(-2, 1));
    CALL(vdp_audio_set_sample_repeat_length(-1, -1));
    CALL(vdp_audio_set_sample_repeat_length(-2, 256));
    CALL(vdp_audio_set_waveform(2, -1));
    CALL(vdp_audio_create_sample_from_buffer(0, 0xFB00, VDP_AUDIO_SAMPLE_FORMAT_8BIT_UNSIGNED));
    CALL(vdp_audio_create_sample_from_buffer(1, 0xFB01, VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED));
    CALL(vdp_audio_set_buffer_frequency(0, 0xFB00, 1000));
    CALL(vdp_audio_set_buffer_repeat_start(1, 0xFB00, 0x12345));
    CALL(vdp_audio_set_buffer_repeat_length(1, 0xFB01, 65536));
    CALL(vdp_audio_set_sample(1, 0xFB00));
    CALL(vdp_audio_sample_seek(1, 0x10203));
    CALL(vdp_audio_sample_duration(1, 70000));
    CALL(vdp_audio_sample_rate(1, 8000));

    /* libagon's VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE is 8, the VDP's
     * rate-follows bit, so its build sends a format of 08 and the VDP then
     * waits for a rate; the VDP's tuneable bit is 16. */
#ifndef AGONDEV
    CALL(vdp_audio_create_sample_from_buffer(0, 0xFB00, VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE));
#else
    expect("vdp_audio_create_sample_from_buffer(0, 0xFB00, VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE)",
           "17 00 85 00 05 02 00 fb 10");
#endif

    CALL(vdp_audio_volume_envelope_ADSR(0, 10, 300, 100, 65535));
    CALL(vdp_audio_volume_envelope_multiphase_ADSR(1, 2, 1, 1, phases));
    CALL(vdp_audio_volume_envelope_disable(0));
    CALL(vdp_audio_frequency_envelope_stepped(0, 2, VDP_AUDIO_FREQ_ENVELOPE_CONTROL_REPEATS | VDP_AUDIO_FREQ_ENVELOPE_CONTROL_CUMULATIVE, 20, steps));
    CALL(vdp_audio_frequency_envelope_stepped(2, 1, VDP_AUDIO_FREQ_ENVELOPE_CONTROL_RESTRICT, 256, steps));
    CALL(vdp_audio_frequency_envelope_disable(0));
    CALL(vdp_audio_set_waveform_parameter(0, 0, 128));
    CALL(vdp_audio_set_waveform_parameter(0, 0x83, 440));
    CALL(vdp_audio_set_waveform_parameter(0, 3, 300));

    CALL(vdp_audio_enable_channel(5));
    CALL(vdp_audio_disable_channel(5));
    CALL(vdp_audio_reset_channel(5));

    NEW(vdp_audio_set_system_volume(100), "17 00 85 ff 02 64");
    NEW(vdp_audio_system_sample_rate(16384), "17 00 85 ff 0d 00 40");
    NEW((vdp_audio_load_sample_header(-4, 3), mos_puts((char *) wave, 3, 0)),
        "17 00 85 fc 05 00 03 00 00 10 80 f0");
    NEW(vdp_audio_create_sample_from_buffer_rate(0, 0xFB03, 16, 8000),
        "17 00 85 00 05 02 03 fb 18 40 1f");
    NEW(vdp_audio_sample_debug_info(0, 0xFB00), "17 00 85 00 05 10 00 fb");

    /* libagon sends 8 bytes for this 6-byte command: two from past its
     * template in memory. */
#ifndef AGONDEV
    CALL(vdp_audio_clear_sample(-2));
#else
    expect("vdp_audio_clear_sample(-2)", "17 00 85 fe 05 01");
#endif

    /* With nothing left to send, libagon still calls mos_puts, with a
     * count of 0, which sends up to a zero byte: here the 3 bytes of wave,
     * which the VDP takes for commands. */
#ifndef AGONDEV
    CALL(vdp_audio_load_sample(-3, 0, wave));
#else
    expect("vdp_audio_load_sample(-3, 0, wave)", "17 00 85 fd 05 00 00 00 00");
#endif

    done();
    return 0;
}
