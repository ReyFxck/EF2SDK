#include <assert.h>

#include <ef2/audio.h>

static void test_stream_config(void)
{
    ef2_audio_stream_config config = {
        .sample_rate = 32000,
        .channels = 2,
        .format = EF2_AUDIO_SAMPLE_S16,
        .resampler = EF2_AUDIO_RESAMPLE_LINEAR,
    };

    assert(ef2_audio_stream_config_valid(&config) == 1);

    config.sample_rate = 0;
    assert(ef2_audio_stream_config_valid(&config) == 0);

    config.sample_rate = 32768;
    config.channels = 3;
    assert(ef2_audio_stream_config_valid(&config) == 0);
}

static void test_exact_rate_copy(void)
{
    ef2_audio_rate_converter converter;
    const ef2_s16 input[8] = {
        -30000, -10000, -1, 0, 1, 10000, 20000, 30000
    };
    ef2_s16 output[8] = {0};
    ef2_u32 consumed = 0;
    ef2_u32 produced;
    ef2_u32 i;

    assert(ef2_audio_rate_converter_init(
        &converter,
        48000,
        48000,
        1,
        EF2_AUDIO_RESAMPLE_LINEAR) == 0);

    produced = ef2_audio_rate_converter_process_s16(
        &converter,
        input,
        8,
        output,
        8,
        &consumed);

    assert(produced == 8);
    assert(consumed == 8);

    for (i = 0; i < 8; ++i) {
        assert(output[i] == input[i]);
    }
}

static void test_32000_to_48000_stereo(void)
{
    ef2_audio_rate_converter converter;
    ef2_s16 input[64 * 2];
    ef2_s16 output[128 * 2];
    ef2_u32 consumed = 0;
    ef2_u32 produced;
    ef2_u32 i;

    for (i = 0; i < 64; ++i) {
        input[i * 2] = (ef2_s16)((ef2_s32)i * 400 - 12000);
        input[i * 2 + 1] = (ef2_s16)(12000 - (ef2_s32)i * 400);
    }

    assert(ef2_audio_rate_converter_init(
        &converter,
        32000,
        48000,
        2,
        EF2_AUDIO_RESAMPLE_LINEAR) == 0);

    produced = ef2_audio_rate_converter_process_s16(
        &converter,
        input,
        64,
        output,
        128,
        &consumed);

    /* 32 kHz -> 48 kHz should produce about 1.5x as many frames. */
    assert(produced >= 93);
    assert(produced <= 96);
    assert(consumed == 63);

    assert(output[0] == input[0]);
    assert(output[1] == input[1]);

    for (i = 1; i < produced; ++i) {
        assert(output[i * 2] >= output[(i - 1) * 2]);
        assert(output[i * 2 + 1] <= output[(i - 1) * 2 + 1]);
    }
}

static void test_arbitrary_emulator_rate(void)
{
    ef2_audio_rate_converter converter;

    /*
     * A non-round rate is intentional: emulator cores should not need to
     * pretend their native rate is one of a small set accepted by the SDK.
     */
    assert(ef2_audio_rate_converter_init(
        &converter,
        32768,
        48000,
        2,
        EF2_AUDIO_RESAMPLE_NEAREST) == 0);

    assert(converter.step_q32 != 0);
}

static void test_saturating_mix(void)
{
    ef2_s16 destination[4] = {30000, -30000, 100, -100};
    const ef2_s16 source[4] = {10000, -10000, 200, -200};

    ef2_audio_mix_s16(destination, source, 4, 32768);

    assert(destination[0] == 32767);
    assert(destination[1] == -32768);
    assert(destination[2] == 300);
    assert(destination[3] == -300);
}

int main(void)
{
    test_stream_config();
    test_exact_rate_copy();
    test_32000_to_48000_stereo();
    test_arbitrary_emulator_rate();
    test_saturating_mix();
    return 0;
}
