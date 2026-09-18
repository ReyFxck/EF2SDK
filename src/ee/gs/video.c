#include <ef2/gif.h>
#include <ef2/gs.h>
#include <ef2/kernel.h>
#include <ef2/video.h>

static ef2_u16 ef2_video_width;
static ef2_u16 ef2_video_height;
static ef2_u32 ef2_video_gif_dma_available;
static ef2_u32 ef2_video_gif_dma_fallbacks;

static int ef2_video_standard_valid(ef2_video_standard standard)
{
    return standard == EF2_VIDEO_NTSC || standard == EF2_VIDEO_PAL;
}

static void ef2_gif_ad(ef2_gif_qword *qword, ef2_u64 data, ef2_u64 address)
{
    qword->lo = data;
    qword->hi = address;
}

void ef2_video_set_background(ef2_u8 r, ef2_u8 g, ef2_u8 b)
{
    *EF2_GS_REG_BGCOLOR = ef2_gs_pack_bgcolor(r, g, b);
    ef2_gs_sync();
}

int ef2_video_clear(ef2_u8 r, ef2_u8 g, ef2_u8 b)
{
    ef2_gif_qword packet[9] EF2_ALIGN(16);
    ef2_u16 right;
    ef2_u16 bottom;

    if (ef2_video_width == 0 || ef2_video_height == 0) {
        return -1;
    }

    right = (ef2_u16)(ef2_video_width << 4);
    bottom = (ef2_u16)(ef2_video_height << 4);

    packet[0].lo = ef2_gif_pack_tag(8, 1, 0, 0, 0, 1);
    packet[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &packet[1],
        ef2_gs_pack_frame(0, (ef2_u8)(ef2_video_width / 64u), EF2_GS_PSMCT32, 0),
        EF2_GS_ADDR_FRAME_1);
    ef2_gif_ad(&packet[2], ef2_gs_pack_xyoffset(0, 0), EF2_GS_ADDR_XYOFFSET_1);
    ef2_gif_ad(
        &packet[3],
        ef2_gs_pack_scissor(
            0,
            (ef2_u16)(ef2_video_width - 1u),
            0,
            (ef2_u16)(ef2_video_height - 1u)),
        EF2_GS_ADDR_SCISSOR_1);
    ef2_gif_ad(&packet[4], 0, EF2_GS_ADDR_TEST_1);
    ef2_gif_ad(&packet[5], ef2_gs_pack_prim(EF2_GS_PRIM_SPRITE), EF2_GS_ADDR_PRIM);
    ef2_gif_ad(&packet[6], ef2_gs_pack_rgbaq(r, g, b, 0x80), EF2_GS_ADDR_RGBAQ);
    ef2_gif_ad(&packet[7], ef2_gs_pack_xyz(0, 0, 0), EF2_GS_ADDR_XYZ2);
    ef2_gif_ad(&packet[8], ef2_gs_pack_xyz(right, bottom, 0), EF2_GS_ADDR_XYZ2);

    if (ef2_video_gif_dma_available) {
        if (ef2_gif_dma_send_qwords(
                packet,
                9) == 0)
            return 0;

        ++ef2_video_gif_dma_fallbacks;
        ef2_video_gif_dma_available = 0;
    }

    ef2_gif_send_qwords(packet, 9);
    return 0;
}

int ef2_video_init(const ef2_video_config *config)
{
    ef2_u16 dx;
    ef2_u16 dy;

    if (config == (const ef2_video_config *)0) {
        return -1;
    }

    if (!ef2_video_standard_valid(config->standard)) {
        return -2;
    }

    /*
     * The current PoC timing tables intentionally cover only the classic
     * interlaced field modes. Other combinations will get their own tables
     * instead of silently reusing timings that may not be safe on hardware.
     */
    if (!config->interlaced || config->field_mode != EF2_VIDEO_FIELD) {
        return -3;
    }

    if (config->standard == EF2_VIDEO_PAL) {
        ef2_video_width = 640;
        ef2_video_height = 512;
        dx = 680;
        dy = 72;
    } else {
        ef2_video_width = 640;
        ef2_video_height = 448;
        dx = 656;
        dy = 36;
    }

    *EF2_GS_REG_PMODE = 0;
    ef2_gs_sync();

    *EF2_GS_REG_CSR = EF2_GS_CSR_RESET;
    ef2_gs_sync();
    *EF2_GS_REG_CSR = 0;
    *EF2_GS_REG_IMR = 0x00007F00u;
    ef2_gs_sync();

    ef2_gif_reset();

    ef2_video_gif_dma_fallbacks = 0;
    ef2_video_gif_dma_available =
        ef2_gif_dma_init() == 0 ? 1u : 0u;

    ef2_kernel_set_gs_crt(
        (ef2_s16)1,
        (ef2_s16)config->standard,
        (ef2_s16)EF2_VIDEO_FIELD);

    ef2_video_set_background(0, 0, 0);

    *EF2_GS_REG_DISPFB2 = ef2_gs_pack_dispfb(
        0,
        (ef2_u8)(ef2_video_width / 64u),
        EF2_GS_PSMCT32,
        0,
        0);

    *EF2_GS_REG_DISPLAY2 = ef2_gs_pack_display(
        dx,
        dy,
        3,
        0,
        (ef2_u16)(ef2_video_width * 4u - 1u),
        (ef2_u16)(ef2_video_height - 1u));

    ef2_gs_sync();

    if (ef2_video_clear(0, 0, 0) != 0) {
        return -4;
    }

    /*
     * Use read circuit 2 at full alpha. Unlike the alpha.1 BGCOLOR shortcut,
     * this actually scans the framebuffer that the GIF packet writes.
     */
    *EF2_GS_REG_PMODE = ef2_gs_pack_pmode(0, 1, 1, 1, 0, 0xFF);
    ef2_gs_sync();

    return 0;
}
