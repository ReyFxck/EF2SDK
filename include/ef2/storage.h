#ifndef EF2_STORAGE_H
#define EF2_STORAGE_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EF2_STORAGE_MAX_DEVICES 3u
#define EF2_STORAGE_IO_CHUNK 512u
#define EF2_STORAGE_PATH_MAX 240u
#define EF2_STORAGE_GAME_ID_MAX 250u

typedef enum {
    EF2_STORAGE_KIND_NONE = 0,
    EF2_STORAGE_KIND_PS2_MEMORY_CARD = 1,
    EF2_STORAGE_KIND_MMCE = 2,
    EF2_STORAGE_KIND_MX4SIO = 3
} ef2_storage_kind;

enum {
    EF2_STORAGE_CAP_MEMORY_CARD = 1u << 0,
    EF2_STORAGE_CAP_GEOMETRY = 1u << 1,
    EF2_STORAGE_CAP_BLOCK_READ = 1u << 2,
    EF2_STORAGE_CAP_BLOCK_WRITE = 1u << 3,
    EF2_STORAGE_CAP_FILESYSTEM = 1u << 4,
    EF2_STORAGE_CAP_VIRTUAL_CARDS = 1u << 5,
    EF2_STORAGE_CAP_GAME_ID = 1u << 6,
    EF2_STORAGE_CAP_PAGE_READ = 1u << 7
};

enum {
    EF2_STORAGE_OPEN_READ = 1,
    EF2_STORAGE_OPEN_WRITE = 2,
    EF2_STORAGE_OPEN_READ_WRITE = 3,
    EF2_STORAGE_OPEN_APPEND = 0x100,
    EF2_STORAGE_OPEN_CREATE = 0x200,
    EF2_STORAGE_OPEN_TRUNCATE = 0x400,
    EF2_STORAGE_OPEN_EXCLUSIVE = 0x800
};

typedef struct {
    ef2_storage_kind kind;
    ef2_u32 capabilities;
    ef2_u32 physical_port;
    ef2_u32 page_size;
    ef2_u32 erase_block_pages;
    ef2_u32 page_count;
    ef2_u32 sector_size;
    ef2_u32 sector_count;
    ef2_u32 protocol_version;
    ef2_u32 product_id;
    ef2_u32 product_revision;
    ef2_u32 card_flags;
    ef2_u32 current_card;
    ef2_u32 current_channel;
    ef2_u32 status;
} ef2_storage_device_info;

typedef struct {
    ef2_s32 mmce_result[2];
    ef2_s32 mc_terminator_result[2];
    ef2_s32 mc_geometry_result[2];
    ef2_s32 mx4sio_result;
} ef2_storage_scan_diag;

int ef2_storage_init(void);
int ef2_storage_scan(void);
ef2_u32 ef2_storage_get_device_count(void);
int ef2_storage_get_device(ef2_u32 index, ef2_storage_device_info *info);
int ef2_storage_get_scan_diag(ef2_storage_scan_diag *diag);

int ef2_storage_read_page(
    ef2_u32 index,
    ef2_u32 page,
    void *buffer,
    ef2_u32 size);

int ef2_storage_read_sectors(ef2_u32 index, ef2_u32 sector, void *buffer, ef2_u32 count);
int ef2_storage_write_sectors(ef2_u32 index, ef2_u32 sector, const void *buffer, ef2_u32 count);

int ef2_storage_mmce_get_status(ef2_u32 index, ef2_u32 *status);
int ef2_storage_mmce_get_card(ef2_u32 index, ef2_u32 *card);
int ef2_storage_mmce_set_card(ef2_u32 index, ef2_u32 type, ef2_u32 mode, ef2_u32 card);
int ef2_storage_mmce_get_channel(ef2_u32 index, ef2_u32 *channel);
int ef2_storage_mmce_set_channel(ef2_u32 index, ef2_u32 mode, ef2_u32 channel);
int ef2_storage_mmce_set_game_id(ef2_u32 index, const char *game_id);

int ef2_storage_mmce_open(ef2_u32 index, const char *path, ef2_u32 flags);
int ef2_storage_mmce_close(ef2_u32 index, ef2_s32 fd);
int ef2_storage_mmce_read(ef2_u32 index, ef2_s32 fd, void *buffer, ef2_u32 size);
int ef2_storage_mmce_write(ef2_u32 index, ef2_s32 fd, const void *buffer, ef2_u32 size);
ef2_s32 ef2_storage_mmce_lseek(ef2_u32 index, ef2_s32 fd, ef2_s32 offset, ef2_s32 whence);

#ifdef __cplusplus
}
#endif

#endif
