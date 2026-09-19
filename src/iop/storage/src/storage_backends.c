#include "irx_imports.h"
#include "sio2_storage.h"
#include "storage_backends.h"

#define MC_CTRL1 0xFF020405u
#define MC_CTRL2 0x0005FFFFu
#define MMCE_ID 0x8Bu
#define MMCE_REPLY 0xAAu
#define MMCE_CTRL1 0xFF020005u
#define MMCE_CTRL2 0x0005FFFFu

#define MX_PORT 3u
#define MX_BAUD_SLOW 0x78u
#define MX_BAUD_FAST 0x02u
#define MX_CTRL1(baud) (0x00000505u | (0x78u << 16) | ((ef2_u32)(baud) << 24))
#define MX_CTRL2 0x0000012Cu

#define SD_CMD0  (0u  | 0x40u)
#define SD_CMD1  (1u  | 0x40u)
#define SD_CMD8  (8u  | 0x40u)
#define SD_CMD9  (9u  | 0x40u)
#define SD_CMD16 (16u | 0x40u)
#define SD_CMD17 (17u | 0x40u)
#define SD_CMD24 (24u | 0x40u)
#define SD_CMD55 (55u | 0x40u)
#define SD_CMD58 (58u | 0x40u)
#define SD_CMD59 (59u | 0x40u)
#define SD_ACMD41 (41u | 0x40u)

enum {
    MX_CARD_MMC = 0,
    MX_CARD_SDV1 = 1,
    MX_CARD_SDV2 = 2,
    MX_CARD_SDHC = 4
};

static ef2_u32 g_mx_baud = MX_BAUD_SLOW;
static ef2_u32 g_mx_card_type;
static ef2_u32 g_mx_sector_count;
static ef2_s32 g_mx_initialized;

#define MC_PIO_READ_CHUNK 32u
static ef2_u8 g_mc_format_page[EF2_STORAGE_IO_CHUNK];

static void clear_bytes(void *ptr, ef2_u32 size)
{
    ef2_u8 *bytes = (ef2_u8 *)ptr;
    ef2_u32 i;
    for (i = 0; i < size; ++i)
        bytes[i] = 0u;
}

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
    dest->card_flags = source->card_flags;
    dest->formatted = source->formatted;
    dest->current_card = source->current_card;
    dest->current_channel = source->current_channel;
    dest->status = source->status;
}

static ef2_u32 string_length(const char *text, ef2_u32 limit)
{
    ef2_u32 length = 0u;
    while (text != (const char *)0 && length < limit && text[length] != '\0')
        ++length;
    return length;
}

static ef2_u8 xor_edc(const ef2_u8 *data, ef2_u32 size)
{
    ef2_u8 value = 0u;
    ef2_u32 i;
    for (i = 0; i < size; ++i)
        value ^= data[i];
    return value;
}

static int mc_set_terminator(
    ef2_u32 port,
    ef2_u8 terminator)
{
    ef2_u8 tx[5];
    ef2_u8 rx[5];
    int result;

    clear_bytes(tx, sizeof(tx));
    clear_bytes(rx, sizeof(rx));

    tx[0] = 0x81u;
    tx[1] = 0x27u;
    tx[2] = terminator;

    result = ef2_storage_sio2_exchange(
        port,
        MC_CTRL1,
        MC_CTRL2,
        0u,
        tx,
        sizeof(tx),
        rx,
        sizeof(rx),
        20000u);

    if (result < 0)
        return result;

    if (rx[3] != 0x2Bu)
        return -20;

    if (rx[4] != terminator)
        return -21;

    return 0;
}

static int mc_get_spec(
    ef2_u32 port,
    ef2_storage_device_info *info,
    ef2_s32 *terminator_result)
{
    ef2_u8 tx[13];
    ef2_u8 rx[13];
    int result;
    int term_result;

    term_result =
        mc_set_terminator(port, 0x5Au);

    if (terminator_result != (ef2_s32 *)0)
        *terminator_result = term_result;

    clear_bytes(tx, sizeof(tx));
    clear_bytes(rx, sizeof(rx));

    tx[0] = 0x81u;
    tx[1] = 0x26u;

    result = ef2_storage_sio2_exchange(
        port,
        MC_CTRL1,
        MC_CTRL2,
        0u,
        tx,
        sizeof(tx),
        rx,
        sizeof(rx),
        20000u);

    if (result < 0)
        return result;

    /*
     * A freshly reset card can still use the standard 0x55 terminator if
     * SET_TERMINATOR was not accepted.  The EDC below is the stronger
     * geometry validation; accept both known ready terminators here.
     */
    if (rx[12] != 0x5Au &&
        rx[12] != 0x55u)
        return -10;

    if (xor_edc(&rx[3], 8u) != rx[11])
        return -11;

    info->card_flags = rx[2];
    info->page_size =
        (ef2_u32)rx[3] |
        ((ef2_u32)rx[4] << 8);
    info->erase_block_pages =
        (ef2_u32)rx[5] |
        ((ef2_u32)rx[6] << 8);
    info->page_count =
        (ef2_u32)rx[7] |
        ((ef2_u32)rx[8] << 8) |
        ((ef2_u32)rx[9] << 16) |
        ((ef2_u32)rx[10] << 24);

    if (info->page_size == 0u ||
        info->page_count == 0u)
        return -12;

    return 0;
}

static int mmce_exchange(ef2_u32 port, const ef2_u8 *tx, ef2_u32 tx_size, ef2_u8 *rx, ef2_u32 rx_size)
{
    return ef2_storage_sio2_exchange(
        port, MMCE_CTRL1, MMCE_CTRL2, 0u,
        tx, tx_size, rx, rx_size, 200000u);
}

static int mmce_ping(ef2_u32 port, ef2_storage_device_info *info)
{
    ef2_u8 tx[7] = {MMCE_ID, 0x01u, 0xFFu, 0u, 0u, 0u, 0xFFu};
    ef2_u8 rx[7];
    int result;

    clear_bytes(rx, sizeof(rx));
    result = mmce_exchange(port, tx, sizeof(tx), rx, sizeof(rx));
    if (result < 0)
        return result;
    if (rx[1] != MMCE_REPLY)
        return -10;

    info->protocol_version = rx[3];
    info->product_id = rx[4];
    info->product_revision = rx[5];
    return 0;
}

static int mmce_get_u16(ef2_u32 port, ef2_u8 command)
{
    ef2_u8 tx[3] = {MMCE_ID, command, 0xFFu};
    ef2_u8 rx[6];
    int result;
    clear_bytes(rx, sizeof(rx));
    result = mmce_exchange(port, tx, sizeof(tx), rx, sizeof(rx));
    if (result < 0)
        return result;
    if (rx[1] != MMCE_REPLY)
        return -10;
    return ((int)rx[3] << 8) | rx[4];
}

static int mmce_set_packet(ef2_u32 port, const ef2_u8 *tx, ef2_u32 size)
{
    ef2_u8 rx[2];
    int result;
    clear_bytes(rx, sizeof(rx));
    result = mmce_exchange(port, tx, size, rx, sizeof(rx));
    if (result < 0)
        return result;
    return rx[1] == MMCE_REPLY ? 0 : -10;
}

static ef2_u8 mmce_pack_open_flags(ef2_u32 flags)
{
    ef2_u32 access = flags & 3u;
    ef2_u32 packed = 0u;
    if (access != 0u)
        packed = access - 1u;
    packed |= (flags & 0x100u) >> 5;
    packed |= (flags & 0xE00u) >> 4;
    return (ef2_u8)packed;
}

static int mmce_validate_device(const ef2_storage_device_info *device)
{
    if (device == (const ef2_storage_device_info *)0 ||
        device->kind != EF2_STORAGE_KIND_MMCE ||
        device->physical_port < 2u || device->physical_port > 3u)
        return -1;
    return 0;
}

int ef2_storage_backend_mmce_status(const ef2_storage_device_info *device)
{
    if (mmce_validate_device(device) < 0)
        return -1;
    return mmce_get_u16(device->physical_port, 0x02u);
}

int ef2_storage_backend_mmce_get_card(const ef2_storage_device_info *device)
{
    if (mmce_validate_device(device) < 0)
        return -1;
    return mmce_get_u16(device->physical_port, 0x03u);
}

int ef2_storage_backend_mmce_set_card(const ef2_storage_device_info *device, ef2_u32 type, ef2_u32 mode, ef2_u32 card)
{
    ef2_u8 tx[8];
    if (mmce_validate_device(device) < 0 || type > 1u || mode > 2u || card > 0xFFFFu)
        return -1;
    tx[0]=MMCE_ID; tx[1]=0x04u; tx[2]=0xFFu; tx[3]=(ef2_u8)type; tx[4]=(ef2_u8)mode;
    tx[5]=(ef2_u8)(card>>8); tx[6]=(ef2_u8)card; tx[7]=0xFFu;
    return mmce_set_packet(device->physical_port, tx, sizeof(tx));
}

int ef2_storage_backend_mmce_get_channel(const ef2_storage_device_info *device)
{
    if (mmce_validate_device(device) < 0)
        return -1;
    return mmce_get_u16(device->physical_port, 0x05u);
}

int ef2_storage_backend_mmce_set_channel(const ef2_storage_device_info *device, ef2_u32 mode, ef2_u32 channel)
{
    ef2_u8 tx[7];
    if (mmce_validate_device(device) < 0 || mode > 2u || channel > 0xFFFFu)
        return -1;
    tx[0]=MMCE_ID; tx[1]=0x06u; tx[2]=0xFFu; tx[3]=(ef2_u8)mode;
    tx[4]=(ef2_u8)(channel>>8); tx[5]=(ef2_u8)channel; tx[6]=0xFFu;
    return mmce_set_packet(device->physical_port, tx, sizeof(tx));
}

int ef2_storage_backend_mmce_set_game_id(const ef2_storage_device_info *device, const char *game_id)
{
    ef2_u8 tx[256];
    ef2_u32 length;
    ef2_u32 i;
    if (mmce_validate_device(device) < 0 || game_id == (const char *)0)
        return -1;
    length = string_length(game_id, EF2_STORAGE_GAME_ID_MAX - 1u);
    if (length + 1u > EF2_STORAGE_GAME_ID_MAX)
        return -2;
    clear_bytes(tx, sizeof(tx));
    tx[0]=MMCE_ID; tx[1]=0x08u; tx[2]=0xFFu; tx[3]=(ef2_u8)(length+1u);
    for (i=0;i<length;++i) tx[4u+i]=(ef2_u8)game_id[i];
    tx[4u+length]=0u; tx[5u+length]=0xFFu;
    return mmce_set_packet(device->physical_port, tx, length+6u);
}

int ef2_storage_backend_mmce_open(const ef2_storage_device_info *device, const char *path, ef2_u32 flags)
{
    ef2_u8 header_tx[5] = {MMCE_ID,0x40u,0xFFu,0u,0xFFu};
    ef2_u8 header_rx[5];
    ef2_u8 path_rx[EF2_STORAGE_PATH_MAX];
    ef2_u8 footer[3];
    ef2_u32 length;
    int result;

    if (mmce_validate_device(device)<0 || path==(const char *)0) return -1;
    length=string_length(path,EF2_STORAGE_PATH_MAX-1u)+1u;
    header_tx[3]=mmce_pack_open_flags(flags);
    clear_bytes(header_rx,sizeof(header_rx));
    result=mmce_exchange(device->physical_port,header_tx,sizeof(header_tx),header_rx,sizeof(header_rx));
    if(result<0||header_rx[1]!=MMCE_REPLY) return -2;
    clear_bytes(path_rx,sizeof(path_rx));
    result=mmce_exchange(device->physical_port,(const ef2_u8 *)path,length,path_rx,length);
    if(result<0) return -3;
    clear_bytes(footer,sizeof(footer));
    result=mmce_exchange(device->physical_port,(const ef2_u8 *)0,0u,footer,sizeof(footer));
    if(result<0) return -4;
    return footer[1]==0xFFu ? -5 : (int)footer[1];
}

int ef2_storage_backend_mmce_close(const ef2_storage_device_info *device, ef2_s32 fd)
{
    ef2_u8 tx[6]={MMCE_ID,0x41u,0xFFu,0u,0xFFu,0xFFu};
    ef2_u8 rx[6];
    int result;
    if(mmce_validate_device(device)<0||fd<0||fd>255) return -1;
    tx[3]=(ef2_u8)fd; clear_bytes(rx,sizeof(rx));
    result=mmce_exchange(device->physical_port,tx,sizeof(tx),rx,sizeof(rx));
    if(result<0||rx[1]!=MMCE_REPLY) return -2;
    return rx[4]==0u ? 0 : -3;
}

int ef2_storage_backend_mmce_read(const ef2_storage_device_info *device, ef2_s32 fd, ef2_u8 *data, ef2_u32 size)
{
    ef2_u8 tx[10],rx[10],footer[6];
    ef2_u32 off=0;
    int result,bytes_read;
    if(mmce_validate_device(device)<0||data==(ef2_u8 *)0||size>EF2_STORAGE_IO_CHUNK||fd<0||fd>255) return -1;
    clear_bytes(tx,sizeof(tx));
    tx[0]=MMCE_ID;tx[1]=0x42u;tx[2]=0xFFu;tx[4]=(ef2_u8)fd;
    tx[5]=(ef2_u8)(size>>24);tx[6]=(ef2_u8)(size>>16);tx[7]=(ef2_u8)(size>>8);tx[8]=(ef2_u8)size;tx[9]=0xFFu;
    clear_bytes(rx,sizeof(rx));
    result=mmce_exchange(device->physical_port,tx,sizeof(tx),rx,sizeof(rx));
    if(result<0||rx[1]!=MMCE_REPLY||rx[9]!=0u) return -2;
    while(off<size){ef2_u32 chunk=size-off;if(chunk>256u)chunk=256u;result=mmce_exchange(device->physical_port,(const ef2_u8 *)0,0u,&data[off],chunk);if(result<0)return -3;off+=chunk;}
    clear_bytes(footer,sizeof(footer));
    result=mmce_exchange(device->physical_port,(const ef2_u8 *)0,0u,footer,sizeof(footer));
    if(result<0)return -4;
    bytes_read=((int)footer[1]<<24)|((int)footer[2]<<16)|((int)footer[3]<<8)|footer[4];
    if (bytes_read < 0)
        return -5;
    if ((ef2_u32)bytes_read > size)
        bytes_read = (int)size;
    return bytes_read;
}

int ef2_storage_backend_mmce_write(const ef2_storage_device_info *device, ef2_s32 fd, const ef2_u8 *data, ef2_u32 size)
{
    ef2_u8 tx[10],rx[10],ready[2],footer[6];
    ef2_u32 off=0,attempts;
    int result,bytes_written;
    if(mmce_validate_device(device)<0||data==(const ef2_u8 *)0||size>EF2_STORAGE_IO_CHUNK||fd<0||fd>255)return -1;
    clear_bytes(tx,sizeof(tx));
    tx[0]=MMCE_ID;tx[1]=0x43u;tx[2]=0xFFu;tx[4]=(ef2_u8)fd;
    tx[5]=(ef2_u8)(size>>24);tx[6]=(ef2_u8)(size>>16);tx[7]=(ef2_u8)(size>>8);tx[8]=(ef2_u8)size;tx[9]=0xFFu;
    clear_bytes(rx,sizeof(rx));
    result=mmce_exchange(device->physical_port,tx,sizeof(tx),rx,sizeof(rx));
    if(result<0||rx[1]!=MMCE_REPLY||rx[9]!=0u)return -2;
    for(attempts=0;attempts<128u;++attempts){clear_bytes(ready,sizeof(ready));result=mmce_exchange(device->physical_port,(const ef2_u8 *)0,0u,ready,sizeof(ready));if(result<0)return -3;if(ready[1]!=0u)break;DelayThread(1000);}
    if(attempts==128u)return -4;
    while(off<size){ef2_u32 chunk=size-off;if(chunk>256u)chunk=256u;result=mmce_exchange(device->physical_port,&data[off],chunk,(ef2_u8 *)0,0u);if(result<0)return -5;off+=chunk;}
    clear_bytes(footer,sizeof(footer));result=mmce_exchange(device->physical_port,(const ef2_u8 *)0,0u,footer,sizeof(footer));if(result<0)return -6;
    bytes_written=((int)footer[1]<<24)|((int)footer[2]<<16)|((int)footer[3]<<8)|footer[4];
    if (bytes_written < 0)
        return -7;
    if ((ef2_u32)bytes_written > size)
        bytes_written = (int)size;
    return bytes_written;
}

ef2_s32 ef2_storage_backend_mmce_lseek(const ef2_storage_device_info *device, ef2_s32 fd, ef2_s32 offset, ef2_s32 whence)
{
    ef2_u8 tx[9],rx[14];
    int result;
    if(mmce_validate_device(device)<0||fd<0||fd>255)return -1;
    clear_bytes(tx,sizeof(tx));
    tx[0]=MMCE_ID;tx[1]=0x44u;tx[2]=0xFFu;tx[3]=(ef2_u8)fd;
    tx[4]=(ef2_u8)((ef2_u32)offset>>24);tx[5]=(ef2_u8)((ef2_u32)offset>>16);tx[6]=(ef2_u8)((ef2_u32)offset>>8);tx[7]=(ef2_u8)offset;tx[8]=(ef2_u8)whence;
    clear_bytes(rx,sizeof(rx));result=mmce_exchange(device->physical_port,tx,sizeof(tx),rx,sizeof(rx));
    if(result<0||rx[1]!=MMCE_REPLY)return -2;
    return (ef2_s32)(((ef2_u32)rx[9]<<24)|((ef2_u32)rx[10]<<16)|((ef2_u32)rx[11]<<8)|rx[12]);
}

static ef2_u8 reverse_bits(ef2_u8 value)
{
    value=(ef2_u8)(((value&0x55u)<<1)|((value>>1)&0x55u));
    value=(ef2_u8)(((value&0x33u)<<2)|((value>>2)&0x33u));
    return (ef2_u8)((value<<4)|(value>>4));
}

static int mx_exchange(const ef2_u8 *tx, ef2_u32 tx_size, ef2_u8 *rx, ef2_u32 rx_size)
{
    ef2_u8 tx_raw[256],rx_raw[256];
    ef2_u32 i;
    int result;
    if(tx_size>sizeof(tx_raw)||rx_size>sizeof(rx_raw))return -1;
    for(i=0;i<tx_size;++i)tx_raw[i]=reverse_bits(tx[i]);
    clear_bytes(rx_raw,sizeof(rx_raw));
    result=ef2_storage_sio2_exchange(MX_PORT,MX_CTRL1(g_mx_baud),MX_CTRL2,1u,
        tx_size?tx_raw:(const ef2_u8 *)0,tx_size,rx_size?rx_raw:(ef2_u8 *)0,rx_size,100000u);
    if(result<0)return result;
    for(i=0;i<rx_size;++i)rx[i]=reverse_bits(rx_raw[i]);
    return 0;
}

static ef2_u8 mx_crc7(const ef2_u8 *data, ef2_u32 size)
{
    ef2_u8 crc=0u;
    ef2_u32 i;
    for(i=0;i<size;++i){ef2_u8 v=data[i];ef2_u32 bit;for(bit=0;bit<8u;++bit){ef2_u8 mix=(ef2_u8)(((crc&0x40u)!=0u)^((v&0x80u)!=0u));crc=(ef2_u8)((crc<<1)&0x7Fu);if(mix)crc^=0x09u;v<<=1;}}
    return crc;
}

static int mx_dummy(void)
{
    ef2_u8 tx=0xFFu,rx=0xFFu;
    if(mx_exchange(&tx,1u,&rx,1u)<0)return -1;
    return rx;
}

static int mx_receive_byte(ef2_u8 *value)
{
    ef2_u8 rx=0xFFu;
    if(mx_exchange((const ef2_u8 *)0,0u,&rx,1u)<0)return -1;
    *value=rx;return 0;
}

static int mx_wait_not_equal(ef2_u8 value,ef2_u32 attempts)
{
    ef2_u32 i;
    for(i=0;i<attempts;++i){ef2_u8 rx;if(mx_receive_byte(&rx)<0)return -1;if(rx!=value)return rx;}
    return -2;
}

static int mx_wait_equal(ef2_u8 value,ef2_u32 attempts)
{
    ef2_u32 i;
    for(i=0;i<attempts;++i){ef2_u8 rx;if(mx_receive_byte(&rx)<0)return -1;if(rx==value)return 0;}
    return -2;
}

static int mx_send_command(ef2_u8 command,ef2_u32 argument)
{
    ef2_u8 packet[6];
    if(command!=SD_CMD0)(void)mx_dummy();
    packet[0]=command;packet[1]=(ef2_u8)(argument>>24);packet[2]=(ef2_u8)(argument>>16);packet[3]=(ef2_u8)(argument>>8);packet[4]=(ef2_u8)argument;
    packet[5]=(ef2_u8)((mx_crc7(packet,5u)<<1)|1u);
    if(mx_exchange(packet,sizeof(packet),(ef2_u8 *)0,0u)<0)return -1;
    return mx_wait_not_equal(0xFFu,256u);
}

static int mx_send_command_data(ef2_u8 command,ef2_u32 argument,ef2_u8 *data,ef2_u32 size)
{
    int response=mx_send_command(command,argument);
    if(response<0)return response;
    if(mx_exchange((const ef2_u8 *)0,0u,data,size)<0)return -3;
    (void)mx_dummy();return response;
}

static int mx_read_register(ef2_u8 command,ef2_u8 data[18])
{
    if(mx_send_command(command,0u)!=0)return -1;
    if(mx_wait_equal(0xFEu,4096u)<0)return -2;
    if(mx_exchange((const ef2_u8 *)0,0u,data,18u)<0)return -3;
    (void)mx_dummy();return 0;
}

static ef2_u32 mx_csd_sector_count(const ef2_u8 csd[16])
{
    ef2_u32 structure =
        (csd[0] >> 6) & 3u;

    if (structure == 1u) {
        ef2_u32 c_size =
            ((ef2_u32)(csd[7] & 0x3Fu) << 16) |
            ((ef2_u32)csd[8] << 8) |
            csd[9];

        return (c_size + 1u) * 1024u;
    }

    if (structure == 0u) {
        ef2_u32 read_block_shift =
            csd[5] & 0x0Fu;
        ef2_u32 c_size =
            ((ef2_u32)(csd[6] & 0x03u) << 10) |
            ((ef2_u32)csd[7] << 2) |
            ((csd[8] >> 6) & 0x03u);
        ef2_u32 c_size_mult =
            ((ef2_u32)(csd[9] & 0x03u) << 1) |
            ((csd[10] >> 7) & 1u);
        ef2_u32 units = c_size + 1u;
        ef2_s32 sector_shift =
            (ef2_s32)c_size_mult +
            2 +
            (ef2_s32)read_block_shift -
            9;

        if (sector_shift >= 0) {
            if (sector_shift >= 32 ||
                units >
                    (0xFFFFFFFFu >>
                     (ef2_u32)sector_shift))
                return 0u;

            return
                units <<
                (ef2_u32)sector_shift;
        }

        if (sector_shift <= -32)
            return 0u;

        return
            units >>
            (ef2_u32)(-sector_shift);
    }

    return 0u;
}

static int mx_initialize(void)
{
    ef2_u8 dummy[10],ocr[6],reg[18];
    ef2_u32 i;
    int response;
    g_mx_baud=MX_BAUD_SLOW;g_mx_initialized=0;g_mx_sector_count=0u;
    for(i=0;i<sizeof(dummy);++i)dummy[i]=0xFFu;
    if(mx_exchange(dummy,sizeof(dummy),(ef2_u8 *)0,0u)<0)return -1;
    response = mx_send_command(SD_CMD0, 0u);
    if (response != 0x01)
        return -2;
    clear_bytes(ocr,sizeof(ocr));response=mx_send_command_data(SD_CMD8,0x1AAu,ocr,sizeof(ocr));
    if(response==0x01&&ocr[2]==0x01u&&ocr[3]==0xAAu){
        for(i=0;i<4096u;++i){response=mx_send_command(SD_CMD55,0u);if(response!=0x01)return -3;response=mx_send_command(SD_ACMD41,0x40000000u);if(response==0)break;}
        if(response!=0)return -4;
        clear_bytes(ocr, sizeof(ocr));
        response = mx_send_command_data(
            SD_CMD58, 0u, ocr, sizeof(ocr));
        if (response != 0)
            return -5;
        g_mx_card_type=(ocr[0]&0x40u)?MX_CARD_SDHC:MX_CARD_SDV2;
    }else{
        for(i=0;i<4096u;++i){response=mx_send_command(SD_CMD55,0u);if(response==0x01){response=mx_send_command(SD_ACMD41,0u);if(response==0)break;}}
        if(response==0){g_mx_card_type=MX_CARD_SDV1;}else{for(i=0;i<4096u;++i){response=mx_send_command(SD_CMD1,0u);if(response==0)break;}if(response!=0)return -6;g_mx_card_type=MX_CARD_MMC;}
        (void)mx_send_command(SD_CMD59, 0u);
        if (mx_send_command(SD_CMD16, 512u) != 0)
            return -7;
    }
    g_mx_baud=MX_BAUD_FAST;
    if(mx_read_register(SD_CMD9,reg)<0)return -8;
    g_mx_sector_count = mx_csd_sector_count(reg);
    if (g_mx_sector_count == 0u)
        return -9;
    g_mx_initialized=1;return 0;
}

static int mx_read_sector(ef2_u32 sector,ef2_u8 data[512])
{
    ef2_u32 address=sector;ef2_u8 crc[2];int response;
    if(!g_mx_initialized||sector>=g_mx_sector_count)return -1;
    if(g_mx_card_type!=MX_CARD_SDHC)address<<=9;
    if(mx_wait_equal(0xFFu,4096u)<0)return -2;
    response = mx_send_command(SD_CMD17, address);
    if (response != 0)
        return -3;
    if(mx_wait_equal(0xFEu,100000u)<0)return -4;
    if(mx_exchange((const ef2_u8 *)0,0u,data,256u)<0)return -5;
    if(mx_exchange((const ef2_u8 *)0,0u,data+256u,256u)<0)return -6;
    if(mx_exchange((const ef2_u8 *)0,0u,crc,sizeof(crc))<0)return -7;
    (void)mx_dummy();return 0;
}

static int mx_write_sector(ef2_u32 sector,const ef2_u8 data[512])
{
    ef2_u32 address=sector;ef2_u8 token=0xFEu,crc[2]={0xFFu,0xFFu},response;int cmd;
    if(!g_mx_initialized||sector>=g_mx_sector_count)return -1;
    if(g_mx_card_type!=MX_CARD_SDHC)address<<=9;
    if(mx_wait_equal(0xFFu,4096u)<0)return -2;
    cmd = mx_send_command(SD_CMD24, address);
    if (cmd != 0)
        return -3;
    if(mx_exchange(&token,1u,(ef2_u8 *)0,0u)<0)return -4;
    if(mx_exchange(data,256u,(ef2_u8 *)0,0u)<0)return -5;
    if(mx_exchange(data+256u,256u,(ef2_u8 *)0,0u)<0)return -6;
    if(mx_exchange(crc,sizeof(crc),(ef2_u8 *)0,0u)<0)return -7;
    if (mx_receive_byte(&response) < 0)
        return -8;
    if ((response & 0x1Fu) != 0x05u)
        return -9;
    if (mx_wait_equal(0xFFu, 0x80000u) < 0)
        return -10;
    (void)mx_dummy();
    return 0;
}

static int mc_valid_terminator(ef2_u8 value)
{
    return
        value == 0x55u ||
        value == 0x5Au;
}

static int mc_select_read_page(
    const ef2_storage_device_info *device,
    ef2_u32 page)
{
    ef2_u8 tx[9];
    ef2_u8 rx[9];
    int result;

    clear_bytes(tx, sizeof(tx));
    clear_bytes(rx, sizeof(rx));

    tx[0] = 0x81u;
    tx[1] = 0x23u;
    tx[2] = (ef2_u8)page;
    tx[3] = (ef2_u8)(page >> 8);
    tx[4] = (ef2_u8)(page >> 16);
    tx[5] = (ef2_u8)(page >> 24);
    tx[6] = xor_edc(&tx[2], 4u);

    result = ef2_storage_sio2_exchange(
        device->physical_port,
        MC_CTRL1,
        MC_CTRL2,
        0u,
        tx,
        sizeof(tx),
        rx,
        sizeof(rx),
        20000u);

    if (result < 0)
        return -10;

    if (rx[7] != 0x2Bu)
        return -11;

    if (!mc_valid_terminator(rx[8]))
        return -12;

    return 0;
}

static int mc_read_data_chunk(
    const ef2_storage_device_info *device,
    ef2_u8 *dest,
    ef2_u32 size)
{
    ef2_u8 tx[MC_PIO_READ_CHUNK + 6u];
    ef2_u8 rx[MC_PIO_READ_CHUNK + 6u];
    ef2_u32 packet_size;
    ef2_u32 i;
    int result;

    if (size == 0u ||
        size > MC_PIO_READ_CHUNK)
        return -20;

    packet_size = size + 6u;

    clear_bytes(tx, sizeof(tx));
    clear_bytes(rx, sizeof(rx));

    tx[0] = 0x81u;
    tx[1] = 0x43u;
    tx[2] = (ef2_u8)size;

    result = ef2_storage_sio2_exchange(
        device->physical_port,
        MC_CTRL1,
        MC_CTRL2,
        0u,
        tx,
        packet_size,
        rx,
        packet_size,
        40000u);

    if (result < 0)
        return -21;

    if (rx[3] != 0x2Bu)
        return -22;

    if (xor_edc(&rx[4], size) !=
        rx[4u + size])
        return -23;

    if (!mc_valid_terminator(
            rx[5u + size]))
        return -24;

    for (i = 0; i < size; ++i)
        dest[i] = rx[4u + i];

    return 0;
}

static int mc_finish_read(
    const ef2_storage_device_info *device)
{
    ef2_u8 tx[4];
    ef2_u8 rx[4];
    int result;

    clear_bytes(tx, sizeof(tx));
    clear_bytes(rx, sizeof(rx));

    tx[0] = 0x81u;
    tx[1] = 0x81u;

    result = ef2_storage_sio2_exchange(
        device->physical_port,
        MC_CTRL1,
        MC_CTRL2,
        0u,
        tx,
        sizeof(tx),
        rx,
        sizeof(rx),
        20000u);

    if (result < 0)
        return -30;

    if (rx[2] != 0x2Bu)
        return -31;

    if (!mc_valid_terminator(rx[3]))
        return -32;

    return 0;
}

static int mc_read_page(
    const ef2_storage_device_info *device,
    ef2_u32 page,
    ef2_u8 data[EF2_STORAGE_IO_CHUNK])
{
    ef2_u32 chunk;
    int result;

    if (device == (const ef2_storage_device_info *)0 ||
        data == (ef2_u8 *)0)
        return -1;

    if ((device->capabilities &
         EF2_STORAGE_CAP_GEOMETRY) == 0u ||
        device->page_size != EF2_STORAGE_IO_CHUNK ||
        page >= device->page_count)
        return -2;

    result =
        mc_select_read_page(
            device,
            page);

    if (result < 0)
        return result;

    for (chunk = 0u;
         chunk < EF2_STORAGE_IO_CHUNK;
         chunk += MC_PIO_READ_CHUNK) {
        result =
            mc_read_data_chunk(
                device,
                &data[chunk],
                MC_PIO_READ_CHUNK);

        if (result < 0)
            return result;
    }

    return
        mc_finish_read(device);
}

static int mc_check_format(
    const ef2_storage_device_info *device,
    ef2_s32 *formatted)
{
    static const char magic[] =
        "Sony PS2 Memory Card Format ";
    ef2_u32 i;
    int result;

    if (formatted == (ef2_s32 *)0)
        return -1;

    *formatted =
        EF2_STORAGE_FORMAT_UNKNOWN;

    result =
        mc_read_page(
            device,
            0u,
            g_mc_format_page);

    if (result < 0)
        return result;

    for (i = 0u;
         i < sizeof(magic) - 1u;
         ++i) {
        if (g_mc_format_page[i] !=
            (ef2_u8)magic[i]) {
            *formatted =
                EF2_STORAGE_FORMAT_UNFORMATTED;
            return 0;
        }
    }

    *formatted =
        EF2_STORAGE_FORMAT_FORMATTED;
    return 0;
}

int ef2_storage_backend_scan(
    ef2_storage_device_info *devices,
    ef2_u32 capacity,
    ef2_u32 *count,
    ef2_storage_scan_diag *diag)
{
    ef2_u32 port;
    ef2_u32 found = 0u;

    if (devices == (ef2_storage_device_info *)0 ||
        count == (ef2_u32 *)0 ||
        diag == (ef2_storage_scan_diag *)0 ||
        capacity == 0u)
        return -1;

    diag->mmce_result[0] = -127;
    diag->mmce_result[1] = -127;
    diag->mc_terminator_result[0] = -127;
    diag->mc_terminator_result[1] = -127;
    diag->mc_geometry_result[0] = -127;
    diag->mc_geometry_result[1] = -127;
    diag->mc_format_result[0] = -127;
    diag->mc_format_result[1] = -127;
    diag->mx4sio_result = -127;

    for (port = 2u;
         port <= 3u && found < capacity;
         ++port) {
        ef2_storage_device_info info;
        ef2_u32 logical_port = port - 2u;
        ef2_s32 term_result = -127;
        int mmce_result;
        int mc_result;

        clear_bytes(&info, sizeof(info));
        info.physical_port = port;
        info.formatted =
            EF2_STORAGE_FORMAT_UNKNOWN;

        mmce_result =
            mmce_ping(port, &info);
        diag->mmce_result[logical_port] =
            mmce_result;

        if (mmce_result == 0) {
            info.kind = EF2_STORAGE_KIND_MMCE;
            info.capabilities =
                EF2_STORAGE_CAP_MEMORY_CARD |
                EF2_STORAGE_CAP_FILESYSTEM |
                EF2_STORAGE_CAP_VIRTUAL_CARDS |
                EF2_STORAGE_CAP_GAME_ID;

            mc_result =
                mc_get_spec(
                    port,
                    &info,
                    &term_result);

            diag->mc_terminator_result[logical_port] =
                term_result;
            diag->mc_geometry_result[logical_port] =
                mc_result;

            if (mc_result == 0) {
                int format_result;

                info.capabilities |=
                    EF2_STORAGE_CAP_GEOMETRY |
                    EF2_STORAGE_CAP_PAGE_READ;

                format_result =
                    mc_check_format(
                        &info,
                        &info.formatted);

                diag->mc_format_result[logical_port] =
                    format_result;
            }

            {
                int value =
                    mmce_get_u16(port, 0x03u);
                info.current_card =
                    value < 0 ? 0u : (ef2_u32)value;
            }
            {
                int value =
                    mmce_get_u16(port, 0x05u);
                info.current_channel =
                    value < 0 ? 0u : (ef2_u32)value;
            }
            {
                int value =
                    mmce_get_u16(port, 0x02u);
                info.status =
                    value < 0 ? 0u : (ef2_u32)value;
            }

            copy_device_info(
                &devices[found],
                &info);
            ++found;
            continue;
        }

        mc_result =
            mc_get_spec(
                port,
                &info,
                &term_result);

        diag->mc_terminator_result[logical_port] =
            term_result;
        diag->mc_geometry_result[logical_port] =
            mc_result;

        if (mc_result == 0) {
            int format_result;

            info.kind =
                EF2_STORAGE_KIND_PS2_MEMORY_CARD;
            info.capabilities =
                EF2_STORAGE_CAP_MEMORY_CARD |
                EF2_STORAGE_CAP_GEOMETRY |
                EF2_STORAGE_CAP_PAGE_READ;

            format_result =
                mc_check_format(
                    &info,
                    &info.formatted);

            diag->mc_format_result[logical_port] =
                format_result;

            copy_device_info(
                &devices[found],
                &info);
            ++found;
            continue;
        }

        if (port == MX_PORT) {
            int mx_result =
                mx_initialize();

            diag->mx4sio_result =
                mx_result;

            if (mx_result == 0) {
                clear_bytes(&info, sizeof(info));
                info.formatted =
                    EF2_STORAGE_FORMAT_UNKNOWN;
                info.kind =
                    EF2_STORAGE_KIND_MX4SIO;
                info.capabilities =
                    EF2_STORAGE_CAP_BLOCK_READ |
                    EF2_STORAGE_CAP_BLOCK_WRITE;
                info.physical_port = MX_PORT;
                info.sector_size = 512u;
                info.sector_count =
                    g_mx_sector_count;
                info.product_id =
                    g_mx_card_type;

                copy_device_info(
                    &devices[found],
                    &info);
                ++found;
            }
        }
    }

    *count = found;
    return 0;
}

int ef2_storage_backend_read_page(
    const ef2_storage_device_info *device,
    ef2_u32 page,
    ef2_u8 data[EF2_STORAGE_IO_CHUNK])
{
    if (device == (const ef2_storage_device_info *)0 ||
        data == (ef2_u8 *)0)
        return -1;

    if ((device->capabilities &
         EF2_STORAGE_CAP_PAGE_READ) == 0u)
        return -2;

    return
        mc_read_page(
            device,
            page,
            data);
}

int ef2_storage_backend_read_sector(const ef2_storage_device_info *device,ef2_u32 sector,ef2_u8 data[EF2_STORAGE_IO_CHUNK])
{
    if(device==(const ef2_storage_device_info *)0||data==(ef2_u8 *)0)return -1;
    if(device->kind!=EF2_STORAGE_KIND_MX4SIO)return -2;
    return mx_read_sector(sector,data);
}
int ef2_storage_backend_write_sector(const ef2_storage_device_info *device,ef2_u32 sector,const ef2_u8 data[EF2_STORAGE_IO_CHUNK])
{
    if(device==(const ef2_storage_device_info *)0||data==(const ef2_u8 *)0)return -1;
    if(device->kind!=EF2_STORAGE_KIND_MX4SIO)return -2;
    return mx_write_sector(sector,data);
}
