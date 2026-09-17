#ifndef EF2_SIF_H
#define EF2_SIF_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *src;
    void *dest;
    ef2_s32 size;
    ef2_s32 attr;
} ef2_sif_dma_transfer;

typedef struct {
    void *pkt_addr;
    ef2_u32 rpc_id;
    ef2_s32 sema_id;
    ef2_u32 mode;
} ef2_sif_rpc_header;

typedef struct {
    ef2_sif_rpc_header hdr;
    ef2_u32 command;
    void *buf;
    void *cbuf;
    void (*end_function)(void *);
    void *end_param;
    void *server;
} ef2_sif_rpc_client;

int ef2_sif_init(void);
int ef2_sif_bind(ef2_sif_rpc_client *client, ef2_u32 sid);
int ef2_sif_call(
    ef2_sif_rpc_client *client,
    ef2_s32 function,
    void *send_buffer,
    ef2_u32 send_size,
    void *receive_buffer,
    ef2_u32 receive_size);

int ef2_iop_debug_read_module_u32(
    const char *module_name,
    ef2_u32 module_offset,
    ef2_u32 *value);

int ef2_iop_load_module(const char *path);
int ef2_iop_load_module_ex(const char *path, ef2_s32 *module_result);

/*
 * Enables the legacy ROM LOADFILE module-buffer RPC on systems that ship
 * without it. Returns 0 on success/already enabled and a negative stage code
 * if the legacy dispatcher cannot be located or patched.
 */
int ef2_iop_enable_module_buffer(void);

int ef2_iop_exec_module_buffer(const void *module, ef2_u32 size);
int ef2_iop_exec_module_buffer_ex(
    const void *module,
    ef2_u32 size,
    ef2_s32 *module_result);

#ifdef __cplusplus
}
#endif

#endif
