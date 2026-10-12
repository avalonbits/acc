# `agon/vdp/audio.h`

Sound: VDU 23, 0, &85, channel, command, arguments. A program includes
[agon/vdp.h](agon-vdp.md), which includes this.

The byte after &85 is a channel, 0 to 31, or, for the commands on a
sample, a negative sample number: -1 is the sample kept in buffer
&FB00, -2 the one in &FB01, and so on. Channel 255 is the sound system
as a whole.

The VDP answers every command but the sample debug dump with a status
packet, which MOS keeps in the audio system variables; nothing here
waits for it.

The names down to `vdp_audio_set_waveform_parameter` are libagon's,
sending libagon's bytes except where libagon sends what VDP 2.16.0 does
not take; those places are said below. The ones after are the rest of
what VDP 2.16.0 takes.

In the sequences, a plain argument is one byte, its low byte when the
argument is wider; `x;` is two bytes, low first; `x(3)` is three. Every
sequence starts VDU 23, 0, &85, written `85` below.

## `VDP_AUDIO_CHANNELS`, `VDP_AUDIO_MAX_CHANNELS`, `VDP_AUDIO_DEFAULT_SAMPLE_RATE`, `VDP_AUDIO_DEFAULT_FREQUENCY`, `VDP_AUDIO_BUFFERED_SAMPLE_BASEID`

The VDP's own names, each with `VDP_` in front.

| Name                               | Value | Meaning                 |
|------------------------------------|-------|-------------------------|
| `VDP_AUDIO_CHANNELS`               | 3     | enabled at boot         |
| `VDP_AUDIO_MAX_CHANNELS`           | 32    | channels in all         |
| `VDP_AUDIO_DEFAULT_SAMPLE_RATE`    | 16384 | the default rate        |
| `VDP_AUDIO_DEFAULT_FREQUENCY`      | 523   | a tuneable sample's, C5 |
| `VDP_AUDIO_BUFFERED_SAMPLE_BASEID` | &FB00 | sample -1's buffer      |

## `VDP_AUDIO_CMD_PLAY`

The command byte, after the channel.

| Name                        | Value | Function                         |
|-----------------------------|-------|----------------------------------|
| `VDP_AUDIO_CMD_PLAY`        | 0     | `vdp_audio_play_note`            |
| `VDP_AUDIO_CMD_STATUS`      | 1     | `vdp_audio_status`               |
| `VDP_AUDIO_CMD_VOLUME`      | 2     | `vdp_audio_set_volume`           |
| `VDP_AUDIO_CMD_FREQUENCY`   | 3     | `vdp_audio_set_frequency`        |
| `VDP_AUDIO_CMD_WAVEFORM`    | 4     | `vdp_audio_set_waveform`         |
| `VDP_AUDIO_CMD_SAMPLE`      | 5     | the sample calls                 |
| `VDP_AUDIO_CMD_ENV_VOLUME`  | 6     | the volume envelopes             |
| `VDP_AUDIO_CMD_ENV_FREQUENCY` | 7   | the frequency envelopes          |
| `VDP_AUDIO_CMD_ENABLE`      | 8     | `vdp_audio_enable_channel`       |
| `VDP_AUDIO_CMD_DISABLE`     | 9     | `vdp_audio_disable_channel`      |
| `VDP_AUDIO_CMD_RESET`       | 10    | `vdp_audio_reset_channel`        |
| `VDP_AUDIO_CMD_SEEK`        | 11    | `vdp_audio_sample_seek`          |
| `VDP_AUDIO_CMD_DURATION`    | 12    | `vdp_audio_sample_duration`      |
| `VDP_AUDIO_CMD_SAMPLERATE`  | 13    | `vdp_audio_sample_rate`          |
| `VDP_AUDIO_CMD_SET_PARAM`   | 14    | `..._set_waveform_parameter`     |

## `vdp_audio_play_note`, `vdp_audio_play_sample`

    void vdp_audio_play_note(int channel, int volume, int frequency,
                             int duration);
    void vdp_audio_play_sample(int channel, int volume);

85 channel, 0, volume, frequency;, duration;. Play a note on the
channel. `vdp_audio_play_sample` is a note of frequency 0 and duration
0: a channel set to a sample plays the whole of it.

    vdp_audio_play_note(0, 100, 440, 500);

## `vdp_audio_status`, `VDP_AUDIO_STATUS_ACTIVE`

    void vdp_audio_status(int channel);

85 channel, 1. Ask for the channel's status. The VDP answers with a
status packet, which this does not wait for. Its bits:

| Name                                      | Value |
|-------------------------------------------|-------|
| `VDP_AUDIO_STATUS_ACTIVE`                 | &01   |
| `VDP_AUDIO_STATUS_PLAYING`                | &02   |
| `VDP_AUDIO_STATUS_INDEFINITE`             | &04   |
| `VDP_AUDIO_STATUS_HAS_VOLUME_ENVELOPE`    | &08   |
| `VDP_AUDIO_STATUS_HAS_FREQUENCY_ENVELOPE` | &10   |

## `vdp_audio_set_volume`, `vdp_audio_set_frequency`

    void vdp_audio_set_volume(int channel, int volume);
    void vdp_audio_set_frequency(int channel, int frequency);

85 channel, 2, volume and 85 channel, 3, frequency;.

## `vdp_audio_set_waveform`, `vdp_audio_set_sample`, `VDP_AUDIO_WAVE_SQUARE`, `VDP_AUDIO_WAVEFORM_SQUARE`

    void vdp_audio_set_waveform(int channel, int waveform);
    void vdp_audio_set_sample(int channel, int bufferID);

`vdp_audio_set_waveform` sends 85 channel, 4, waveform. A negative
waveform is a sample number. Waveform 8 takes a buffer ID after it,
which only `vdp_audio_set_sample` sends: 85 channel, 4, 8, bufferID;.
As libagon does, `vdp_audio_set_waveform` sends the one byte whatever
the waveform is.

| VDP's name                 | libagon's name                | Value |
|----------------------------|-------------------------------|-------|
| `VDP_AUDIO_WAVE_SQUARE`    | `VDP_AUDIO_WAVEFORM_SQUARE`   | 0     |
| `VDP_AUDIO_WAVE_TRIANGLE`  | `VDP_AUDIO_WAVEFORM_TRIANGLE` | 1     |
| `VDP_AUDIO_WAVE_SAWTOOTH`  | `VDP_AUDIO_WAVEFORM_SAWTOOTH` | 2     |
| `VDP_AUDIO_WAVE_SINE`      | `VDP_AUDIO_WAVEFORM_SINEWAVE` | 3     |
| `VDP_AUDIO_WAVE_NOISE`     | `VDP_AUDIO_WAVEFORM_NOISE`    | 4     |
| `VDP_AUDIO_WAVE_VICNOISE`  | `VDP_AUDIO_WAVEFORM_VICNOISE` | 5     |
| `VDP_AUDIO_WAVE_SAMPLE`    |                               | 8     |

`VDP_AUDIO_WAVE_DEFAULT` is 0, the square wave.

## `vdp_audio_load_sample`, `vdp_audio_load_sample_header`

    void vdp_audio_load_sample(int sample, int length, uint8_t *data);
    void vdp_audio_load_sample_header(int sample, int length);

85 sample, 5, 0, length(3), and then that many bytes of data.
`vdp_audio_load_sample` sends the `length` bytes at `data`, in pieces
of at most 65535 and none at all when there are none.
`vdp_audio_load_sample_header` sends the command without the data: the
caller sends the `length` bytes itself, straight after, in as many
pieces as it likes.

libagon ends with a call to MOS for whatever is left after its
65535-byte pieces, even when that is nothing, and a count of 0 has MOS
send on up to a zero byte, past the end of the data, which the VDP then
reads as commands.

    vdp_audio_load_sample(-1, len, pcm);
    vdp_audio_set_waveform(0, -1);
    vdp_audio_play_sample(0, 127);

## `vdp_audio_clear_sample`

    void vdp_audio_clear_sample(int sample);

85 sample, 5, 1. libagon sends 8 bytes for this 6-byte command: the
two after it are whatever lies past its template in memory.

## `vdp_audio_create_sample_from_buffer`, `vdp_audio_create_sample_from_buffer_rate`, `VDP_AUDIO_FORMAT_8BIT_SIGNED`

    void vdp_audio_create_sample_from_buffer(int channel, int bufferID,
                                             int format);
    void vdp_audio_create_sample_from_buffer_rate(int channel,
                                                  int bufferID,
                                                  int format,
                                                  int rate);

85 channel, 5, 2, bufferID;, format. Make the buffer a sample.
`vdp_audio_create_sample_from_buffer_rate` sets
`VDP_AUDIO_FORMAT_WITH_RATE` in the format and sends rate; after it.
With that bit in the format, the plain call sends no rate, as libagon's
does not, and the VDP waits for one.

A format is the data in the low three bits, and two flags:

| Name                             | Value | Meaning                   |
|----------------------------------|-------|---------------------------|
| `VDP_AUDIO_FORMAT_8BIT_SIGNED`   | 0     | 8-bit signed              |
| `VDP_AUDIO_FORMAT_8BIT_UNSIGNED` | 1     | 8-bit unsigned            |
| `VDP_AUDIO_FORMAT_DATA_MASK`     | 7     | the data's bits           |
| `VDP_AUDIO_FORMAT_WITH_RATE`     | 8     | a 16-bit rate follows     |
| `VDP_AUDIO_FORMAT_TUNEABLE`      | 16    | tuneable                  |

## `VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED`, `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE`

libagon's names for the same formats:

| Name                                          | Value |
|-----------------------------------------------|-------|
| `VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED`         | 0     |
| `VDP_AUDIO_SAMPLE_FORMAT_8BIT_UNSIGNED`       | 1     |
| `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_RATE_FOLLOWS` | 8     |
| `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE`     | 16    |

`VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE` is 16, as the VDP reads it.
libagon has 8, which is the rate-follows bit, and a sample made with it
has the VDP wait for a rate libagon never sends.

## `vdp_audio_set_sample_frequency`, `vdp_audio_set_buffer_frequency`

    void vdp_audio_set_sample_frequency(int sample, int frequency);
    void vdp_audio_set_buffer_frequency(int channel, int bufferID,
                                        int frequency);

85 sample, 5, 3, frequency; and 85 channel, 5, 4, bufferID;,
frequency;. A sample's frequency, by sample number or by buffer ID.

## `vdp_audio_set_sample_repeat_start`, `vdp_audio_set_buffer_repeat_start`, `vdp_audio_set_sample_repeat_length`, `vdp_audio_set_buffer_repeat_length`

    void vdp_audio_set_sample_repeat_start(int sample, int repeatStart);
    void vdp_audio_set_buffer_repeat_start(int channel, int bufferID,
                                           int repeatStart);
    void vdp_audio_set_sample_repeat_length(int sample,
                                            int repeatLength);
    void vdp_audio_set_buffer_repeat_length(int channel, int bufferID,
                                            int repeatLength);

| Call                          | Sends (after 85)                      |
|-------------------------------|---------------------------------------|
| `..._sample_repeat_start`     | sample, 5, 5, repeatStart(3)          |
| `..._buffer_repeat_start`     | channel, 5, 6, bufferID;,             |
|                               | repeatStart(3)                        |
| `..._sample_repeat_length`    | sample, 5, 7, repeatLength(3)         |
| `..._buffer_repeat_length`    | channel, 5, 8, bufferID;,             |
|                               | repeatLength(3)                       |

A length of &FFFFFF (-1) repeats to the end of the sample.

## `vdp_audio_sample_debug_info`, `VDP_AUDIO_SAMPLE_LOAD`

    void vdp_audio_sample_debug_info(int channel, int bufferID);

85 channel, 5, &10, bufferID;. The VDP writes what it knows of the
sample to its debug log. The channel is not used, and there is no
answer, not even a status packet.

Command 5's sub-commands:

| Name                                        | Value |
|---------------------------------------------|-------|
| `VDP_AUDIO_SAMPLE_LOAD`                     | 0     |
| `VDP_AUDIO_SAMPLE_CLEAR`                    | 1     |
| `VDP_AUDIO_SAMPLE_FROM_BUFFER`              | 2     |
| `VDP_AUDIO_SAMPLE_SET_FREQUENCY`            | 3     |
| `VDP_AUDIO_SAMPLE_BUFFER_SET_FREQUENCY`     | 4     |
| `VDP_AUDIO_SAMPLE_SET_REPEAT_START`         | 5     |
| `VDP_AUDIO_SAMPLE_BUFFER_SET_REPEAT_START`  | 6     |
| `VDP_AUDIO_SAMPLE_SET_REPEAT_LENGTH`        | 7     |
| `VDP_AUDIO_SAMPLE_BUFFER_SET_REPEAT_LENGTH` | 8     |
| `VDP_AUDIO_SAMPLE_DEBUG_INFO`               | &10   |

## `vdp_audio_volume_envelope_disable`, `vdp_audio_volume_envelope_ADSR`, `VDP_AUDIO_ENVELOPE_ADSR`

    void vdp_audio_volume_envelope_disable(int channel);
    void vdp_audio_volume_envelope_ADSR(int channel, int attack,
                                        int decay, int sustain,
                                        int release);

85 channel, 6, 0 takes the channel's volume envelope off. 85 channel,
6, 1, attack;, decay;, sustain, release; gives it an ADSR one.

| Name                               | Value |
|------------------------------------|-------|
| `VDP_AUDIO_ENVELOPE_NONE`          | 0     |
| `VDP_AUDIO_ENVELOPE_ADSR`          | 1     |
| `VDP_AUDIO_ENVELOPE_MULTIPHASE_ADSR` | 2   |

The VDP reads an envelope's arguments only for a channel that is
enabled: sent to any other, they are taken for VDU commands. This holds
for the frequency envelopes too.

## `vdp_audio_volume_envelope_multiphase_ADSR`

    void vdp_audio_volume_envelope_multiphase_ADSR(uint8_t channel,
                                                   uint8_t numAttack,
                                                   uint8_t numSustain,
                                                   uint8_t numRelease,
                                                   int16_t *envelopeData);

85 channel, 6, 2, then the attack, sustain and release phases in turn,
each part as a count byte and then, for each phase, level, duration;.
`envelopeData` is `[level, duration]` pairs, attack then sustain then
release; the level sent is the low byte of its entry.

libagon counts its way through `envelopeData` in a byte, so with more
than 127 phases in all it wraps back to the start of it.

    int16_t env[] = { 127, 50,   100, 200,   0, 100 };

    vdp_audio_volume_envelope_multiphase_ADSR(0, 1, 1, 1, env);

## `vdp_audio_frequency_envelope_disable`, `vdp_audio_frequency_envelope_stepped`, `VDP_AUDIO_FREQUENCY_REPEATS`, `VDP_AUDIO_FREQ_ENVELOPE_CONTROL_REPEATS`

    void vdp_audio_frequency_envelope_disable(int channel);
    void vdp_audio_frequency_envelope_stepped(int channel,
                                              int phaseCount,
                                              int controlByte,
                                              int stepLength,
                                              int16_t *envelopeData);

85 channel, 7, 0 takes the frequency envelope off. The stepped one
sends 85 channel, 7, 1, phaseCount, controlByte, stepLength;, then
`phaseCount * 4` bytes of `envelopeData`: `[adjustment, steps]` pairs,
two 16-bit words a phase, which lie in memory as the VDP reads them.

The phase count is a byte. libagon counts the words in a byte as well,
and so never finishes with 128 phases or more.

| Name                                   | Value |
|----------------------------------------|-------|
| `VDP_AUDIO_FREQUENCY_ENVELOPE_STEPPED` | 1     |
| `VDP_AUDIO_FREQUENCY_REPEATS`          | &01   |
| `VDP_AUDIO_FREQUENCY_CUMULATIVE`       | &02   |
| `VDP_AUDIO_FREQUENCY_RESTRICT`         | &04   |

libagon's names for the three control bits, with the same values, are
`VDP_AUDIO_FREQ_ENVELOPE_CONTROL_REPEATS`,
`VDP_AUDIO_FREQ_ENVELOPE_CONTROL_CUMULATIVE` and
`VDP_AUDIO_FREQ_ENVELOPE_CONTROL_RESTRICT`.

## `vdp_audio_enable_channel`, `vdp_audio_disable_channel`, `vdp_audio_reset_channel`

    void vdp_audio_enable_channel(int channel);
    void vdp_audio_disable_channel(int channel);
    void vdp_audio_reset_channel(int channel);

85 channel, 8; 85 channel, 9; 85 channel, 10. Disabling idles the
channel rather than removing it: it stays enabled.

## `vdp_audio_sample_seek`, `vdp_audio_sample_duration`, `vdp_audio_sample_rate`

    void vdp_audio_sample_seek(int channel, int position);
    void vdp_audio_sample_duration(int channel, int duration);
    void vdp_audio_sample_rate(int channel, int rate);

85 channel, 11, position(3); 85 channel, 12, duration(3); 85 channel,
13, rate;.

## `vdp_audio_set_waveform_parameter`, `VDP_AUDIO_PARAM_16BIT`

    void vdp_audio_set_waveform_parameter(int channel, int parameter,
                                          int value);

85 channel, 14, parameter, value. With `VDP_AUDIO_PARAM_16BIT` in the
parameter the value is a word, value;, otherwise a byte; a byte
frequency changes only the low byte.

| Name                         | Value |
|------------------------------|-------|
| `VDP_AUDIO_PARAM_DUTY_CYCLE` | 0     |
| `VDP_AUDIO_PARAM_VOLUME`     | 2     |
| `VDP_AUDIO_PARAM_FREQUENCY`  | 3     |
| `VDP_AUDIO_PARAM_16BIT`      | &80   |
| `VDP_AUDIO_PARAM_MASK`       | &0F   |

## `vdp_audio_set_system_volume`, `vdp_audio_system_sample_rate`

    void vdp_audio_set_system_volume(int volume);
    void vdp_audio_system_sample_rate(int rate);

`vdp_audio_set_volume` and `vdp_audio_sample_rate` on channel 255, the
sound system as a whole: 85 255, 2, volume and 85 255, 13, rate;. The
volume is 0 to 127 (255 only asks for it); a rate of 65535 is the
default.
