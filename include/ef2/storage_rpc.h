#ifndef EF2_STORAGE_RPC_H
#define EF2_STORAGE_RPC_H

#include <ef2/storage.h>

#define EF2_STORAGE_RPC_SID 0xEF2C0001u

enum {
    EF2_STORAGE_RPC_INIT = 0,
    EF2_STORAGE_RPC_SCAN = 1,
    EF2_STORAGE_RPC_GET_DEVICE = 2,
    EF2_STORAGE_RPC_READ_SECTOR = 3,
    EF2_STORAGE_RPC_WRITE_SECTOR = 4,
    EF2_STORAGE_RPC_MMCE_STATUS = 5,
    EF2_STORAGE_RPC_MMCE_GET_CARD = 6,
    EF2_STORAGE_RPC_MMCE_SET_CARD = 7,
    EF2_STORAGE_RPC_MMCE_GET_CHANNEL = 8,
    EF2_STORAGE_RPC_MMCE_SET_CHANNEL = 9,
    EF2_STORAGE_RPC_MMCE_SET_GAME_ID = 10,
    EF2_STORAGE_RPC_MMCE_OPEN = 11,
    EF2_STORAGE_RPC_MMCE_CLOSE = 12,
    EF2_STORAGE_RPC_MMCE_READ = 13,
    EF2_STORAGE_RPC_MMCE_WRITE = 14,
    EF2_STORAGE_RPC_MMCE_LSEEK = 15
};

typedef struct {
    ef2_u32 index;
    ef2_u32 sector;
    ef2_u32 count;
    ef2_s32 fd;
    ef2_s32 offset;
    ef2_s32 whence;
    ef2_u32 value0;
    ef2_u32 value1;
    ef2_u32 value2;
    ef2_u32 data_size;
    char path[EF2_STORAGE_PATH_MAX];
    ef2_u8 data[EF2_STORAGE_IO_CHUNK];
} ef2_storage_rpc_request;

typedef struct {
    ef2_s32 result;
    ef2_u32 device_count;
    ef2_s32 value;
    ef2_u32 data_size;
    ef2_storage_device_info device;
    ef2_storage_scan_diag scan_diag;
    ef2_u8 data[EF2_STORAGE_IO_CHUNK];
} ef2_storage_rpc_reply;

#endif
