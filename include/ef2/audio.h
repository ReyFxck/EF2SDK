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

typedef struct {
    ef2_u32 input_rate;
    ef2_u32 output_rate;
    ef2_u8 channels;
    ef2_audio_resample_mode mode;
    ef2_u64 phase_q32;
    ef2_u64 step_q32;
} ef2_audio_rate_converter;

typedef struct {
    ef2_u32 queued_frames;
    ef2_u32 capacity_frames;
    ef2_u32 underruns;
    ef2_u32 overruns;
} ef2_audio_device_stats;

int ef2_audio_stream_config_valid(const ef2_audio_stream_config *config);

int ef2_audio_rate_converter_init(
    ef2_audio_rate_converter *converter,
    ef2_u32 input_rate,
    ef2_u32 output_rate,
    ef2_u8 channels,
    ef2_audio_resample_mode mode);

void ef2_audio_rate_converter_reset(ef2_audio_rate_converter *converter);

ef2_u32 ef2_audio_rate_converter_process_s16(
    ef2_audio_rate_converter *converter,
    const ef2_s16 *input,
    ef2_u32 input_frames,
    ef2_s16 *output,
    ef2_u32 output_frames_capacity,
    ef2_u32 *input_frames_consumed);

void ef2_audio_mix_s16(
    ef2_s16 *destination,
    const ef2_s16 *source,
    ef2_u32 sample_count,
    ef2_s32 gain_q15);

int ef2_audio_device_init(void);
int ef2_audio_device_submit_48k_stereo_s16(
    const ef2_s16 *samples,
    ef2_u32 frames);
int ef2_audio_device_start(void);
int ef2_audio_device_get_stats(ef2_audio_device_stats *stats);

#ifdef __cplusplus
}
#endif

#endif
