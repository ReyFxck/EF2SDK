#include <ef2/audio.h>
#include <ef2/audio_rpc.h>
#include <ef2/sif.h>

#include "ef2audio_debug.h"

extern const ef2_u8 ef2audio_irx[];
extern const ef2_u32 ef2audio_irx_size;

static ef2_sif_rpc_client g_audio_client;
static ef2_audio_rpc_submit g_submit_buffer EF2_ALIGN(64);
static ef2_audio_rpc_reply g_reply_buffer EF2_ALIGN(64);
static ef2_s32 g_audio_bound;

static void audio_debug_emit_loader_marker(ef2_s32 stage)
{
    const char *path;

    switch (stage) {
        case -1: path = "rom0:EF2NOMOD"; break;
        case 1:  path = "rom0:EF2DBG01"; break;
        case 2:  path = "rom0:EF2DBG02"; break;
        case 3:  path = "rom0:EF2DBG03"; break;
        case 4:  path = "rom0:EF2DBG04"; break;
        case 5:  path = "rom0:EF2DBG05"; break;
        case 6:  path = "rom0:EF2DBG06"; break;
        case 7:  path = "rom0:EF2DBG07"; break;
        case 8:  path = "rom0:EF2DBG08"; break;
        case 9:  path = "rom0:EF2DBG09"; break;
        case 10: path = "rom0:EF2DBG10"; break;
        case 11: path = "rom0:EF2DBG11"; break;
        case 12: path = "rom0:EF2DBG12"; break;
        case 13: path = "rom0:EF2DBG13"; break;
        default: path = "rom0:EF2DBG00"; break;
    }

    /*
     * These names intentionally do not exist. NetherSX2 logs LOADFILE path
     * requests, giving us a reliable textual trace of the last IOP stage even
     * when Kprintf/stdio output is not surfaced by the emulator.
     */
    (void)ef2_iop_load_module(path);
}

static ef2_s32 audio_debug_read_stage(void)
{
    ef2_u32 stage = 0;
    int result;

    result = ef2_iop_debug_read_module_u32(
        "ef2audio",
        EF2AUDIO_DEBUG_STAGE_OFFSET,
        &stage);

    if (result < 0)
        return -1;

    return (ef2_s32)stage;
}

static void audio_zero(void *ptr, ef2_u32 size)
{
    ef2_u8 *p = (ef2_u8 *)ptr;
    ef2_u32 i;
    for (i = 0; i < size; ++i)
        p[i] = 0;
}

static int audio_rpc_simple(ef2_s32 function)
{
    int result;

    audio_zero(&g_reply_buffer, sizeof(g_reply_buffer));

    result = ef2_sif_call(
        &g_audio_client,
        function,
        (void *)0,
        0,
        &g_reply_buffer,
        sizeof(g_reply_buffer));

    if (result < 0)
        return result;

    return g_reply_buffer.result;
}

int ef2_audio_device_init(void)
{
    int result;
    ef2_s32 libsd_modres = -1;
    ef2_s32 audio_modres = -1;

    result = ef2_sif_init();
    if (result < 0)
        return -1000 + result;

    /*
     * ROM LIBSD may already be resident. A negative module-id result alone is
     * therefore not fatal; the authoritative test is whether ef2audio.irx can
     * start and remain resident with its libsd import resolved.
     */
    (void)ef2_iop_load_module_ex("rom0:LIBSD", &libsd_modres);

    /*
     * Embedded IRX remains the real EF2SDK path. Android SAF-backed host:
     * paths are unreliable in some Aether/NetherSX2 builds, so alpha.12
     * diagnoses the embedded path without filesystem assistance.
     */
    audio_modres = -1;
    result = ef2_iop_exec_module_buffer_ex(
        ef2audio_irx,
        ef2audio_irx_size,
        &audio_modres);

    if (result < 0) {
        int patch_result = ef2_iop_enable_module_buffer();

        if (patch_result < 0)
            return -2000 + patch_result;

        audio_modres = -1;
        result = ef2_iop_exec_module_buffer_ex(
            ef2audio_irx,
            ef2audio_irx_size,
            &audio_modres);

        if (result < 0)
            return -2500 + result;
    }

    /*
     * Emit the last stage through the emulator-visible LOADFILE log. This is
     * intentionally done before trusting modres, because alpha.10 showed that
     * a broken return path can otherwise make a zeroed result look valid.
     */
    {
        ef2_s32 debug_stage = audio_debug_read_stage();
        audio_debug_emit_loader_marker(debug_stage);
    }

    /*
     * MODULE_RESIDENT_END is 0. Any other module-start result means the IRX
     * was parsed/started but did not remain installed, so binding its SID can
     * never succeed.
     */
    if (audio_modres != 0)
        return -2800 - (audio_modres & 0xFF);

    audio_zero(&g_audio_client, sizeof(g_audio_client));

    result = ef2_sif_bind(&g_audio_client, EF2_AUDIO_RPC_SID_PRIMARY);
    if (result < 0 || g_audio_client.server == (void *)0) {
        audio_zero(&g_audio_client, sizeof(g_audio_client));
        result = ef2_sif_bind(
            &g_audio_client,
            EF2_AUDIO_RPC_SID_FALLBACK);
    }

    if (result < 0 || g_audio_client.server == (void *)0)
        return -3000 + ((result < 0) ? result : -9);

    g_audio_bound = 1;

    result = audio_rpc_simple(EF2_AUDIO_RPC_INIT);
    if (result < 0)
        return -4000 + result;

    return 0;
}

int ef2_audio_device_submit_48k_stereo_s16(
    const ef2_s16 *samples,
    ef2_u32 frames)
{
    ef2_u32 offset = 0;

    if (!g_audio_bound || samples == (const ef2_s16 *)0)
        return -1;

    while (offset < frames) {
        ef2_u32 count = frames - offset;
        ef2_u32 i;
        ef2_u32 bytes;
        int result;

        if (count > EF2_AUDIO_RPC_MAX_FRAMES)
            count = EF2_AUDIO_RPC_MAX_FRAMES;

        g_submit_buffer.frames = count;

        for (i = 0; i < count * 2u; ++i)
            g_submit_buffer.samples[i] = samples[offset * 2u + i];

        bytes = sizeof(ef2_u32) + count * 2u * sizeof(ef2_s16);
        audio_zero(&g_reply_buffer, sizeof(g_reply_buffer));

        result = ef2_sif_call(
            &g_audio_client,
            EF2_AUDIO_RPC_SUBMIT,
            &g_submit_buffer,
            bytes,
            &g_reply_buffer,
            sizeof(g_reply_buffer));

        if (result < 0)
            return result;
        if (g_reply_buffer.result < 0)
            return g_reply_buffer.result;

        offset += count;
    }

    return (int)frames;
}

int ef2_audio_device_start(void)
{
    if (!g_audio_bound)
        return -1;
    return audio_rpc_simple(EF2_AUDIO_RPC_START);
}

int ef2_audio_device_get_stats(ef2_audio_device_stats *stats)
{
    int result;

    if (!g_audio_bound || stats == (ef2_audio_device_stats *)0)
        return -1;

    result = audio_rpc_simple(EF2_AUDIO_RPC_STATS);
    if (result < 0)
        return result;

    stats->queued_frames = g_reply_buffer.queued_frames;
    stats->capacity_frames = g_reply_buffer.capacity_frames;
    stats->underruns = g_reply_buffer.underruns;
    stats->overruns = g_reply_buffer.overruns;

    return 0;
}
