#ifndef EF2_AUDIO_H
#define EF2_AUDIO_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EF2_AUDIO_SAMPLE_S16 = 1
} ef2_audio_sample_format;

typedef enum {
    EF2_AUDIO_RESAMPLE_NEAREST = 0,
    EF2_AUDIO_RESAMPLE_LINEAR = 1
} ef2_audio_resample_mode;

typedef struct {
    ef2_u32 sample_rate;
    ef2_u8 channels;
    ef2_audio_sample_format format;
    ef2_audio_resample_mode resampler;
} ef2_audio_stream_config;

/*
 * Streaming sample-rate converter state.
 *
 * phase_q32 is relative to the first input frame passed to the next process
 * call. process() reports how many input frames the caller may discard.
 * Keeping unconsumed frames makes interpolation continuous across chunks.
 */
typedef struct {
    ef2_u32 input_rate;
    ef2_u32 output_rate;
    ef2_u8 channels;
    ef2_audio_resample_mode mode;
    ef2_u64 phase_q32;
    ef2_u64 step_q32;
} ef2_audio_rate_converter;

int ef2_audio_stream_config_valid(const ef2_audio_stream_config *config);

int ef2_audio_rate_converter_init(
    ef2_audio_rate_converter *converter,
    ef2_u32 input_rate,
    ef2_u32 output_rate,
    ef2_u8 channels,
    ef2_audio_resample_mode mode);

void ef2_audio_rate_converter_reset(ef2_audio_rate_converter *converter);

/*
 * Convert interleaved signed 16-bit PCM.
 *
 * Returns the number of output frames written. input_frames_consumed receives
 * the number of complete input frames the caller may discard before the next
 * call. For rate conversion, at least one look-ahead frame is retained so
 * interpolation remains continuous across streaming chunks.
 */
ef2_u32 ef2_audio_rate_converter_process_s16(
    ef2_audio_rate_converter *converter,
    const ef2_s16 *input,
    ef2_u32 input_frames,
    ef2_s16 *output,
    ef2_u32 output_frames_capacity,
    ef2_u32 *input_frames_consumed);

/*
 * Mix signed 16-bit samples into an existing destination buffer.
 * gain_q15 uses 32768 as unity gain. The result is saturated to S16.
 */
void ef2_audio_mix_s16(
    ef2_s16 *destination,
    const ef2_s16 *source,
    ef2_u32 sample_count,
    ef2_s32 gain_q15);

#ifdef __cplusplus
}
#endif

#endif
