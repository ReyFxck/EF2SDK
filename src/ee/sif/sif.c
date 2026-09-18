#include <ef2/cache.h>
#include <ef2/kernel.h>
#include <ef2/sif.h>

#define EF2_SIF_CMD_SYSTEM          0x80000000u
#define EF2_SIF_CMD_CHANGE_SADDR   (EF2_SIF_CMD_SYSTEM | 0u)
#define EF2_SIF_CMD_SET_SREG       (EF2_SIF_CMD_SYSTEM | 1u)
#define EF2_SIF_CMD_INIT_CMD       (EF2_SIF_CMD_SYSTEM | 2u)
#define EF2_SIF_CMD_RPC_END        (EF2_SIF_CMD_SYSTEM | 8u)
#define EF2_SIF_CMD_RPC_BIND       (EF2_SIF_CMD_SYSTEM | 9u)
#define EF2_SIF_CMD_RPC_CALL       (EF2_SIF_CMD_SYSTEM | 10u)

#define EF2_SIF_REG_ID_SYSTEM      0x80000000u
#define EF2_SIF_REG_SUBADDR        2u
#define EF2_SIF_REG_SMFLAG         4u
#define EF2_SIF_SYSREG_SUBADDR     (EF2_SIF_REG_ID_SYSTEM | 0u)
#define EF2_SIF_SYSREG_MAINADDR    (EF2_SIF_REG_ID_SYSTEM | 1u)
#define EF2_SIF_SYSREG_RPCINIT     (EF2_SIF_REG_ID_SYSTEM | 2u)

#define EF2_SIF_STAT_CMDINIT       0x00020000u
#define EF2_SIF_SREG_RPCINIT       0u
#define EF2_SIF_DMA_ERT            0x40
#define EF2_SIF_DMA_INT_O          0x04

#define EF2_DMAC_SIF0              5
#define EF2_DMAC_COMM_STAT         (*(volatile ef2_u32 *)0x1000E010u)
#define EF2_DMAC_SIF0_CHCR         (*(volatile ef2_u32 *)0x1000C000u)
#define EF2_DMAC_STAT_SIF0         0x20u
#define EF2_DMAC_CHCR_STR          0x100u

#define EF2_RPC_PACKET_SIZE        64u
#define EF2_RPC_PACKET_COUNT       8u
#define EF2_RPC_ALLOC              0x01
#define EF2_RPC_REC_REQUEST        0x04

#define EF2_IOP_HEAP_SID           0x80000003u
#define EF2_LOADFILE_SID           0x80000006u
#define EF2_LF_MOD_LOAD            0
#define EF2_LF_MOD_BUF_LOAD        6
#define EF2_LF_PATH_MAX            252
#define EF2_LF_ARG_MAX             252

typedef struct {
    ef2_u32 sizes;
    void *dest;
    ef2_s32 cid;
    ef2_u32 opt;
} ef2_sif_cmd_header;

typedef void (*ef2_sif_cmd_handler)(void *packet, void *arg);

typedef struct {
    ef2_sif_cmd_handler handler;
    void *arg;
    void *unused;
} ef2_sif_sys_handler;

typedef struct {
    void *pktbuf;
    void *unused;
    void *iopbuf;
    ef2_sif_sys_handler *sys_handlers;
    ef2_u32 sys_handler_count;
    void *user_handlers;
    ef2_u32 user_handler_count;
    ef2_s32 *sregs;
} ef2_sif_cmd_data;

typedef struct {
    ef2_sif_cmd_header header;
    void *buf;
} ef2_sif_change_addr_packet;

typedef struct {
    ef2_sif_cmd_header header;
    ef2_u32 sreg;
    ef2_s32 value;
} ef2_sif_sreg_packet;

typedef struct {
    ef2_sif_cmd_header sifcmd;
    ef2_s32 rec_id;
    void *pkt_addr;
    ef2_s32 rpc_id;
} ef2_rpc_packet_header;

typedef struct {
    ef2_sif_cmd_header sifcmd;
    ef2_s32 rec_id;
    void *pkt_addr;
    ef2_s32 rpc_id;
    ef2_sif_rpc_client *client;
    ef2_u32 cid;
    void *server;
    void *buf;
    void *cbuf;
} ef2_rpc_end_packet;

typedef struct {
    ef2_sif_cmd_header sifcmd;
    ef2_s32 rec_id;
    void *pkt_addr;
    ef2_s32 rpc_id;
    ef2_sif_rpc_client *client;
    ef2_s32 sid;
} ef2_rpc_bind_packet;

typedef struct {
    ef2_sif_cmd_header sifcmd;
    ef2_s32 rec_id;
    void *pkt_addr;
    ef2_s32 rpc_id;
    ef2_sif_rpc_client *client;
    ef2_s32 rpc_number;
    ef2_s32 send_size;
    void *recvbuf;
    ef2_s32 recv_size;
    ef2_s32 rmode;
    void *server;
} ef2_rpc_call_packet;

typedef struct {
    union {
        ef2_s32 arg_len;
        ef2_s32 result;
    } p;
    ef2_s32 modres;
    char path[EF2_LF_PATH_MAX];
    char args[EF2_LF_ARG_MAX];
} EF2_ALIGN(64) ef2_lf_module_load_arg;

typedef struct {
    union {
        void *ptr;
        ef2_s32 result;
    } p;
    union {
        ef2_s32 arg_len;
        ef2_s32 modres;
    } q;
    char unused[EF2_LF_PATH_MAX];
    char args[EF2_LF_ARG_MAX];
} EF2_ALIGN(64) ef2_lf_module_buffer_arg;

static ef2_u8 g_cmd_packet_buffer[128] EF2_ALIGN(64);
static ef2_u8 g_cmd_unused[64] EF2_ALIGN(64);
static ef2_sif_sys_handler g_sys_handlers[32];
static ef2_s32 g_sregs[32];
static ef2_sif_cmd_data g_cmd_data EF2_ALIGN(64);
static ef2_s32 g_sif0_handler_id = -1;
static ef2_s32 g_sif_initialized;

static ef2_u8 g_rpc_packets[EF2_RPC_PACKET_COUNT][EF2_RPC_PACKET_SIZE]
    EF2_ALIGN(64);
static ef2_s32 g_rpc_next_id = 1;
static volatile ef2_s32 g_rpc_wait_done;

static ef2_sif_rpc_client g_heap_client;
static ef2_sif_rpc_client g_loadfile_client;
static ef2_s32 g_heap_bound;
static ef2_s32 g_loadfile_bound;

static ef2_lf_module_load_arg g_load_arg;
static ef2_lf_module_buffer_arg g_load_buffer_arg;
static ef2_u32 g_heap_arg[4] EF2_ALIGN(64);

static void *ef2_uncached(void *ptr)
{
    return (void *)((ef2_u32)ptr | 0x20000000u);
}

static void ef2_zero(void *ptr, ef2_u32 size)
{
    ef2_u8 *p = (ef2_u8 *)ptr;
    ef2_u32 i;

    for (i = 0; i < size; ++i)
        p[i] = 0;
}

static void ef2_copy_string(char *dest, const char *src, ef2_u32 capacity)
{
    ef2_u32 i = 0;

    if (capacity == 0)
        return;

    while (i + 1u < capacity && src[i] != '\0') {
        dest[i] = src[i];
        ++i;
    }

    dest[i] = '\0';
}

static void ef2_cmd_set_sizes(
    ef2_sif_cmd_header *header,
    ef2_u32 packet_size,
    ef2_u32 data_size)
{
    header->sizes = (packet_size & 0xFFu) | ((data_size & 0xFFFFFFu) << 8);
}

static ef2_u32 ef2_cmd_packet_size(const ef2_sif_cmd_header *header)
{
    return header->sizes & 0xFFu;
}

static ef2_u32 ef2_sif_send_cmd(
    ef2_u32 cid,
    void *packet,
    ef2_u32 packet_size,
    void *extra_src,
    void *extra_dest,
    ef2_u32 extra_size)
{
    ef2_sif_dma_transfer transfers[2] EF2_ALIGN(16);
    ef2_sif_cmd_header *header = (ef2_sif_cmd_header *)packet;
    ef2_s32 count = 0;

    if (packet_size == 0 || packet_size > 112u || g_cmd_data.iopbuf == (void *)0)
        return 0;

    header->cid = (ef2_s32)cid;
    header->dest = (void *)0;
    ef2_cmd_set_sizes(header, packet_size, 0);

    if (extra_size != 0) {
        header->dest = extra_dest;
        ef2_cmd_set_sizes(header, packet_size, extra_size);
        ef2_cache_writeback_invalidate_range(extra_src, extra_size);

        transfers[count].src = extra_src;
        transfers[count].dest = extra_dest;
        transfers[count].size = (ef2_s32)extra_size;
        transfers[count].attr = 0;
        ++count;
    }

    transfers[count].src = packet;
    transfers[count].dest = g_cmd_data.iopbuf;
    transfers[count].size = (ef2_s32)packet_size;
    transfers[count].attr = EF2_SIF_DMA_ERT | EF2_SIF_DMA_INT_O;
    ++count;

    ef2_cache_writeback_invalidate_range(packet, packet_size);
    return (ef2_u32)ef2_kernel_sif_set_dma(transfers, count);
}

static void ef2_cmd_change_addr(void *packet, void *arg)
{
    ef2_sif_change_addr_packet *change =
        (ef2_sif_change_addr_packet *)packet;
    ((ef2_sif_cmd_data *)arg)->iopbuf = change->buf;
}

static void ef2_cmd_set_sreg(void *packet, void *arg)
{
    ef2_sif_sreg_packet *set = (ef2_sif_sreg_packet *)packet;
    ef2_sif_cmd_data *data = (ef2_sif_cmd_data *)arg;

    if (set->sreg < 32u)
        data->sregs[set->sreg] = set->value;
}

static void ef2_rpc_packet_free(void *packet)
{
    ef2_rpc_packet_header *header = (ef2_rpc_packet_header *)packet;
    header->rpc_id = 0;
    header->rec_id &= ~EF2_RPC_ALLOC;
}

static void ef2_cmd_rpc_end(void *packet, void *arg)
{
    ef2_rpc_end_packet *end = (ef2_rpc_end_packet *)packet;
    ef2_sif_rpc_client *client = end->client;

    (void)arg;

    if (client != (ef2_sif_rpc_client *)0) {
        if (end->cid == EF2_SIF_CMD_RPC_BIND) {
            client->server = end->server;
            client->buf = end->buf;
            client->cbuf = end->cbuf;
        }

        if (client->hdr.pkt_addr != (void *)0) {
            ef2_rpc_packet_free(client->hdr.pkt_addr);
            client->hdr.pkt_addr = (void *)0;
        }
    }

    g_rpc_wait_done = 1;
}

static ef2_s32 ef2_sif_irq_handler(ef2_s32 channel)
{
    ef2_u32 packet_words[32] EF2_ALIGN(16);
    ef2_sif_cmd_header *source =
        (ef2_sif_cmd_header *)g_cmd_data.pktbuf;
    ef2_sif_cmd_header *header = (ef2_sif_cmd_header *)packet_words;
    ef2_u32 size;
    ef2_u32 words;
    ef2_u32 i;
    ef2_u32 id;

    (void)channel;
    __asm__ volatile("ei" ::: "memory");

    size = ef2_cmd_packet_size(source);
    if (size == 0 || size > 128u)
        goto done;

    source->sizes &= ~0xFFu;
    words = (size + 3u) >> 2;

    for (i = 0; i < words; ++i)
        packet_words[i] = ((volatile ef2_u32 *)source)[i];

    ef2_kernel_isif_set_dchain();

    id = ((ef2_u32)header->cid) & ~EF2_SIF_CMD_SYSTEM;
    if ((((ef2_u32)header->cid) & EF2_SIF_CMD_SYSTEM) != 0 &&
        id < g_cmd_data.sys_handler_count &&
        g_cmd_data.sys_handlers[id].handler != (ef2_sif_cmd_handler)0) {
        g_cmd_data.sys_handlers[id].handler(
            packet_words,
            g_cmd_data.sys_handlers[id].arg);
    }

done:
    __asm__ volatile("sync\n\tei" ::: "memory");
    return 0;
}

static int ef2_wait_flag(volatile ef2_s32 *flag)
{
    ef2_u32 outer;

    for (outer = 0; outer < 0x02000000u; ++outer) {
        if (*flag != 0)
            return 0;
        __asm__ volatile("nop");
    }

    return -1;
}

static void *ef2_rpc_alloc_packet(void)
{
    ef2_u32 i;

    __asm__ volatile("di" ::: "memory");

    for (i = 0; i < EF2_RPC_PACKET_COUNT; ++i) {
        ef2_rpc_packet_header *packet =
            (ef2_rpc_packet_header *)ef2_uncached(&g_rpc_packets[i][0]);

        if ((packet->rec_id & EF2_RPC_ALLOC) == 0) {
            ef2_s32 id = g_rpc_next_id++;

            if (g_rpc_next_id == 0)
                g_rpc_next_id = 1;

            ef2_zero(packet, EF2_RPC_PACKET_SIZE);
            packet->rec_id =
                (ef2_s32)((i << 16) | EF2_RPC_REC_REQUEST | EF2_RPC_ALLOC);
            packet->rpc_id = id;
            packet->pkt_addr = packet;

            __asm__ volatile("ei" ::: "memory");
            return packet;
        }
    }

    __asm__ volatile("ei" ::: "memory");
    return (void *)0;
}

static int ef2_sif_init_cmd(void)
{
    ef2_sif_change_addr_packet change EF2_ALIGN(64);
    ef2_u32 i;

    if (g_sif0_handler_id >= 0)
        return 0;

    ef2_zero(&g_cmd_data, sizeof(g_cmd_data));
    ef2_zero(g_sys_handlers, sizeof(g_sys_handlers));
    ef2_zero(g_sregs, sizeof(g_sregs));
    ef2_zero(g_cmd_packet_buffer, sizeof(g_cmd_packet_buffer));

    g_cmd_data.pktbuf = ef2_uncached(g_cmd_packet_buffer);
    g_cmd_data.unused = ef2_uncached(g_cmd_unused);
    g_cmd_data.sys_handlers = g_sys_handlers;
    g_cmd_data.sys_handler_count = 32;
    g_cmd_data.sregs = g_sregs;

    g_sys_handlers[0].handler = ef2_cmd_change_addr;
    g_sys_handlers[0].arg = &g_cmd_data;
    g_sys_handlers[1].handler = ef2_cmd_set_sreg;
    g_sys_handlers[1].arg = &g_cmd_data;
    g_sys_handlers[8].handler = ef2_cmd_rpc_end;
    g_sys_handlers[8].arg = &g_cmd_data;

    ef2_kernel_flush_cache(0);

    if ((EF2_DMAC_COMM_STAT & EF2_DMAC_STAT_SIF0) != 0)
        EF2_DMAC_COMM_STAT = EF2_DMAC_STAT_SIF0;

    if ((EF2_DMAC_SIF0_CHCR & EF2_DMAC_CHCR_STR) == 0)
        ef2_kernel_sif_set_dchain();

    g_sif0_handler_id =
        ef2_kernel_add_dmac_handler(EF2_DMAC_SIF0, ef2_sif_irq_handler, 0);
    if (g_sif0_handler_id < 0)
        return -1;

    if (ef2_kernel_enable_dmac(EF2_DMAC_SIF0) < 0)
        return -2;

    g_cmd_data.iopbuf =
        (void *)(ef2_u32)ef2_kernel_sif_get_reg(EF2_SIF_SYSREG_SUBADDR);

    if (g_cmd_data.iopbuf != (void *)0) {
        ef2_zero(&change, sizeof(change));
        change.buf = g_cmd_data.pktbuf;

        if (ef2_sif_send_cmd(
                EF2_SIF_CMD_CHANGE_SADDR,
                &change,
                sizeof(change),
                (void *)0,
                (void *)0,
                0) == 0)
            return -3;

        return 0;
    }

    for (i = 0; i < 0x02000000u; ++i) {
        if (((ef2_u32)ef2_kernel_sif_get_reg(EF2_SIF_REG_SMFLAG) &
             EF2_SIF_STAT_CMDINIT) != 0)
            break;
    }

    if (i == 0x02000000u)
        return -4;

    g_cmd_data.iopbuf =
        (void *)(ef2_u32)ef2_kernel_sif_get_reg(EF2_SIF_REG_SUBADDR);
    if (g_cmd_data.iopbuf == (void *)0)
        return -5;

    ef2_kernel_sif_set_reg(
        EF2_SIF_SYSREG_SUBADDR,
        (ef2_s32)(ef2_u32)g_cmd_data.iopbuf);
    ef2_kernel_sif_set_reg(
        EF2_SIF_SYSREG_MAINADDR,
        (ef2_s32)(ef2_u32)&g_cmd_data);

    ef2_zero(&change, sizeof(change));
    change.header.opt = 0;
    change.buf = g_cmd_data.pktbuf;

    if (ef2_sif_send_cmd(
            EF2_SIF_CMD_INIT_CMD,
            &change,
            sizeof(change),
            (void *)0,
            (void *)0,
            0) == 0)
        return -6;

    return 0;
}

static int ef2_sif_init_rpc(void)
{
    ef2_sif_cmd_header init_packet EF2_ALIGN(64);

    if (ef2_kernel_sif_get_reg(EF2_SIF_SYSREG_RPCINIT) != 0)
        return 0;

    ef2_zero(&init_packet, sizeof(init_packet));
    init_packet.opt = 1;

    if (ef2_sif_send_cmd(
            EF2_SIF_CMD_INIT_CMD,
            &init_packet,
            sizeof(init_packet),
            (void *)0,
            (void *)0,
            0) == 0)
        return -1;

    if (ef2_wait_flag((volatile ef2_s32 *)&g_sregs[EF2_SIF_SREG_RPCINIT]) < 0)
        return -2;

    ef2_kernel_sif_set_reg(EF2_SIF_SYSREG_RPCINIT, 1);
    return 0;
}

int ef2_sif_init(void)
{
    ef2_u32 i;
    int result;

    if (g_sif_initialized)
        return 0;

    for (i = 0; i < EF2_RPC_PACKET_COUNT; ++i) {
        ef2_rpc_packet_header *packet =
            (ef2_rpc_packet_header *)ef2_uncached(&g_rpc_packets[i][0]);
        packet->rec_id = 0;
        packet->rpc_id = 0;
    }

    result = ef2_sif_init_cmd();
    if (result < 0)
        return result - 100;

    result = ef2_sif_init_rpc();
    if (result < 0)
        return result - 200;

    g_sif_initialized = 1;
    return 0;
}

int ef2_sif_bind(ef2_sif_rpc_client *client, ef2_u32 sid)
{
    ef2_u32 attempt;

    if (client == (ef2_sif_rpc_client *)0)
        return -1;

    for (attempt = 0; attempt < 4096u; ++attempt) {
        ef2_rpc_bind_packet *bind =
            (ef2_rpc_bind_packet *)ef2_rpc_alloc_packet();

        if (bind == (ef2_rpc_bind_packet *)0)
            return -2;

        client->command = 0;
        client->server = (void *)0;
        client->buf = (void *)0;
        client->cbuf = (void *)0;
        client->hdr.pkt_addr = bind;
        client->hdr.rpc_id = (ef2_u32)bind->rpc_id;
        client->hdr.sema_id = -1;

        bind->client = client;
        bind->sid = (ef2_s32)sid;
        g_rpc_wait_done = 0;

        if (ef2_sif_send_cmd(
                EF2_SIF_CMD_RPC_BIND,
                bind,
                EF2_RPC_PACKET_SIZE,
                (void *)0,
                (void *)0,
                0) == 0) {
            ef2_rpc_packet_free(bind);
            client->hdr.pkt_addr = (void *)0;
            return -3;
        }

        if (ef2_wait_flag(&g_rpc_wait_done) < 0)
            return -4;

        if (client->server != (void *)0)
            return 0;
    }

    return -5;
}

int ef2_sif_call(
    ef2_sif_rpc_client *client,
    ef2_s32 function,
    void *send_buffer,
    ef2_u32 send_size,
    void *receive_buffer,
    ef2_u32 receive_size)
{
    ef2_rpc_call_packet *call;

    if (client == (ef2_sif_rpc_client *)0 || client->server == (void *)0)
        return -1;

    call = (ef2_rpc_call_packet *)ef2_rpc_alloc_packet();
    if (call == (ef2_rpc_call_packet *)0)
        return -2;

    client->hdr.pkt_addr = call;
    client->hdr.rpc_id = (ef2_u32)call->rpc_id;
    client->hdr.sema_id = -1;

    call->client = client;
    call->rpc_number = function;
    call->send_size = (ef2_s32)send_size;
    call->recvbuf = receive_buffer;
    call->recv_size = (ef2_s32)receive_size;
    call->rmode = 1;
    call->server = client->server;

    if (send_size != 0)
        ef2_cache_writeback_invalidate_range(send_buffer, send_size);
    if (receive_size != 0)
        ef2_cache_writeback_invalidate_range(receive_buffer, receive_size);

    g_rpc_wait_done = 0;

    if (ef2_sif_send_cmd(
            EF2_SIF_CMD_RPC_CALL,
            call,
            EF2_RPC_PACKET_SIZE,
            send_buffer,
            client->buf,
            send_size) == 0) {
        ef2_rpc_packet_free(call);
        client->hdr.pkt_addr = (void *)0;
        return -3;
    }

    if (ef2_wait_flag(&g_rpc_wait_done) < 0)
        return -4;

    return 0;
}

static int ef2_bind_heap(void)
{
    if (!g_heap_bound) {
        ef2_zero(&g_heap_client, sizeof(g_heap_client));
        if (ef2_sif_bind(&g_heap_client, EF2_IOP_HEAP_SID) < 0)
            return -1;
        g_heap_bound = 1;
    }
    return 0;
}

static int ef2_bind_loadfile(void)
{
    if (!g_loadfile_bound) {
        ef2_zero(&g_loadfile_client, sizeof(g_loadfile_client));
        if (ef2_sif_bind(&g_loadfile_client, EF2_LOADFILE_SID) < 0)
            return -1;
        g_loadfile_bound = 1;
    }
    return 0;
}

static void *ef2_iop_alloc(ef2_u32 size)
{
    if (ef2_bind_heap() < 0)
        return (void *)0;

    ef2_zero(g_heap_arg, sizeof(g_heap_arg));
    g_heap_arg[0] = size;

    if (ef2_sif_call(
            &g_heap_client, 1,
            g_heap_arg, 4,
            g_heap_arg, 4) < 0)
        return (void *)0;

    return (void *)g_heap_arg[0];
}

static int ef2_iop_free(void *ptr)
{
    if (ef2_bind_heap() < 0)
        return -1;

    ef2_zero(g_heap_arg, sizeof(g_heap_arg));
    g_heap_arg[0] = (ef2_u32)ptr;

    if (ef2_sif_call(
            &g_heap_client, 2,
            g_heap_arg, 4,
            g_heap_arg, 4) < 0)
        return -2;

    return (ef2_s32)g_heap_arg[0];
}

static int ef2_sif_dma_to_iop(const void *source, void *dest, ef2_u32 size)
{
    ef2_sif_dma_transfer transfer EF2_ALIGN(16);
    ef2_s32 id;

    transfer.src = (void *)source;
    transfer.dest = dest;
    transfer.size = (ef2_s32)size;
    transfer.attr = 0;

    ef2_cache_writeback_invalidate_range((void *)source, size);

    id = ef2_kernel_sif_set_dma(&transfer, 1);
    if (id <= 0)
        return -1;

    while (ef2_kernel_sif_dma_stat(id) >= 0)
        __asm__ volatile("nop");

    return 0;
}


#define EF2_IOP_WINDOW_BASE 0xBC000000u

typedef struct {
    ef2_u32 next;
    ef2_u32 name;
    ef2_u16 version;
    ef2_u16 newflags;
    ef2_u16 id;
    ef2_u16 unused;
    ef2_u32 entry;
    ef2_u32 gp;
    ef2_u32 text_start;
    ef2_u32 text_size;
    ef2_u32 data_size;
    ef2_u32 bss_size;
    ef2_u32 unused1;
    ef2_u32 unused2;
} ef2_iop_module_info;

typedef struct {
    ef2_u32 prev;
    ef2_u32 caller;
    ef2_u16 version;
    ef2_u16 flags;
    ef2_u8 name[8];
    ef2_u32 exports[20];
} ef2_iop_export_lib;

static int ef2_bytes_match(
    const ef2_u8 *left,
    const char *right,
    ef2_u32 count)
{
    ef2_u32 i;

    for (i = 0; i < count; ++i) {
        if (left[i] != (ef2_u8)right[i])
            return 0;
    }

    return 1;
}

static void ef2_iop_window_enter(void)
{
    ef2_u32 status;

    __asm__ volatile(
        "di\n\t"
        "sync.p\n\t"
        "mfc0 %0, $12\n\t"
        : "=r"(status)
        :
        : "memory");

    status &= ~0x18u;

    __asm__ volatile(
        "mtc0 %0, $12\n\t"
        "sync.p\n\t"
        :
        : "r"(status)
        : "memory");
}

static void ef2_iop_window_exit(void)
{
    ef2_u32 status;

    __asm__ volatile(
        "mfc0 %0, $12\n\t"
        : "=r"(status)
        :
        : "memory");

    status |= 0x10u;

    __asm__ volatile(
        "mtc0 %0, $12\n\t"
        "sync.p\n\t"
        "ei\n\t"
        :
        : "r"(status)
        : "memory");
}

static int ef2_iop_read(
    ef2_u32 iop_address,
    void *destination,
    ef2_u32 size)
{
    volatile const ef2_u8 *source;
    ef2_u8 *dest = (ef2_u8 *)destination;
    ef2_u32 i;

    if (destination == (void *)0 || size == 0)
        return -1;

    source = (volatile const ef2_u8 *)(EF2_IOP_WINDOW_BASE + iop_address);

    ef2_iop_window_enter();
    for (i = 0; i < size; ++i)
        dest[i] = source[i];
    ef2_iop_window_exit();

    return 0;
}

static int ef2_iop_find_module(
    const char *name,
    ef2_u32 name_length,
    ef2_iop_module_info *found)
{
    ef2_u32 address = 0x800u;
    ef2_u32 pass;

    for (pass = 0; pass < 128u; ++pass) {
        ef2_iop_module_info info;
        ef2_u8 module_name[64];

        if (ef2_iop_read(address, &info, sizeof(info)) < 0)
            return -1;

        if (info.name != 0) {
            if (ef2_iop_read(info.name, module_name, sizeof(module_name)) < 0)
                return -2;

            if (name_length < sizeof(module_name) &&
                ef2_bytes_match(module_name, name, name_length) &&
                module_name[name_length] == 0) {
                *found = info;
                return 0;
            }
        }

        if (info.next == 0)
            break;

        address = info.next;
    }

    return -3;
}

static int ef2_iop_find_modload_exports(
    ef2_u32 *start_module,
    ef2_u32 *load_module_buffer)
{
    ef2_iop_module_info list_head;
    ef2_iop_module_info loadcore;
    ef2_u8 scan[512] EF2_ALIGN(64);
    ef2_u32 loadcore_end;
    ef2_u32 get_internal = 0;
    ef2_u32 internal_code[2];
    ef2_u32 export_list_addr;
    ef2_u32 export_list[2];
    ef2_u32 current;
    ef2_u32 i;
    ef2_u32 pass;

    if (ef2_iop_read(0x800u, &list_head, sizeof(list_head)) < 0)
        return -1;

    if (list_head.next == 0)
        return -2;

    if (ef2_iop_read(list_head.next, &loadcore, sizeof(loadcore)) < 0)
        return -3;

    loadcore_end = loadcore.text_start + loadcore.text_size;
    if (loadcore_end < 512u)
        return -4;

    if (ef2_iop_read(loadcore_end - 512u, scan, sizeof(scan)) < 0)
        return -5;

    for (i = 0; i + 88u <= sizeof(scan); i += 4u) {
        ef2_u32 previous = *(const ef2_u32 *)(scan + i);

        if (previous == 0x830u &&
            ef2_bytes_match(scan + i + 12u, "loadcore", 8u)) {
            const ef2_u32 *exports =
                (const ef2_u32 *)(scan + i + 20u);
            get_internal = exports[3];
            break;
        }
    }

    if (get_internal == 0)
        return -6;

    if (ef2_iop_read(get_internal, internal_code, sizeof(internal_code)) < 0)
        return -7;

    if ((internal_code[0] & 0xFFFF0000u) != 0x3C020000u ||
        (internal_code[1] & 0xFFFF0000u) != 0x24420000u)
        return -8;

    export_list_addr =
        ((internal_code[0] & 0xFFFFu) << 16) +
        (ef2_s32)(ef2_s16)(internal_code[1] & 0xFFFFu);

    if (ef2_iop_read(export_list_addr, export_list, sizeof(export_list)) < 0)
        return -9;

    current = export_list[0];

    for (pass = 0; pass < 128u && current != 0; ++pass) {
        ef2_iop_export_lib library;

        if (ef2_iop_read(current, &library, sizeof(library)) < 0)
            return -10;

        if (ef2_bytes_match(library.name, "modload", 7u)) {
            if (library.exports[8] == 0 || library.exports[10] == 0)
                return -11;

            *start_module = library.exports[8];
            *load_module_buffer = library.exports[10];
            return 0;
        }

        current = library.prev;
    }

    return -12;
}

static int ef2_iop_write_word_dma(
    ef2_u32 iop_address,
    ef2_u32 value)
{
    ef2_u8 block[64] EF2_ALIGN(64);
    ef2_u32 aligned = iop_address & ~63u;
    ef2_u32 offset = iop_address & 63u;

    if (offset > 60u)
        return -1;

    if (ef2_iop_read(aligned, block, sizeof(block)) < 0)
        return -2;

    *(ef2_u32 *)(block + offset) = value;

    return ef2_sif_dma_to_iop(
        block,
        (void *)aligned,
        sizeof(block));
}

int ef2_iop_enable_module_buffer(void)
{
    static const ef2_u32 patch_template[32] = {
        0x27BDFFD8u, 0xAFB00018u, 0xAFBF0020u, 0x00808021u,
        0x8C840000u, 0x0C000000u, 0xAFB1001Cu, 0x3C110000u,
        0x04400008u, 0x36310000u, 0x00402021u, 0x26250008u,
        0x8E060004u, 0x26070104u, 0x26280004u, 0x0C000000u,
        0xAFA80010u, 0xAE220000u, 0x02201021u, 0x8FBF0020u,
        0x8FB1001Cu, 0x8FB00018u, 0x03E00008u, 0x27BD0028u,
        0x00000000u, 0x00000000u, 0x7962424Cu, 0x00004545u,
        0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u
    };
    ef2_iop_module_info loadfile;
    ef2_u32 dispatch[32] EF2_ALIGN(64);
    ef2_u32 patch[32] EF2_ALIGN(64);
    ef2_u32 start_module;
    ef2_u32 load_module_buffer;
    ef2_u32 dispatch_address;
    ef2_u32 jump_table_end;
    ef2_u32 result_address;
    void *patch_address;
    ef2_u32 i;
    int result;

    result = ef2_iop_find_modload_exports(
        &start_module,
        &load_module_buffer);
    if (result < 0)
        return -100 + result;

    result = ef2_iop_find_module(
        "LoadModuleByEE",
        14u,
        &loadfile);
    if (result < 0)
        return -200 + result;

    if (loadfile.text_size < 0x544u)
        return -301;

    dispatch_address = loadfile.text_start + 0x4C4u;

    if (ef2_iop_read(
            dispatch_address,
            dispatch,
            sizeof(dispatch)) < 0)
        return -302;

    /*
     * Legacy ROM LOADFILE checks function < 6. If it already accepts
     * function 6, nothing needs patching.
     */
    if (dispatch[1] == 0x2C820007u)
        return 0;

    if (dispatch[0] != 0x27BDFFE8u ||
        dispatch[1] != 0x2C820006u ||
        dispatch[2] != 0x14400003u ||
        dispatch[3] != 0xAFBF0010u ||
        dispatch[5] != 0x00001021u ||
        dispatch[6] != 0x00041080u)
        return -303;

    jump_table_end =
        ((dispatch[7] & 0xFFFFu) << 16) +
        (ef2_s32)(ef2_s16)(dispatch[9] & 0xFFFFu) +
        0x18u;

    patch_address = ef2_iop_alloc(sizeof(patch));
    if (patch_address == (void *)0)
        return -304;

    for (i = 0; i < 32u; ++i)
        patch[i] = patch_template[i];

    result_address = (ef2_u32)patch_address + 96u;

    patch[5] =
        0x0C000000u |
        ((load_module_buffer >> 2) & 0x03FFFFFFu);
    patch[7] =
        0x3C110000u |
        ((result_address >> 16) & 0xFFFFu);
    patch[9] =
        0x36310000u |
        (result_address & 0xFFFFu);
    patch[15] =
        0x0C000000u |
        ((start_module >> 2) & 0x03FFFFFFu);

    if (ef2_sif_dma_to_iop(
            patch,
            patch_address,
            sizeof(patch)) < 0)
        return -305;

    if (ef2_iop_write_word_dma(
            jump_table_end,
            (ef2_u32)patch_address) < 0)
        return -306;

    if (ef2_iop_write_word_dma(
            dispatch_address + 4u,
            0x2C820007u) < 0)
        return -307;

    return 0;
}

int ef2_iop_load_module_ex(const char *path, ef2_s32 *module_result)
{
    if (module_result != (ef2_s32 *)0)
        *module_result = -1;

    if (path == (const char *)0)
        return -1;

    if (ef2_bind_loadfile() < 0)
        return -2;

    ef2_zero(&g_load_arg, sizeof(g_load_arg));
    ef2_copy_string(g_load_arg.path, path, sizeof(g_load_arg.path));
    g_load_arg.p.arg_len = 0;

    if (ef2_sif_call(
            &g_loadfile_client,
            EF2_LF_MOD_LOAD,
            &g_load_arg,
            sizeof(g_load_arg),
            &g_load_arg,
            8) < 0)
        return -3;

    if (module_result != (ef2_s32 *)0)
        *module_result = g_load_arg.modres;

    return g_load_arg.p.result;
}

int ef2_iop_load_module(const char *path)
{
    return ef2_iop_load_module_ex(path, (ef2_s32 *)0);
}

int ef2_iop_exec_module_buffer_ex(
    const void *module,
    ef2_u32 size,
    ef2_s32 *module_result)
{
    void *iop_address;
    ef2_u32 allocation_size;
    int result;

    if (module_result != (ef2_s32 *)0)
        *module_result = -1;

    if (module == (const void *)0 || size == 0)
        return -1;

    if (ef2_bind_loadfile() < 0 || ef2_bind_heap() < 0)
        return -2;

    allocation_size = (size + 15u) & ~15u;
    iop_address = ef2_iop_alloc(allocation_size);
    if (iop_address == (void *)0)
        return -3;

    if (ef2_sif_dma_to_iop(module, iop_address, size) < 0) {
        ef2_iop_free(iop_address);
        return -4;
    }

    ef2_zero(&g_load_buffer_arg, sizeof(g_load_buffer_arg));
    g_load_buffer_arg.p.ptr = iop_address;
    g_load_buffer_arg.q.arg_len = 0;

    result = ef2_sif_call(
        &g_loadfile_client,
        EF2_LF_MOD_BUF_LOAD,
        &g_load_buffer_arg,
        sizeof(g_load_buffer_arg),
        &g_load_buffer_arg,
        8);

    /*
     * Legacy LOADFILE versions simply have no dispatch entry for function 6.
     * SIFRPC still completes the call, but no reply payload is written back.
     * Since p.ptr is also the first result word, an untouched input buffer
     * used to look like a successful positive module id, while q.arg_len == 0
     * looked exactly like MODULE_RESIDENT_END. Detect that false-success case
     * by checking whether the request pointer survived unchanged.
     */
    if (result >= 0 && g_load_buffer_arg.p.ptr == iop_address)
        result = -6;

    ef2_iop_free(iop_address);

    if (result < 0)
        return result == -6 ? -6 : -5;

    if (module_result != (ef2_s32 *)0)
        *module_result = g_load_buffer_arg.q.modres;

    return g_load_buffer_arg.p.result;
}

int ef2_iop_exec_module_buffer(const void *module, ef2_u32 size)
{
    return ef2_iop_exec_module_buffer_ex(
        module,
        size,
        (ef2_s32 *)0);
}
