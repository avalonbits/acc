/* <agon/vdp/audio.h>'s calls against the VDP: every audio command but the
 * sample debug dump is answered with a status packet, which MOS puts in
 * sys_vars->audioChannel and audioSuccess. Each check here waits for that
 * answer and writes it down. Only answers the VDP's source settles are
 * asked for -- nothing that hangs on how far the audio task has got with a
 * note -- and each line says what the source says it should be. */
#include "result.h"
#include <agon/vdp.h>

static uint8_t wave[] = { 0x10, 0x80, 0xf0 };
static int16_t steps[] = { 10, 5 };

static void answer(const char *what)
{
    volatile SYSVAR *v = sys_vars;

    while (!(v->vdp_pflags & vdp_pflag_audio))
        ;
    say("%s %d %d\n", what, v->audioChannel, v->audioSuccess);
}

/* The flags cleared first, so that the audio bit is this call's answer. */
#define ASK(what, call) do { sys_vars->vdp_pflags = 0; call; answer(what); } while (0)

int main(void)
{
    /* A channel at boot: square wave, silent, no envelopes; one past the
     * last has no status at all (255). Asking for the volume, 255, answers
     * the channel's own, which starts at 64. Channel 1, because channel 0
     * may still be sounding MOS's bell from the boot. */
    ASK("status", vdp_audio_status(1));
    ASK("status", vdp_audio_status(31));
    ASK("volume", vdp_audio_set_volume(1, 255));

    /* The whole system's volume, set and then asked for. */
    ASK("system volume", vdp_audio_set_system_volume(50));
    ASK("system volume", vdp_audio_set_system_volume(255));

    /* Envelopes answer 1, and show in the status: 8 for a volume envelope,
     * 16 for a frequency one. */
    ASK("adsr", vdp_audio_volume_envelope_ADSR(1, 10, 20, 64, 30));
    ASK("status", vdp_audio_status(1));
    ASK("stepped", vdp_audio_frequency_envelope_stepped(1, 1, 0, 10, steps));
    ASK("status", vdp_audio_status(1));
    ASK("no adsr", vdp_audio_volume_envelope_disable(1));
    ASK("no stepped", vdp_audio_frequency_envelope_disable(1));
    ASK("status", vdp_audio_status(1));

    /* Channel 3 is not enabled at boot; 40 is past the last there is. */
    ASK("enable", vdp_audio_enable_channel(3));
    ASK("status", vdp_audio_status(3));
    ASK("disable", vdp_audio_disable_channel(3));
    ASK("enable", vdp_audio_enable_channel(40));

    /* Samples: the answer's channel is the sample number, -1 as 255. The
     * ones that name a sample or a buffer that is not there answer 0. */
    ASK("load", vdp_audio_load_sample(-1, 3, wave));
    ASK("sample frequency", vdp_audio_set_sample_frequency(-1, 523));
    ASK("sample frequency", vdp_audio_set_sample_frequency(-9, 523));
    ASK("repeat start", vdp_audio_set_sample_repeat_start(-1, 1));
    ASK("repeat length", vdp_audio_set_sample_repeat_length(-1, -1));
    ASK("from buffer", vdp_audio_create_sample_from_buffer(0, 0xFB00,
                                                           VDP_AUDIO_FORMAT_8BIT_UNSIGNED));
    ASK("from buffer", vdp_audio_create_sample_from_buffer(0, 0x4321, 0));
    ASK("from buffer rate", vdp_audio_create_sample_from_buffer_rate(1, 0xFB00,
                                                                     VDP_AUDIO_FORMAT_TUNEABLE,
                                                                     8000));
    ASK("buffer frequency", vdp_audio_set_buffer_frequency(2, 0xFB00, 1000));
    ASK("buffer repeat start", vdp_audio_set_buffer_repeat_start(2, 0xFB00, 2));
    ASK("buffer repeat length", vdp_audio_set_buffer_repeat_length(2, 0xFB00, 1));
    ASK("header", (vdp_audio_load_sample_header(-2, 3), mos_puts((char *) wave, 3, 0)));
    ASK("set sample", vdp_audio_set_sample(1, 0xFB00));
    ASK("set sample", vdp_audio_set_sample(1, 0x4321));
    ASK("waveform", vdp_audio_set_waveform(2, -2));
    ASK("waveform", vdp_audio_set_waveform(2, VDP_AUDIO_WAVE_TRIANGLE));

    /* The duty cycle is a square wave's alone: channel 0 has one, channel 1
     * now plays a sample. */
    ASK("duty", vdp_audio_set_waveform_parameter(0, VDP_AUDIO_PARAM_DUTY_CYCLE, 128));
    ASK("duty", vdp_audio_set_waveform_parameter(1, VDP_AUDIO_PARAM_DUTY_CYCLE, 128));
    ASK("rate", vdp_audio_sample_rate(0, 8000));
    ASK("system rate", vdp_audio_system_sample_rate(65535));

    /* Clearing a sample answers 0 when there was one and 1 when there was
     * not -- the other way round from the rest. */
    ASK("clear", vdp_audio_clear_sample(-2));
    ASK("clear", vdp_audio_clear_sample(-20));

    /* A note: queued (1), then refused while it plays (0); silencing it
     * answers the volume set. Resetting an enabled channel answers 1. */
    ASK("play", vdp_audio_play_note(2, 100, 440, 65535));
    ASK("play", vdp_audio_play_note(2, 100, 440, 65535));
    ASK("silence", vdp_audio_set_volume(2, 0));
    ASK("reset", vdp_audio_reset_channel(2));

    return finish();
}
