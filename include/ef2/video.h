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

int ef2_video_init(const ef2_video_config *config);
void ef2_video_set_background(ef2_u8 r, ef2_u8 g, ef2_u8 b);
void ef2_video_output_background(void);

#ifdef __cplusplus
}
#endif

#endif
