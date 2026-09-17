#include <ef2/audio.h>

#define EF2_AUDIO_Q15_ONE 32768

static ef2_u64 ef2_audio_div_u64_u32(ef2_u64 numerator, ef2_u32 denominator)
{
    ef2_u64 quotient = 0;
    ef2_u64 remainder = 0;
    ef2_s32 bit;

    if (denominator == 0) {
        return 0;
    }

    /*
     * Initialization is not a hot path, so use a tiny freestanding long
     * divider instead of relying on a compiler runtime helper such as
     * __udivdi3.
     */
    for (bit = 63; bit >= 0; --bit) {
        remainder = (remainder << 1) |
                    ((numerator >> (ef2_u32)bit) & 1u);

        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= (ef2_u64)1u << (ef2_u32)bit;
        }
    }

    return quotient;
}

static ef2_s16 ef2_audio_saturate_s16(ef2_s64 value)
{
    if (value > 32767) {
        return 32767;
    }

    if (value < -32768) {
        return -32768;
    }

    return (ef2_s16)value;
}

int ef2_audio_stream_config_valid(const ef2_audio_stream_config *config)
{
    if (config == (const ef2_audio_stream_config *)0) {
        return 0;
    }

    if (config->sample_rate == 0) {
        return 0;
    }

    if (config->channels != 1 && config->channels != 2) {
        return 0;
    }

    if (config->format != EF2_AUDIO_SAMPLE_S16) {
        return 0;
    }

    if (config->resampler != EF2_AUDIO_RESAMPLE_NEAREST &&
        config->resampler != EF2_AUDIO_RESAMPLE_LINEAR) {
        return 0;
    }

    return 1;
}

int ef2_audio_rate_converter_init(
    ef2_audio_rate_converter *converter,
    ef2_u32 input_rate,
    ef2_u32 output_rate,
    ef2_u8 channels,
    ef2_audio_resample_mode mode)
{
    ef2_u64 numerator;

    if (converter == (ef2_audio_rate_converter *)0) {
        return -1;
    }

    if (input_rate == 0 || output_rate == 0) {
        return -2;
    }

    if (channels != 1 && channels != 2) {
        return -3;
    }

    if (mode != EF2_AUDIO_RESAMPLE_NEAREST &&
        mode != EF2_AUDIO_RESAMPLE_LINEAR) {
        return -4;
    }

    converter->input_rate = input_rate;
    converter->output_rate = output_rate;
    converter->channels = channels;
    converter->mode = mode;
    converter->phase_q32 = 0;

    numerator = (ef2_u64)input_rate << 32;
    converter->step_q32 = ef2_audio_div_u64_u32(numerator, output_rate);

    if (converter->step_q32 == 0) {
        return -5;
    }

    return 0;
}

void ef2_audio_rate_converter_reset(ef2_audio_rate_converter *converter)
{
    if (converter != (ef2_audio_rate_converter *)0) {
        converter->phase_q32 = 0;
    }
}

ef2_u32 ef2_audio_rate_converter_process_s16(
    ef2_audio_rate_converter *converter,
    const ef2_s16 *input,
    ef2_u32 input_frames,
    ef2_s16 *output,
    ef2_u32 output_frames_capacity,
    ef2_u32 *input_frames_consumed)
{
    ef2_u32 produced = 0;
    ef2_u32 channels;
    ef2_u64 phase;

    if (input_frames_consumed != (ef2_u32 *)0) {
        *input_frames_consumed = 0;
    }

    if (converter == (ef2_audio_rate_converter *)0 ||
        input == (const ef2_s16 *)0 ||
        output == (ef2_s16 *)0 ||
        input_frames_consumed == (ef2_u32 *)0 ||
        output_frames_capacity == 0 ||
        input_frames == 0) {
        return 0;
    }

    channels = converter->channels;

    /*
     * Exact-rate streams need no interpolation or retained look-ahead frame.
     * This keeps native 48 kHz sources as cheap as a plain copy.
     */
    if (converter->input_rate == converter->output_rate) {
        ef2_u32 frames = input_frames;
        ef2_u32 sample_count;
        ef2_u32 i;

        if (frames > output_frames_capacity) {
            frames = output_frames_capacity;
        }

        sample_count = frames * channels;
        for (i = 0; i < sample_count; ++i) {
            output[i] = input[i];
        }

        converter->phase_q32 = 0;
        *input_frames_consumed = frames;
        return frames;
    }

    if (input_frames < 2) {
        return 0;
    }

    phase = converter->phase_q32;

    while (produced < output_frames_capacity) {
        ef2_u32 input_index = (ef2_u32)(phase >> 32);
        ef2_u32 fraction = (ef2_u32)phase;
        ef2_u32 channel;

        if (input_index + 1u >= input_frames) {
            break;
        }

        for (channel = 0; channel < channels; ++channel) {
            ef2_s32 a = input[input_index * channels + channel];
            ef2_s32 b = input[(input_index + 1u) * channels + channel];
            ef2_s32 sample;

            if (converter->mode == EF2_AUDIO_RESAMPLE_NEAREST) {
                sample = (fraction < 0x80000000u) ? a : b;
            } else {
                ef2_u32 fraction_q16 = fraction >> 16;
                ef2_s32 delta = b - a;
                ef2_s64 interpolated =
                    (ef2_s64)delta * (ef2_s64)fraction_q16;

                sample = a + (ef2_s32)(interpolated >> 16);
            }

            output[produced * channels + channel] = (ef2_s16)sample;
        }

        ++produced;
        phase += converter->step_q32;
    }

    {
        ef2_u64 consumed64 = phase >> 32;
        ef2_u32 consumed;

        /*
         * Keep one source frame when conversion is active. It becomes the
         * left interpolation endpoint when the caller appends the next chunk.
         */
        if (consumed64 >= input_frames) {
            consumed = input_frames - 1u;
        } else {
            consumed = (ef2_u32)consumed64;
        }

        phase -= (ef2_u64)consumed << 32;
        converter->phase_q32 = phase;
        *input_frames_consumed = consumed;
    }

    return produced;
}

void ef2_audio_mix_s16(
    ef2_s16 *destination,
    const ef2_s16 *source,
    ef2_u32 sample_count,
    ef2_s32 gain_q15)
{
    ef2_u32 i;

    if (destination == (ef2_s16 *)0 || source == (const ef2_s16 *)0) {
        return;
    }

    for (i = 0; i < sample_count; ++i) {
        ef2_s64 scaled =
            (ef2_s64)source[i] * (ef2_s64)gain_q15;
        ef2_s64 mixed;

        if (scaled >= 0) {
            scaled = (scaled + EF2_AUDIO_Q15_ONE / 2) >> 15;
        } else {
            scaled = -(((-scaled) + EF2_AUDIO_Q15_ONE / 2) >> 15);
        }

        mixed = (ef2_s64)destination[i] + scaled;
        destination[i] = ef2_audio_saturate_s16(mixed);
    }
}
