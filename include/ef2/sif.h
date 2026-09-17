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

int ef2_iop_load_module(const char *path);
int ef2_iop_exec_module_buffer(const void *module, ef2_u32 size);

#ifdef __cplusplus
}
#endif

#endif
