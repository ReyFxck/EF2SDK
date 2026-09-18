#include <ef2/gif.h>
#include <ef2/gs.h>
#include <ef2/kernel.h>
#include <ef2/video.h>

static ef2_u16 ef2_video_width;
static ef2_u16 ef2_video_height;
static ef2_u32 ef2_video_gif_dma_available;
static ef2_u32 ef2_video_gif_dma_fallbacks;

static int ef2_video_standard_valid(
    ef2_video_standard standard)
{
    return standard == EF2_VIDEO_NTSC ||
           standard == EF2_VIDEO_PAL;
}

static void ef2_gif_ad(
    ef2_gif_qword *qword,
    ef2_u64 data,
    ef2_u64 address)
{
    qword->lo = data;
    qword->hi = address;
}

static ef2_s32 ef2_clamp_s32(
    ef2_s32 value,
    ef2_s32 low,
    ef2_s32 high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static int ef2_video_submit_packet(
    const ef2_gif_qword *packet,
    ef2_u32 count)
{
    if (ef2_video_gif_dma_available) {
        if (ef2_gif_dma_send_qwords(
                packet,
                count) == 0)
            return 0;

        ++ef2_video_gif_dma_fallbacks;
        ef2_video_gif_dma_available = 0;
    }

    ef2_gif_send_qwords(packet, count);
    return 0;
}

static int ef2_video_draw_two_vertex(
    ef2_u8 primitive,
    ef2_s32 x0,
    ef2_s32 y0,
    ef2_s32 x1,
    ef2_s32 y1,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    ef2_gif_qword packet[9] EF2_ALIGN(16);
    ef2_u16 fx0;
    ef2_u16 fy0;
    ef2_u16 fx1;
    ef2_u16 fy1;

    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    x0 = ef2_clamp_s32(
        x0,
        0,
        (ef2_s32)ef2_video_width);
    y0 = ef2_clamp_s32(
        y0,
        0,
        (ef2_s32)ef2_video_height);
    x1 = ef2_clamp_s32(
        x1,
        0,
        (ef2_s32)ef2_video_width);
    y1 = ef2_clamp_s32(
        y1,
        0,
        (ef2_s32)ef2_video_height);

    fx0 = (ef2_u16)((ef2_u32)x0 << 4);
    fy0 = (ef2_u16)((ef2_u32)y0 << 4);
    fx1 = (ef2_u16)((ef2_u32)x1 << 4);
    fy1 = (ef2_u16)((ef2_u32)y1 << 4);

    packet[0].lo =
        ef2_gif_pack_tag(8, 1, 0, 0, 0, 1);
    packet[0].hi = EF2_GIF_REG_AD;

    ef2_gif_ad(
        &packet[1],
        ef2_gs_pack_frame(
            0,
            (ef2_u8)(ef2_video_width / 64u),
            EF2_GS_PSMCT32,
            0),
        EF2_GS_ADDR_FRAME_1);

    ef2_gif_ad(
        &packet[2],
        ef2_gs_pack_xyoffset(0, 0),
        EF2_GS_ADDR_XYOFFSET_1);

    ef2_gif_ad(
        &packet[3],
        ef2_gs_pack_scissor(
            0,
            (ef2_u16)(ef2_video_width - 1u),
            0,
            (ef2_u16)(ef2_video_height - 1u)),
        EF2_GS_ADDR_SCISSOR_1);

    ef2_gif_ad(
        &packet[4],
        0,
        EF2_GS_ADDR_TEST_1);

    ef2_gif_ad(
        &packet[5],
        ef2_gs_pack_prim(primitive),
        EF2_GS_ADDR_PRIM);

    ef2_gif_ad(
        &packet[6],
        ef2_gs_pack_rgbaq(r, g, b, 0x80),
        EF2_GS_ADDR_RGBAQ);

    ef2_gif_ad(
        &packet[7],
        ef2_gs_pack_xyz(fx0, fy0, 0),
        EF2_GS_ADDR_XYZ2);

    ef2_gif_ad(
        &packet[8],
        ef2_gs_pack_xyz(fx1, fy1, 0),
        EF2_GS_ADDR_XYZ2);

    return ef2_video_submit_packet(packet, 9);
}

void ef2_video_set_background(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    *EF2_GS_REG_BGCOLOR =
        ef2_gs_pack_bgcolor(r, g, b);
    ef2_gs_sync();
}

int ef2_video_get_size(
    ef2_u16 *width,
    ef2_u16 *height)
{
    if (width == (ef2_u16 *)0 ||
        height == (ef2_u16 *)0 ||
        ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    *width = ef2_video_width;
    *height = ef2_video_height;
    return 0;
}

int ef2_video_get_transport_stats(
    ef2_video_transport_stats *stats)
{
    if (stats ==
        (ef2_video_transport_stats *)0)
        return -1;

    stats->dma_available =
        ef2_video_gif_dma_available;
    stats->dma_fallbacks =
        ef2_video_gif_dma_fallbacks;

    return 0;
}

int ef2_video_draw_rect(
    ef2_s32 x,
    ef2_s32 y,
    ef2_s32 width,
    ef2_s32 height,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    ef2_s32 right;
    ef2_s32 bottom;

    if (width <= 0 || height <= 0)
        return -1;

    if (x >= (ef2_s32)ef2_video_width ||
        y >= (ef2_s32)ef2_video_height ||
        x + width <= 0 ||
        y + height <= 0)
        return 0;

    right = x + width;
    bottom = y + height;

    x = ef2_clamp_s32(
        x,
        0,
        (ef2_s32)ef2_video_width);
    y = ef2_clamp_s32(
        y,
        0,
        (ef2_s32)ef2_video_height);
    right = ef2_clamp_s32(
        right,
        0,
        (ef2_s32)ef2_video_width);
    bottom = ef2_clamp_s32(
        bottom,
        0,
        (ef2_s32)ef2_video_height);

    if (right <= x || bottom <= y)
        return 0;

    return ef2_video_draw_two_vertex(
        EF2_GS_PRIM_SPRITE,
        x,
        y,
        right,
        bottom,
        r,
        g,
        b);
}

int ef2_video_draw_line(
    ef2_s32 x0,
    ef2_s32 y0,
    ef2_s32 x1,
    ef2_s32 y1,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    /*
     * Endpoint clamping is intentionally simple for the first primitive API.
     * Full line clipping can be added without changing the public ABI.
     */
    return ef2_video_draw_two_vertex(
        EF2_GS_PRIM_LINE,
        x0,
        y0,
        x1,
        y1,
        r,
        g,
        b);
}

int ef2_video_clear(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b)
{
    if (ef2_video_width == 0u ||
        ef2_video_height == 0u)
        return -1;

    return ef2_video_draw_rect(
        0,
        0,
        ef2_video_width,
        ef2_video_height,
        r,
        g,
        b);
}

int ef2_video_init(
    const ef2_video_config *config)
{
    ef2_u16 dx;
    ef2_u16 dy;

    if (config ==
        (const ef2_video_config *)0)
        return -1;

    if (!ef2_video_standard_valid(
            config->standard))
        return -2;

    if (!config->interlaced ||
        config->field_mode != EF2_VIDEO_FIELD)
        return -3;

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

    *EF2_GS_REG_DISPFB2 =
        ef2_gs_pack_dispfb(
            0,
            (ef2_u8)(
                ef2_video_width / 64u),
            EF2_GS_PSMCT32,
            0,
            0);

    *EF2_GS_REG_DISPLAY2 =
        ef2_gs_pack_display(
            dx,
            dy,
            3,
            0,
            (ef2_u16)(
                ef2_video_width * 4u - 1u),
            (ef2_u16)(
                ef2_video_height - 1u));

    ef2_gs_sync();

    if (ef2_video_clear(0, 0, 0) != 0)
        return -4;

    *EF2_GS_REG_PMODE =
        ef2_gs_pack_pmode(
            0,
            1,
            1,
            1,
            0,
            0xFF);

    ef2_gs_sync();
    return 0;
}
