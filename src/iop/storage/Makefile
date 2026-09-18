IOP_BIN = ef2storage.irx

IOP_IMPORT_INCS += \
    system/loadcore \
    system/sifcmd \
    system/threadman

IOP_INCS += -I../../../include
IOP_OBJS = main.o storage_backends.o sio2_storage.o imports.o

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.bin.make
include $(PS2SDKSRC)/iop/Rules.make
