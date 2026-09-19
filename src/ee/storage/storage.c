#include <ef2/sif.h>
#include <ef2/storage.h>
#include <ef2/storage_rpc.h>

extern const ef2_u8 ef2storage_irx[];
extern const ef2_u32 ef2storage_irx_size;

static ef2_sif_rpc_client g_storage_client;
static ef2_storage_rpc_request g_storage_request EF2_ALIGN(64);
static ef2_storage_rpc_reply g_storage_reply EF2_ALIGN(64);
static ef2_u32 g_storage_device_count;
static ef2_storage_scan_diag g_storage_scan_diag;
static ef2_s32 g_storage_bound;

static void storage_zero(void *ptr, ef2_u32 size)
{
    ef2_u8 *bytes = (ef2_u8 *)ptr;
    ef2_u32 i;
    for (i = 0; i < size; ++i)
        bytes[i] = 0;
}

static void storage_copy_string(char *dest, const char *source, ef2_u32 capacity)
{
    ef2_u32 i = 0;
    if (capacity == 0u)
        return;
    while (i + 1u < capacity && source != (const char *)0 && source[i] != '\0') {
        dest[i] = source[i];
        ++i;
    }
    dest[i] = '\0';
}

static int storage_rpc(ef2_s32 function)
{
    int result;
    storage_zero(&g_storage_reply, sizeof(g_storage_reply));
    result = ef2_sif_call(
        &g_storage_client,
        function,
        &g_storage_request,
        sizeof(g_storage_request),
        &g_storage_reply,
        sizeof(g_storage_reply));
    if (result < 0)
        return result;
    g_storage_device_count = g_storage_reply.device_count;
    g_storage_scan_diag = g_storage_reply.scan_diag;
    return g_storage_reply.result;
}

int ef2_storage_init(void)
{
    ef2_s32 module_result = -1;
    int result;

    if (g_storage_bound)
        return 0;

    result = ef2_sif_init();
    if (result < 0)
        return -1000 + result;

    result = ef2_iop_exec_module_buffer_ex(
        ef2storage_irx, ef2storage_irx_size, &module_result);

    if (result < 0) {
        int patch_result = ef2_iop_enable_module_buffer();
        if (patch_result < 0)
            return -2000 + patch_result;
        module_result = -1;
        result = ef2_iop_exec_module_buffer_ex(
            ef2storage_irx, ef2storage_irx_size, &module_result);
        if (result < 0)
            return -2500 + result;
    }

    if (module_result != 0)
        return -2800 - (module_result & 0xFF);

    storage_zero(&g_storage_client, sizeof(g_storage_client));
    result = ef2_sif_bind(&g_storage_client, EF2_STORAGE_RPC_SID);
    if (result < 0 || g_storage_client.server == (void *)0)
        return -3000 + ((result < 0) ? result : -9);

    g_storage_bound = 1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    result = storage_rpc(EF2_STORAGE_RPC_INIT);
    if (result < 0) {
        g_storage_bound = 0;
        return -4000 + result;
    }
    return 0;
}

int ef2_storage_scan(void)
{
    int result;
    if (!g_storage_bound) {
        result = ef2_storage_init();
        if (result < 0)
            return result;
    }
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    return storage_rpc(EF2_STORAGE_RPC_SCAN);
}

ef2_u32 ef2_storage_get_device_count(void)
{
    return g_storage_device_count;
}

int ef2_storage_get_scan_diag(ef2_storage_scan_diag *diag)
{
    if (diag == (ef2_storage_scan_diag *)0)
        return -1;

    *diag = g_storage_scan_diag;
    return 0;
}

int ef2_storage_get_device(ef2_u32 index, ef2_storage_device_info *info)
{
    int result;
    if (!g_storage_bound || info == (ef2_storage_device_info *)0 || index >= g_storage_device_count)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    result = storage_rpc(EF2_STORAGE_RPC_GET_DEVICE);
    if (result < 0)
        return result;
    *info = g_storage_reply.device;
    return 0;
}

int ef2_storage_read_page(
    ef2_u32 index,
    ef2_u32 page,
    void *buffer,
    ef2_u32 size)
{
    ef2_u8 *dest = (ef2_u8 *)buffer;
    ef2_u32 i;
    int result;

    if (!g_storage_bound ||
        buffer == (void *)0 ||
        size == 0u ||
        size > EF2_STORAGE_IO_CHUNK)
        return -1;

    storage_zero(
        &g_storage_request,
        sizeof(g_storage_request));

    g_storage_request.index = index;
    g_storage_request.page = page;
    g_storage_request.data_size = size;

    result =
        storage_rpc(
            EF2_STORAGE_RPC_READ_PAGE);

    if (result < 0)
        return result;

    if (g_storage_reply.data_size != size)
        return -2;

    for (i = 0; i < size; ++i)
        dest[i] = g_storage_reply.data[i];

    return 0;
}

int ef2_storage_read_sectors(ef2_u32 index, ef2_u32 sector, void *buffer, ef2_u32 count)
{
    ef2_u8 *dest = (ef2_u8 *)buffer;
    ef2_u32 done = 0;
    if (!g_storage_bound || buffer == (void *)0 || count == 0u)
        return -1;

    while (done < count) {
        ef2_u32 i;
        int result;
        storage_zero(&g_storage_request, sizeof(g_storage_request));
        g_storage_request.index = index;
        g_storage_request.sector = sector + done;
        g_storage_request.count = 1u;
        result = storage_rpc(EF2_STORAGE_RPC_READ_SECTOR);
        if (result < 0)
            return done != 0u ? (int)done : result;
        if (g_storage_reply.data_size != EF2_STORAGE_IO_CHUNK)
            return done != 0u ? (int)done : -2;
        for (i = 0; i < EF2_STORAGE_IO_CHUNK; ++i)
            dest[done * EF2_STORAGE_IO_CHUNK + i] = g_storage_reply.data[i];
        ++done;
    }
    return (int)done;
}

int ef2_storage_write_sectors(ef2_u32 index, ef2_u32 sector, const void *buffer, ef2_u32 count)
{
    const ef2_u8 *source = (const ef2_u8 *)buffer;
    ef2_u32 done = 0;
    if (!g_storage_bound || buffer == (const void *)0 || count == 0u)
        return -1;

    while (done < count) {
        ef2_u32 i;
        int result;
        storage_zero(&g_storage_request, sizeof(g_storage_request));
        g_storage_request.index = index;
        g_storage_request.sector = sector + done;
        g_storage_request.count = 1u;
        g_storage_request.data_size = EF2_STORAGE_IO_CHUNK;
        for (i = 0; i < EF2_STORAGE_IO_CHUNK; ++i)
            g_storage_request.data[i] = source[done * EF2_STORAGE_IO_CHUNK + i];
        result = storage_rpc(EF2_STORAGE_RPC_WRITE_SECTOR);
        if (result < 0)
            return done != 0u ? (int)done : result;
        ++done;
    }
    return (int)done;
}

static int mmce_value_call(ef2_s32 function, ef2_u32 index, ef2_u32 *value)
{
    int result;
    if (!g_storage_bound)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    result = storage_rpc(function);
    if (result < 0)
        return result;
    if (value != (ef2_u32 *)0)
        *value = (ef2_u32)g_storage_reply.value;
    return 0;
}

int ef2_storage_mmce_get_status(ef2_u32 index, ef2_u32 *status)
{
    return mmce_value_call(EF2_STORAGE_RPC_MMCE_STATUS, index, status);
}

int ef2_storage_mmce_get_card(ef2_u32 index, ef2_u32 *card)
{
    return mmce_value_call(EF2_STORAGE_RPC_MMCE_GET_CARD, index, card);
}

int ef2_storage_mmce_set_card(ef2_u32 index, ef2_u32 type, ef2_u32 mode, ef2_u32 card)
{
    if (!g_storage_bound)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    g_storage_request.value0 = type;
    g_storage_request.value1 = mode;
    g_storage_request.value2 = card;
    return storage_rpc(EF2_STORAGE_RPC_MMCE_SET_CARD);
}

int ef2_storage_mmce_get_channel(ef2_u32 index, ef2_u32 *channel)
{
    return mmce_value_call(EF2_STORAGE_RPC_MMCE_GET_CHANNEL, index, channel);
}

int ef2_storage_mmce_set_channel(ef2_u32 index, ef2_u32 mode, ef2_u32 channel)
{
    if (!g_storage_bound)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    g_storage_request.value0 = mode;
    g_storage_request.value1 = channel;
    return storage_rpc(EF2_STORAGE_RPC_MMCE_SET_CHANNEL);
}

int ef2_storage_mmce_set_game_id(ef2_u32 index, const char *game_id)
{
    if (!g_storage_bound || game_id == (const char *)0)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    storage_copy_string(g_storage_request.path, game_id, EF2_STORAGE_PATH_MAX);
    return storage_rpc(EF2_STORAGE_RPC_MMCE_SET_GAME_ID);
}

int ef2_storage_mmce_open(ef2_u32 index, const char *path, ef2_u32 flags)
{
    int result;
    if (!g_storage_bound || path == (const char *)0)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    g_storage_request.value0 = flags;
    storage_copy_string(g_storage_request.path, path, EF2_STORAGE_PATH_MAX);
    result = storage_rpc(EF2_STORAGE_RPC_MMCE_OPEN);
    if (result < 0)
        return result;
    return g_storage_reply.value;
}

int ef2_storage_mmce_close(ef2_u32 index, ef2_s32 fd)
{
    if (!g_storage_bound)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    g_storage_request.fd = fd;
    return storage_rpc(EF2_STORAGE_RPC_MMCE_CLOSE);
}

int ef2_storage_mmce_read(ef2_u32 index, ef2_s32 fd, void *buffer, ef2_u32 size)
{
    ef2_u8 *dest = (ef2_u8 *)buffer;
    ef2_u32 done = 0;
    if (!g_storage_bound || buffer == (void *)0)
        return -1;
    while (done < size) {
        ef2_u32 chunk = size - done;
        ef2_u32 i;
        int result;
        if (chunk > EF2_STORAGE_IO_CHUNK)
            chunk = EF2_STORAGE_IO_CHUNK;
        storage_zero(&g_storage_request, sizeof(g_storage_request));
        g_storage_request.index = index;
        g_storage_request.fd = fd;
        g_storage_request.data_size = chunk;
        result = storage_rpc(EF2_STORAGE_RPC_MMCE_READ);
        if (result < 0)
            return done != 0u ? (int)done : result;
        for (i = 0; i < g_storage_reply.data_size; ++i)
            dest[done + i] = g_storage_reply.data[i];
        done += g_storage_reply.data_size;
        if (g_storage_reply.data_size < chunk)
            break;
    }
    return (int)done;
}

int ef2_storage_mmce_write(ef2_u32 index, ef2_s32 fd, const void *buffer, ef2_u32 size)
{
    const ef2_u8 *source = (const ef2_u8 *)buffer;
    ef2_u32 done = 0;
    if (!g_storage_bound || buffer == (const void *)0)
        return -1;
    while (done < size) {
        ef2_u32 chunk = size - done;
        ef2_u32 i;
        int result;
        if (chunk > EF2_STORAGE_IO_CHUNK)
            chunk = EF2_STORAGE_IO_CHUNK;
        storage_zero(&g_storage_request, sizeof(g_storage_request));
        g_storage_request.index = index;
        g_storage_request.fd = fd;
        g_storage_request.data_size = chunk;
        for (i = 0; i < chunk; ++i)
            g_storage_request.data[i] = source[done + i];
        result = storage_rpc(EF2_STORAGE_RPC_MMCE_WRITE);
        if (result < 0)
            return done != 0u ? (int)done : result;
        if (g_storage_reply.value <= 0)
            break;
        done += (ef2_u32)g_storage_reply.value;
        if ((ef2_u32)g_storage_reply.value < chunk)
            break;
    }
    return (int)done;
}

ef2_s32 ef2_storage_mmce_lseek(ef2_u32 index, ef2_s32 fd, ef2_s32 offset, ef2_s32 whence)
{
    int result;
    if (!g_storage_bound)
        return -1;
    storage_zero(&g_storage_request, sizeof(g_storage_request));
    g_storage_request.index = index;
    g_storage_request.fd = fd;
    g_storage_request.offset = offset;
    g_storage_request.whence = whence;
    result = storage_rpc(EF2_STORAGE_RPC_MMCE_LSEEK);
    if (result < 0)
        return result;
    return g_storage_reply.value;
}
