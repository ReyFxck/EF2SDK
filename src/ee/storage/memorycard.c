#include <ef2/cache.h>
#include <ef2/memorycard.h>
#include <ef2/sif.h>

#define EF2_MCSERV_RPC_SID      0x80000400u
#define EF2_MCSERV_CMD_INIT     0x70
#define EF2_MCSERV_CMD_GET_INFO 0x78

typedef struct {
    ef2_s32 fd;
    ef2_s32 port;
    ef2_s32 slot;
    ef2_s32 size;
    ef2_s32 offset;
    ef2_s32 origin;
    void *buffer;
    void *param;
    ef2_u8 data[16];
} EF2_ALIGN(64) ef2_mc_desc_param;

typedef struct {
    ef2_s32 type;
    ef2_s32 free_clusters;
    void *dest1;
    void *dest2;
    ef2_u8 src1[16];
    ef2_u8 src2[16];
    ef2_u8 unused[16];
} EF2_ALIGN(64) ef2_mc_end_param;

static ef2_sif_rpc_client g_mc_client;
static ef2_mc_desc_param g_mc_desc;
static ef2_mc_end_param g_mc_end;
static ef2_s32 g_mc_reply[4] EF2_ALIGN(64);
static ef2_s32 g_mc_initialized;

static void ef2_mc_zero(void *ptr, ef2_u32 size)
{
    ef2_u8 *bytes = (ef2_u8 *)ptr;
    ef2_u32 i;

    for (i = 0; i < size; ++i)
        bytes[i] = 0;
}

static const ef2_mc_end_param *ef2_mc_end_uncached(void)
{
    return (const ef2_mc_end_param *)(
        ((ef2_u32)&g_mc_end) | 0x20000000u);
}

static int ef2_mc_bind(void)
{
    ef2_mc_zero(&g_mc_client, sizeof(g_mc_client));

    if (ef2_sif_bind(
            &g_mc_client,
            EF2_MCSERV_RPC_SID) < 0)
        return -1;

    if (g_mc_client.server == (void *)0)
        return -2;

    return 0;
}

static void ef2_mc_try_load_rom_modules(
    const char *sio2,
    const char *mcman,
    const char *mcserv)
{
    (void)ef2_iop_load_module(sio2);
    (void)ef2_iop_load_module(mcman);
    (void)ef2_iop_load_module(mcserv);
}

int ef2_mc_init(void)
{
    int result;

    if (g_mc_initialized)
        return 0;

    result = ef2_sif_init();
    if (result < 0)
        return -100 + result;

    /*
     * Prefer the newer ROM X modules. Their MCSERV remains compatible with
     * the classic 0x70/0x78 commands used below. If the BIOS does not carry
     * them, fall back to the classic ROM modules.
     */
    ef2_mc_try_load_rom_modules(
        "rom0:XSIO2MAN",
        "rom0:XMCMAN",
        "rom0:XMCSERV");

    result = ef2_mc_bind();
    if (result < 0) {
        ef2_mc_try_load_rom_modules(
            "rom0:SIO2MAN",
            "rom0:MCMAN",
            "rom0:MCSERV");

        result = ef2_mc_bind();
        if (result < 0)
            return -200 + result;
    }

    ef2_mc_zero(&g_mc_desc, sizeof(g_mc_desc));
    ef2_mc_zero(g_mc_reply, sizeof(g_mc_reply));

    /*
     * -217 is the classic libmc/MCSERV initialization marker. Newer XMCSERV
     * accepts the same command for compatibility.
     */
    g_mc_desc.offset = -217;

    result = ef2_sif_call(
        &g_mc_client,
        EF2_MCSERV_CMD_INIT,
        &g_mc_desc,
        sizeof(g_mc_desc),
        g_mc_reply,
        sizeof(ef2_s32));
    if (result < 0)
        return -300 + result;

    if (g_mc_reply[0] < 0)
        return -400 + g_mc_reply[0];

    g_mc_initialized = 1;
    return 0;
}

int ef2_mc_get_info(
    ef2_s32 port,
    ef2_s32 slot,
    ef2_mc_info *info)
{
    const ef2_mc_end_param *end;
    int result;

    if (info == (ef2_mc_info *)0)
        return -1000;

    info->result = -1000;
    info->type = EF2_MC_TYPE_NONE;
    info->free_clusters = 0;
    info->formatted = 0;

    if (!g_mc_initialized) {
        result = ef2_mc_init();
        if (result < 0) {
            info->result = result;
            return result;
        }
    }

    if (port < 0 || port > 1 || slot < 0)
        return -1001;

    ef2_mc_zero(&g_mc_desc, sizeof(g_mc_desc));
    ef2_mc_zero(&g_mc_end, sizeof(g_mc_end));
    ef2_mc_zero(g_mc_reply, sizeof(g_mc_reply));

    g_mc_desc.port = port;
    g_mc_desc.slot = slot;
    g_mc_desc.size = 1;
    g_mc_desc.offset = 1;
    g_mc_desc.origin = 1;
    g_mc_desc.param = &g_mc_end;

    ef2_cache_writeback_invalidate_range(
        &g_mc_end,
        sizeof(g_mc_end));

    result = ef2_sif_call(
        &g_mc_client,
        EF2_MCSERV_CMD_GET_INFO,
        &g_mc_desc,
        sizeof(g_mc_desc),
        g_mc_reply,
        sizeof(ef2_s32));
    if (result < 0) {
        info->result = -1100 + result;
        return info->result;
    }

    end = ef2_mc_end_uncached();

    info->result = g_mc_reply[0];
    info->type = end->type;
    info->free_clusters = end->free_clusters;
    info->formatted =
        (end->type == EF2_MC_TYPE_NONE ||
         g_mc_reply[0] == -2)
            ? 0
            : 1;

    return info->result;
}
