#ifndef EF2_STORAGE_BACKENDS_H
#define EF2_STORAGE_BACKENDS_H
#include <ef2/storage.h>
int ef2_storage_backend_scan(ef2_storage_device_info *devices, ef2_u32 capacity, ef2_u32 *count, ef2_storage_scan_diag *diag);
int ef2_storage_backend_read_sector(const ef2_storage_device_info *device, ef2_u32 sector, ef2_u8 data[EF2_STORAGE_IO_CHUNK]);
int ef2_storage_backend_write_sector(const ef2_storage_device_info *device, ef2_u32 sector, const ef2_u8 data[EF2_STORAGE_IO_CHUNK]);
int ef2_storage_backend_mmce_status(const ef2_storage_device_info *device);
int ef2_storage_backend_mmce_get_card(const ef2_storage_device_info *device);
int ef2_storage_backend_mmce_set_card(const ef2_storage_device_info *device, ef2_u32 type, ef2_u32 mode, ef2_u32 card);
int ef2_storage_backend_mmce_get_channel(const ef2_storage_device_info *device);
int ef2_storage_backend_mmce_set_channel(const ef2_storage_device_info *device, ef2_u32 mode, ef2_u32 channel);
int ef2_storage_backend_mmce_set_game_id(const ef2_storage_device_info *device, const char *game_id);
int ef2_storage_backend_mmce_open(const ef2_storage_device_info *device, const char *path, ef2_u32 flags);
int ef2_storage_backend_mmce_close(const ef2_storage_device_info *device, ef2_s32 fd);
int ef2_storage_backend_mmce_read(const ef2_storage_device_info *device, ef2_s32 fd, ef2_u8 *data, ef2_u32 size);
int ef2_storage_backend_mmce_write(const ef2_storage_device_info *device, ef2_s32 fd, const ef2_u8 *data, ef2_u32 size);
ef2_s32 ef2_storage_backend_mmce_lseek(const ef2_storage_device_info *device, ef2_s32 fd, ef2_s32 offset, ef2_s32 whence);
#endif
