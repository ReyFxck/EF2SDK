CROSS ?= mips64r5900el-ps2-elf-
CC := $(CROSS)gcc
AR := $(CROSS)ar
NM := $(CROSS)nm
HOSTCC ?= cc
PYTHON ?= python3
READELF := $(CROSS)readelf
OBJDUMP := $(CROSS)objdump

BUILD := build
ELF := $(BUILD)/ef2-boot.elf
MAP := $(BUILD)/ef2-boot.map
LIB := $(BUILD)/libef2.a
HOST_AUDIO_TEST := $(BUILD)/audio-rate-test
HOST_PAD_TEST := $(BUILD)/pad-input-test
IOP_AUDIO_DIR := src/iop/audio
IOP_AUDIO_IRX := $(BUILD)/ef2audio.irx
IOP_AUDIO_C := $(BUILD)/ef2audio_irx.c
IOP_PAD_DIR := src/iop/pad
IOP_PAD_IRX := $(BUILD)/ef2pad.irx
IOP_PAD_C := $(BUILD)/ef2pad_irx.c

CFLAGS := -G0 -O2 -Wall -Wextra -Werror \
          -ffreestanding -fno-builtin -fno-stack-protector \
          -Iinclude

LDFLAGS := -nostdlib -nostartfiles -nodefaultlibs \
           -T ld/ee.ld \
           -Wl,-Map,$(MAP) \
           -Wl,-zmax-page-size=128 \
           -Wl,--build-id=none

LIB_OBJS := \
    $(BUILD)/syscall.o \
    $(BUILD)/cache.o \
    $(BUILD)/sif.o \
    $(BUILD)/gif.o \
    $(BUILD)/gif_dma.o \
    $(BUILD)/video.o \
    $(BUILD)/audio.o \
    $(BUILD)/audio_iop.o \
    $(BUILD)/ef2audio_irx.o \
    $(BUILD)/pad.o \
    $(BUILD)/pad_iop.o \
    $(BUILD)/ef2pad_irx.o

APP_OBJS := \
    $(BUILD)/start.o \
    $(BUILD)/boot.o

.PHONY: all clean check host-test package toolchain-info

all: $(ELF) $(LIB) $(IOP_AUDIO_IRX) $(IOP_PAD_IRX)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: src/ee/runtime/start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: src/ee/kernel/syscall.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cache.o: src/ee/kernel/cache.c include/ef2/cache.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sif.o: src/ee/sif/sif.c include/ef2/base.h include/ef2/kernel.h include/ef2/sif.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gif.o: src/ee/gs/gif.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gif_dma.o: src/ee/gs/gif_dma.c include/ef2/cache.h include/ef2/gif.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/video.o: src/ee/gs/video.c include/ef2/base.h include/ef2/gif.h include/ef2/gs.h include/ef2/kernel.h include/ef2/video.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/audio.o: src/ee/audio/audio.c include/ef2/audio.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/audio_iop.o: src/ee/audio/iop_backend.c include/ef2/audio.h include/ef2/audio_rpc.h include/ef2/sif.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pad.o: src/ee/input/pad.c include/ef2/pad.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pad_iop.o: src/ee/input/iop_backend.c include/ef2/pad.h include/ef2/pad_rpc.h include/ef2/sif.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(IOP_AUDIO_IRX): $(IOP_AUDIO_DIR)/src/main.c $(IOP_AUDIO_DIR)/src/imports.lst $(IOP_AUDIO_DIR)/src/irx_imports.h $(IOP_AUDIO_DIR)/Makefile | $(BUILD)
	$(MAKE) -C $(IOP_AUDIO_DIR) clean all
	cp $(IOP_AUDIO_DIR)/irx/ef2audio.irx $@

$(IOP_AUDIO_C): $(IOP_AUDIO_IRX) scripts/bin2c.py | $(BUILD)
	$(PYTHON) scripts/bin2c.py $(IOP_AUDIO_IRX) $@ ef2audio_irx

$(BUILD)/ef2audio_irx.o: $(IOP_AUDIO_C) include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $(IOP_AUDIO_C) -o $@

$(IOP_PAD_IRX): $(IOP_PAD_DIR)/src/main.c $(IOP_PAD_DIR)/src/sio2_direct.c $(IOP_PAD_DIR)/src/sio2_direct.h $(IOP_PAD_DIR)/src/imports.lst $(IOP_PAD_DIR)/src/irx_imports.h $(IOP_PAD_DIR)/Makefile | $(BUILD)
	$(MAKE) -C $(IOP_PAD_DIR) clean all
	cp $(IOP_PAD_DIR)/irx/ef2pad.irx $@

$(IOP_PAD_C): $(IOP_PAD_IRX) scripts/bin2c.py | $(BUILD)
	$(PYTHON) scripts/bin2c.py $(IOP_PAD_IRX) $@ ef2pad_irx

$(BUILD)/ef2pad_irx.o: $(IOP_PAD_C) include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $(IOP_PAD_C) -o $@

$(BUILD)/boot.o: examples/boot/main.c include/ef2/audio.h include/ef2/base.h include/ef2/pad.h include/ef2/video.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

$(ELF): $(APP_OBJS) $(LIB) ld/ee.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(LIB) -o $@

$(HOST_AUDIO_TEST): tests/audio_rate_test.c src/ee/audio/audio.c include/ef2/audio.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/audio_rate_test.c src/ee/audio/audio.c -o $@

$(HOST_PAD_TEST): tests/pad_input_test.c src/ee/input/pad.c include/ef2/pad.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/pad_input_test.c src/ee/input/pad.c -o $@

host-test: $(HOST_AUDIO_TEST) $(HOST_PAD_TEST)
	$(HOST_AUDIO_TEST)
	$(HOST_PAD_TEST)

check: $(ELF) $(IOP_AUDIO_IRX) $(IOP_PAD_IRX)
	@echo "== Undefined symbols =="
	@if $(NM) -u $(ELF) | grep -q .; then \
		echo "ERROR: unexpected undefined symbols"; \
		$(NM) -u $(ELF); \
		exit 1; \
	else \
		echo "none"; \
	fi
	@entry=`$(READELF) -h $(ELF) | awk '/Entry point address:/ { print $$4 }'`; \
	case "$$entry" in 0x100000|0x00100000) ;; *) echo "ERROR: unexpected entry point $$entry"; exit 1 ;; esac
	@echo "== EF2SDK library symbols =="
	@for sym in ef2_video_init ef2_video_set_double_buffering ef2_video_wait_vsync ef2_video_present ef2_video_get_frame_stats ef2_video_draw_rect ef2_video_draw_line ef2_video_get_size ef2_video_upload_rgba32 ef2_video_upload_indexed8 ef2_video_upload_indexed4 ef2_video_pack_indices4 ef2_video_draw_texture ef2_video_draw_texture_region ef2_video_get_texture_vram_free ef2_gif_dma_send_qwords ef2_cache_writeback_invalidate_range ef2_audio_rate_converter_init ef2_audio_rate_converter_process_s16 ef2_sif_init ef2_audio_device_init ef2_audio_device_start ef2_pad_init ef2_pad_poll ef2_pad_poll_all ef2_pad_poll_slot ef2_pad_get_slot_count ef2_pad_set_rumble ef2_pad_set_rumble_slot ef2_pad_stop_rumble ef2_pad_is_held ef2_pad_axis_deadzone; do \
		if ! $(NM) $(LIB) | grep -q " $$sym$$"; then \
			echo "ERROR: missing library symbol $$sym"; exit 1; \
		fi; \
	done
	@test -s $(IOP_AUDIO_IRX)
	@test -s $(IOP_PAD_IRX)
	@echo "== Disassembly preview =="
	$(OBJDUMP) -d $(ELF) | head -n 180

package: clean all check
	./scripts/package.sh

toolchain-info:
	$(CC) --version
	$(READELF) --version | head -n 1
	@mipsel-none-elf-gcc --version | head -n 1

clean:
	rm -rf $(BUILD) dist \
		$(IOP_AUDIO_DIR)/obj $(IOP_AUDIO_DIR)/irx \
		$(IOP_PAD_DIR)/obj $(IOP_PAD_DIR)/irx
