IOP_BIN = ef2audio.irx

IOP_IMPORT_INCS += \
    sound/libsd \
    system/intrman \
    system/loadcore \
    system/sifcmd \
    system/sysmem \
    system/threadman

IOP_INCS += -I../../../include
IOP_OBJS = main.o imports.o

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.bin.make
include $(PS2SDKSRC)/iop/Rules.make
