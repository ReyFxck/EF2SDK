CROSS ?= mips64r5900el-ps2-elf-
CC := $(CROSS)gcc
AR := $(CROSS)ar
NM := $(CROSS)nm
HOSTCC ?= cc
READELF := $(CROSS)readelf
OBJDUMP := $(CROSS)objdump

BUILD := build
ELF := $(BUILD)/ef2-boot.elf
MAP := $(BUILD)/ef2-boot.map
LIB := $(BUILD)/libef2.a
HOST_AUDIO_TEST := $(BUILD)/audio-rate-test

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
    $(BUILD)/gif.o \
    $(BUILD)/video.o \
    $(BUILD)/audio.o

APP_OBJS := \
    $(BUILD)/start.o \
    $(BUILD)/boot.o

.PHONY: all clean check host-test package toolchain-info

all: $(ELF) $(LIB)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: src/ee/runtime/start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: src/ee/kernel/syscall.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gif.o: src/ee/gs/gif.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/video.o: src/ee/gs/video.c include/ef2/base.h include/ef2/gif.h include/ef2/gs.h include/ef2/kernel.h include/ef2/video.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/audio.o: src/ee/audio/audio.c include/ef2/audio.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/boot.o: examples/boot/main.c include/ef2/base.h include/ef2/video.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

$(ELF): $(APP_OBJS) $(LIB) ld/ee.ld
	$(CC) $(LDFLAGS) $(APP_OBJS) $(LIB) -o $@

$(HOST_AUDIO_TEST): tests/audio_rate_test.c src/ee/audio/audio.c include/ef2/audio.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/audio_rate_test.c src/ee/audio/audio.c -o $@

host-test: $(HOST_AUDIO_TEST)
	$(HOST_AUDIO_TEST)

check: $(ELF)
	@echo "== ELF header =="
	$(READELF) -h $(ELF)
	@echo "== Program headers =="
	$(READELF) -l $(ELF)
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
	@echo "== Required EF2SDK symbols =="
	@for sym in _start ef2_kernel_set_gs_crt ef2_gif_reset ef2_gif_send_qwords ef2_video_init ef2_video_clear; do \
		if ! $(NM) $(ELF) | grep -q " $$sym$$"; then \
			echo "ERROR: missing $$sym"; exit 1; \
		fi; \
	done
	@echo "== EF2SDK library symbols =="
	@for sym in ef2_audio_rate_converter_init ef2_audio_rate_converter_process_s16 ef2_audio_mix_s16; do \
		if ! $(NM) $(LIB) | grep -q " $sym$"; then \
			echo "ERROR: missing library symbol $sym"; exit 1; \
		fi; \
	done
	@echo "== Disassembly preview =="
	$(OBJDUMP) -d $(ELF) | head -n 160

package: clean all check
	./scripts/package.sh

toolchain-info:
	$(CC) --version
	$(READELF) --version | head -n 1

clean:
	rm -rf $(BUILD) dist
