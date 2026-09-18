#ifndef EF2_MEMORYCARD_H
#define EF2_MEMORYCARD_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EF2_MC_TYPE_NONE = 0,
    EF2_MC_TYPE_PS1 = 1,
    EF2_MC_TYPE_PS2 = 2,
    EF2_MC_TYPE_PDA = 3
} ef2_mc_type;

typedef struct {
    ef2_s32 result;
    ef2_s32 type;
    ef2_s32 free_clusters;
    ef2_s32 formatted;
} ef2_mc_info;

/*
 * Initializes the ROM MCMAN/MCSERV-compatible path and binds the memory-card
 * RPC server. Returns 0 on success or a negative EF2 stage code.
 */
int ef2_mc_init(void);

/*
 * Queries a card slot. A result of 0 means the same card is still present,
 * -1 means a formatted card changed, -2 means an unformatted card changed,
 * and <= -10 is a detect failure. The function itself returns the same card
 * result while also storing it in info->result.
 */
int ef2_mc_get_info(
    ef2_s32 port,
    ef2_s32 slot,
    ef2_mc_info *info);

#ifdef __cplusplus
}
#endif

#endif
