#include "irx_imports.h"
#include "sio2_storage.h"
#include "storage_backends.h"
#include <ef2/storage_rpc.h>

#define EF2_STORAGE_RPC_INPUT_BYTES ((sizeof(ef2_storage_rpc_request) + 63u) & ~63u)
IRX_ID("ef2storage", 1, 0);

static SifRpcDataQueue_t g_rpc_queue;
static SifRpcServerData_t g_rpc_server;
static ef2_u8 g_rpc_input[EF2_STORAGE_RPC_INPUT_BYTES] __attribute__((aligned(64)));
static ef2_storage_rpc_reply g_reply __attribute__((aligned(64)));
static ef2_storage_device_info g_devices[EF2_STORAGE_MAX_DEVICES];
static ef2_u32 g_device_count;
static int g_rpc_ready_sema = -1;

static void clear_bytes(void *ptr, ef2_u32 size){ef2_u8 *p=(ef2_u8 *)ptr;ef2_u32 i;for(i=0;i<size;++i)p[i]=0u;}

static void copy_device_info(
    ef2_storage_device_info *dest,
    const ef2_storage_device_info *source)
{
    dest->kind = source->kind;
    dest->capabilities = source->capabilities;
    dest->physical_port = source->physical_port;
    dest->page_size = source->page_size;
    dest->erase_block_pages = source->erase_block_pages;
    dest->page_count = source->page_count;
    dest->sector_size = source->sector_size;
    dest->sector_count = source->sector_count;
    dest->protocol_version = source->protocol_version;
    dest->product_id = source->product_id;
    dest->product_revision = source->product_revision;
    dest->current_card = source->current_card;
    dest->current_channel = source->current_channel;
    dest->status = source->status;
}

static const ef2_storage_device_info *device_at(ef2_u32 index){return index<g_device_count?&g_devices[index]:(const ef2_storage_device_info *)0;}
static int scan_devices(void){clear_bytes(g_devices,sizeof(g_devices));g_device_count=0u;return ef2_storage_backend_scan(g_devices,EF2_STORAGE_MAX_DEVICES,&g_device_count);}

static void *rpc_handler(int function,void *buffer,int length)
{
    ef2_storage_rpc_request *request=(ef2_storage_rpc_request *)buffer;
    const ef2_storage_device_info *device=0;
    int result=0;
    clear_bytes(&g_reply,sizeof(g_reply));
    if(length>=(int)sizeof(*request))device=device_at(request->index);

    switch(function){
        case EF2_STORAGE_RPC_INIT: result=0; break;
        case EF2_STORAGE_RPC_SCAN: result=scan_devices(); break;
        case EF2_STORAGE_RPC_GET_DEVICE:
            if(length<(int)sizeof(*request)||device==0){result=-1;break;}
            copy_device_info(&g_reply.device, device);
            break;
        case EF2_STORAGE_RPC_READ_SECTOR:
            if(length<(int)sizeof(*request)||device==0){result=-1;break;}
            result=ef2_storage_backend_read_sector(device,request->sector,g_reply.data);
            if(result==0)
                g_reply.data_size=EF2_STORAGE_IO_CHUNK;
            break;
        case EF2_STORAGE_RPC_WRITE_SECTOR:
            if(length<(int)sizeof(*request)||device==0||request->data_size!=EF2_STORAGE_IO_CHUNK){result=-1;break;}
            result=ef2_storage_backend_write_sector(device,request->sector,request->data);break;
        case EF2_STORAGE_RPC_MMCE_STATUS:
            if(device==0){result=-1;break;}g_reply.value=ef2_storage_backend_mmce_status(device);result=g_reply.value<0?g_reply.value:0;break;
        case EF2_STORAGE_RPC_MMCE_GET_CARD:
            if(device==0){result=-1;break;}g_reply.value=ef2_storage_backend_mmce_get_card(device);result=g_reply.value<0?g_reply.value:0;break;
        case EF2_STORAGE_RPC_MMCE_SET_CARD:
            if(device==0){result=-1;break;}result=ef2_storage_backend_mmce_set_card(device,request->value0,request->value1,request->value2);break;
        case EF2_STORAGE_RPC_MMCE_GET_CHANNEL:
            if(device==0){result=-1;break;}g_reply.value=ef2_storage_backend_mmce_get_channel(device);result=g_reply.value<0?g_reply.value:0;break;
        case EF2_STORAGE_RPC_MMCE_SET_CHANNEL:
            if(device==0){result=-1;break;}result=ef2_storage_backend_mmce_set_channel(device,request->value0,request->value1);break;
        case EF2_STORAGE_RPC_MMCE_SET_GAME_ID:
            if(device==0){result=-1;break;}result=ef2_storage_backend_mmce_set_game_id(device,request->path);break;
        case EF2_STORAGE_RPC_MMCE_OPEN:
            if(device==0){result=-1;break;}g_reply.value=ef2_storage_backend_mmce_open(device,request->path,request->value0);result=g_reply.value<0?g_reply.value:0;break;
        case EF2_STORAGE_RPC_MMCE_CLOSE:
            if(device==0){result=-1;break;}result=ef2_storage_backend_mmce_close(device,request->fd);break;
        case EF2_STORAGE_RPC_MMCE_READ:
            if(device==0||request->data_size>EF2_STORAGE_IO_CHUNK){result=-1;break;}
            result=ef2_storage_backend_mmce_read(device,request->fd,g_reply.data,request->data_size);
            if(result>=0){g_reply.data_size=(ef2_u32)result;result=0;}break;
        case EF2_STORAGE_RPC_MMCE_WRITE:
            if(device==0||request->data_size>EF2_STORAGE_IO_CHUNK){result=-1;break;}
            g_reply.value=ef2_storage_backend_mmce_write(device,request->fd,request->data,request->data_size);result=g_reply.value<0?g_reply.value:0;break;
        case EF2_STORAGE_RPC_MMCE_LSEEK:
            if(device==0){result=-1;break;}g_reply.value=ef2_storage_backend_mmce_lseek(device,request->fd,request->offset,request->whence);result=g_reply.value<0?g_reply.value:0;break;
        default: result=-100; break;
    }
    g_reply.result=result;g_reply.device_count=g_device_count;return &g_reply;
}

static void rpc_thread(void *arg)
{
    int tid;(void)arg;tid=GetThreadId();sceSifInitRpc(0);sceSifSetRpcQueue(&g_rpc_queue,tid);
    sceSifRegisterRpc(&g_rpc_server,EF2_STORAGE_RPC_SID,rpc_handler,g_rpc_input,0,0,&g_rpc_queue);
    SignalSema(g_rpc_ready_sema);sceSifRpcLoop(&g_rpc_queue);
}

int _start(int argc,char *argv[])
{
    iop_thread_t thread;iop_sema_t sema;int thread_id;(void)argc;(void)argv;
    if(ef2_storage_sio2_init()<0)return MODULE_NO_RESIDENT_END;
    sema.attr=0;sema.option=0;sema.initial=0;sema.max=1;g_rpc_ready_sema=CreateSema(&sema);if(g_rpc_ready_sema<0)return MODULE_NO_RESIDENT_END;
    thread.attr=TH_C;thread.option=0;thread.thread=rpc_thread;thread.stacksize=0x1800;thread.priority=41;
    thread_id=CreateThread(&thread);if(thread_id<0)return MODULE_NO_RESIDENT_END;if(StartThread(thread_id,0)<0)return MODULE_NO_RESIDENT_END;
    WaitSema(g_rpc_ready_sema);return MODULE_RESIDENT_END;
}
