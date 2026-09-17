#ifndef EF2_KERNEL_H
#define EF2_KERNEL_H

#include <ef2/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EE kernel syscall 0x02. This is provided by the console ROM/kernel,
 * not by PS2SDK. */
void ef2_kernel_set_gs_crt(ef2_s16 interlace, ef2_s16 mode, ef2_s16 field_mode);

#ifdef __cplusplus
}
#endif

#endif
