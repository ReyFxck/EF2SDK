#ifndef EF2_VIDEO_H
#define EF2_VIDEO_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EF2_VIDEO_NTSC = 0x02,
    EF2_VIDEO_PAL  = 0x03
} ef2_video_standard;

typedef enum {
    EF2_VIDEO_FIELD = 0,
    EF2_VIDEO_FRAME = 1
} ef2_video_field_mode;

typedef struct {
    ef2_video_standard standard;
    ef2_u8 interlaced;
    ef2_video_field_mode field_mode;
} ef2_video_config;

typedef struct {
    ef2_u32 dma_available;
    ef2_u32 dma_fallbacks;
} ef2_video_transport_stats;

typedef struct {
    ef2_u16 vram_address;
    ef2_u16 width;
    ef2_u16 height;
    ef2_u8 buffer_width;
    ef2_u8 psm;
    ef2_u8 width_log2;
    ef2_u8 height_log2;
    ef2_u8 valid;
    ef2_u8 reserved[3];
} ef2_video_texture;

int ef2_video_init(const ef2_video_config *config);

int ef2_video_get_size(
    ef2_u16 *width,
    ef2_u16 *height);

int ef2_video_get_transport_stats(
    ef2_video_transport_stats *stats);

int ef2_video_clear(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b);

int ef2_video_draw_rect(
    ef2_s32 x,
    ef2_s32 y,
    ef2_s32 width,
    ef2_s32 height,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b);

int ef2_video_draw_line(
    ef2_s32 x0,
    ef2_s32 y0,
    ef2_s32 x1,
    ef2_s32 y1,
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b);

void ef2_video_reset_texture_allocator(void);

ef2_u32 ef2_video_get_texture_vram_free(void);

int ef2_video_upload_rgba32(
    ef2_video_texture *texture,
    const ef2_u32 *pixels,
    ef2_u16 width,
    ef2_u16 height);

int ef2_video_draw_texture(
    const ef2_video_texture *texture,
    ef2_s32 x,
    ef2_s32 y,
    ef2_s32 width,
    ef2_s32 height);

void ef2_video_set_background(
    ef2_u8 r,
    ef2_u8 g,
    ef2_u8 b);

#ifdef __cplusplus
}
#endif

#endif
