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
HOST_HEAP_TEST := $(BUILD)/heap-test
HOST_LIBC_TEST := $(BUILD)/libc-test
HOST_SETJMP_TEST := $(BUILD)/setjmp-test
HOST_FORMAT_TEST := $(BUILD)/format-test
HOST_STDIO_TEST := $(BUILD)/stdio-test
HOST_TIMER_TEST := $(BUILD)/timer-test
HOST_PROFILE_TEST := $(BUILD)/profile-test
HOST_ZLIB_TEST := $(BUILD)/zlib-host-test
ZLIB_TARGET_DIR := $(BUILD)/ports/zlib-target
ZLIB_HOST_DIR := $(BUILD)/ports/zlib-host
ZLIB_TARGET_LIB := $(ZLIB_TARGET_DIR)/libz.a
ZLIB_HOST_LIB := $(ZLIB_HOST_DIR)/libz.a
ZLIB_LINK_TEST := $(BUILD)/zlib-link-test.elf
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
    $(BUILD)/runtime.o \
    $(BUILD)/heap.o \
    $(BUILD)/heap_default.o \
    $(BUILD)/libc_memory.o \
    $(BUILD)/libc_alloc.o \
    $(BUILD)/libc_format.o \
    $(BUILD)/libc_stdio.o \
    $(BUILD)/setjmp.o \
    $(BUILD)/syscall.o \
    $(BUILD)/interrupt.o \
    $(BUILD)/timer.o \
    $(BUILD)/profile.o \
    $(BUILD)/cache.o \
    $(BUILD)/debug_sio.o \
    $(BUILD)/debug_log.o \
    $(BUILD)/crash_vector.o \
    $(BUILD)/crash.o \
    $(BUILD)/sif.o \
    $(BUILD)/memorycard.o \
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

.PHONY: all clean check host-test ports package toolchain-info

all: $(ELF) $(LIB) $(IOP_AUDIO_IRX) $(IOP_PAD_IRX)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: src/ee/runtime/start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/runtime.o: src/ee/runtime/runtime.c include/ef2/base.h include/ef2/heap.h include/ef2/kernel.h include/ef2/runtime.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/heap.o: src/ee/runtime/heap.c include/ef2/base.h include/ef2/heap.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/heap_default.o: src/ee/runtime/heap_default.c include/ef2/base.h include/ef2/heap.h include/ef2/kernel.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libc_memory.o: src/ee/runtime/libc_memory.c include/ef2/base.h include/ef2/libc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libc_alloc.o: src/ee/runtime/libc_alloc.c include/ef2/base.h include/ef2/heap.h include/ef2/libc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libc_format.o: src/ee/runtime/libc_format.c include/ef2/base.h include/ef2/libc.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libc_stdio.o: src/ee/runtime/libc_stdio.c include/ef2/heap.h include/ef2/libc.h include/ef2/stdio.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/setjmp.o: src/ee/runtime/setjmp.c include/ef2/compat/setjmp.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: src/ee/kernel/syscall.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/interrupt.o: src/ee/kernel/interrupt.c include/ef2/interrupt.h include/ef2/kernel.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/timer.o: src/ee/kernel/timer.c include/ef2/timer.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/profile.o: src/ee/kernel/profile.c include/ef2/profile.h include/ef2/timer.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cache.o: src/ee/kernel/cache.c include/ef2/cache.h include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/debug_sio.o: src/ee/kernel/debug_sio.c include/ef2/debug.h include/ef2/libc.h include/ef2/stdio.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/debug_log.o: src/ee/runtime/debug_log.c include/ef2/debug.h include/ef2/libc.h include/ef2/stdio.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crash_vector.o: src/ee/runtime/crash_vector.S src/ee/runtime/crash_defs.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crash.o: src/ee/runtime/crash.c include/ef2/crash.h include/ef2/debug.h include/ef2/kernel.h include/ef2/video.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sif.o: src/ee/sif/sif.c include/ef2/base.h include/ef2/kernel.h include/ef2/sif.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/memorycard.o: src/ee/storage/memorycard.c include/ef2/base.h include/ef2/cache.h include/ef2/memorycard.h include/ef2/sif.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gif.o: src/ee/gs/gif.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gif_dma.o: src/ee/gs/gif_dma.c include/ef2/cache.h include/ef2/gif.h include/ef2/profile.h include/ef2/timer.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/video.o: src/ee/gs/video.c include/ef2/base.h include/ef2/gif.h include/ef2/gs.h include/ef2/kernel.h include/ef2/profile.h include/ef2/sif.h include/ef2/video.h | $(BUILD)
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

$(BUILD)/boot.o: examples/boot/main.c include/ef2/audio.h include/ef2/base.h include/ef2/crash.h include/ef2/debug.h include/ef2/gif.h include/ef2/interrupt.h include/ef2/kernel.h include/ef2/memorycard.h include/ef2/pad.h include/ef2/sif.h include/ef2/timer.h include/ef2/video.h | $(BUILD)
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

$(HOST_HEAP_TEST): tests/heap_test.c src/ee/runtime/heap.c include/ef2/heap.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/heap_test.c src/ee/runtime/heap.c -o $@

$(HOST_LIBC_TEST): tests/libc_test.c src/ee/runtime/libc_memory.c include/ef2/libc.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin \
		-DEF2_LIBC_NO_STANDARD_ALIASES -Iinclude \
		tests/libc_test.c src/ee/runtime/libc_memory.c -o $@

$(ZLIB_HOST_LIB): ports/zlib/build.sh ports/zlib/ef2_zutil.c | $(BUILD)
	CC="$(HOSTCC)" AR="ar" ./ports/zlib/build.sh host $(ZLIB_HOST_DIR)

$(HOST_SETJMP_TEST): tests/setjmp_test.c src/ee/runtime/setjmp.c include/ef2/compat/setjmp.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-Iinclude/ef2/compat -Iinclude \
		tests/setjmp_test.c src/ee/runtime/setjmp.c -o $@

$(HOST_FORMAT_TEST): tests/format_test.c src/ee/runtime/libc_format.c include/ef2/libc.h include/ef2/compat/stdio.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin \
		-Iinclude/ef2/compat -Iinclude \
		tests/format_test.c src/ee/runtime/libc_format.c -o $@

$(HOST_STDIO_TEST): tests/stdio_test.c src/ee/runtime/libc_stdio.c src/ee/runtime/libc_format.c src/ee/runtime/libc_memory.c src/ee/runtime/heap.c include/ef2/heap.h include/ef2/libc.h include/ef2/stdio.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -fno-builtin \
		-DEF2_LIBC_NO_STANDARD_ALIASES -Iinclude \
		tests/stdio_test.c src/ee/runtime/libc_stdio.c \
		src/ee/runtime/libc_format.c src/ee/runtime/libc_memory.c \
		src/ee/runtime/heap.c -o $@

$(HOST_TIMER_TEST): tests/timer_test.c include/ef2/timer.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/timer_test.c -o $@

$(HOST_PROFILE_TEST): tests/profile_test.c src/ee/kernel/profile.c include/ef2/profile.h include/ef2/timer.h include/ef2/base.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/profile_test.c src/ee/kernel/profile.c -o $@

$(HOST_ZLIB_TEST): tests/zlib_test.c $(ZLIB_HOST_LIB) | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror \
		-I$(ZLIB_HOST_DIR) tests/zlib_test.c $(ZLIB_HOST_LIB) -o $@

host-test: $(HOST_AUDIO_TEST) $(HOST_PAD_TEST) $(HOST_HEAP_TEST) $(HOST_LIBC_TEST) $(HOST_SETJMP_TEST) $(HOST_FORMAT_TEST) $(HOST_STDIO_TEST) $(HOST_TIMER_TEST) $(HOST_PROFILE_TEST) $(HOST_ZLIB_TEST)
	$(HOST_AUDIO_TEST)
	$(HOST_PAD_TEST)
	$(HOST_HEAP_TEST)
	$(HOST_LIBC_TEST)
	$(HOST_SETJMP_TEST)
	$(HOST_FORMAT_TEST)
	$(HOST_STDIO_TEST)
	$(HOST_TIMER_TEST)
	$(HOST_PROFILE_TEST)
	$(HOST_ZLIB_TEST)

$(ZLIB_TARGET_LIB): ports/zlib/build.sh ports/zlib/ef2_zutil.c | $(BUILD)
	CC="$(CC)" AR="$(AR)" ./ports/zlib/build.sh target $(ZLIB_TARGET_DIR)

$(BUILD)/zlib_link_test.o: tests/zlib_test.c $(ZLIB_TARGET_LIB) | $(BUILD)
	$(CC) $(CFLAGS) -I$(ZLIB_TARGET_DIR) -c tests/zlib_test.c -o $@

$(ZLIB_LINK_TEST): $(BUILD)/start.o $(BUILD)/zlib_link_test.o $(ZLIB_TARGET_LIB) $(LIB) ld/ee.ld
	$(CC) $(LDFLAGS) $(BUILD)/start.o $(BUILD)/zlib_link_test.o \
		$(ZLIB_TARGET_LIB) $(LIB) -o $@
	@if $(NM) -u $@ | grep -q .; then \
		echo "ERROR: zlib port has unresolved target symbols"; \
		$(NM) -u $@; \
		exit 1; \
	fi

ports: $(ZLIB_TARGET_LIB) $(ZLIB_LINK_TEST)

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
	@for sym in ef2_runtime_get_args ef2_runtime_exit ef2_runtime_abort ef2_heap_init ef2_heap_init_default ef2_malloc ef2_free ef2_calloc ef2_realloc ef2_heap_get_stats ef2_memcpy ef2_memmove ef2_memset ef2_memcmp ef2_strlen ef2_strcmp ef2_vsnprintf ef2_snprintf ef2_vfprintf ef2_fprintf ef2_vprintf ef2_printf ef2_puts ef2_stdio_set_stdout ef2_debug_init ef2_debug_printf ef2_debug_write_raw ef2_debug_copy_recent ef2_debug_get_stats ef2_debug_use_sio_stdio ef2_crash_install ef2_crash_is_installed ef2_crash_trigger_test ef2_interrupt_suspend ef2_interrupt_add_intc ef2_kernel_create_thread ef2_kernel_get_thread_id ef2_kernel_refer_thread_status ef2_kernel_create_sema ef2_kernel_poll_sema ef2_kernel_signal_sema ef2_kernel_delete_sema ef2_kernel_get_cop0 ef2_kernel_machine_type ef2_kernel_get_memory_size ef2_timer_configure ef2_timer_start ef2_timer_get_count ef2_cpu_count ef2_profile_reset ef2_profile_record ef2_gif_dma_reset_stats ef2_gif_dma_get_stats ef2_video_init ef2_video_set_double_buffering ef2_video_wait_vsync ef2_video_present ef2_video_get_frame_stats ef2_video_draw_rect ef2_video_draw_line ef2_video_get_size ef2_video_upload_rgba32 ef2_video_upload_indexed8 ef2_video_upload_indexed4 ef2_video_pack_indices4 ef2_video_draw_texture ef2_video_draw_texture_region ef2_video_get_texture_vram_free ef2_gif_dma_send_qwords ef2_cache_writeback_invalidate_range ef2_audio_rate_converter_init ef2_audio_rate_converter_process_s16 ef2_sif_init ef2_iop_get_romver ef2_mc_init ef2_mc_get_info ef2_mc_get_diag ef2_video_detect_standard ef2_video_get_config ef2_video_get_framebuffer_layout ef2_audio_device_init ef2_audio_device_start ef2_pad_init ef2_pad_poll ef2_pad_poll_all ef2_pad_poll_slot ef2_pad_get_slot_count ef2_pad_set_rumble ef2_pad_set_rumble_slot ef2_pad_stop_rumble ef2_pad_is_held ef2_pad_axis_deadzone; do \
		if ! $(NM) $(LIB) | grep -q " $$sym$$"; then \
			echo "ERROR: missing library symbol $$sym"; exit 1; \
		fi; \
	done
	@test -s $(IOP_AUDIO_IRX)
	@test -s $(IOP_PAD_IRX)
	@echo "== Disassembly preview =="
	$(OBJDUMP) -d $(ELF) | head -n 180

package: clean all ports check
	./scripts/package.sh

toolchain-info:
	$(CC) --version
	$(READELF) --version | head -n 1
	@mipsel-none-elf-gcc --version | head -n 1

clean:
	rm -rf $(BUILD) dist \
		$(IOP_AUDIO_DIR)/obj $(IOP_AUDIO_DIR)/irx \
		$(IOP_PAD_DIR)/obj $(IOP_PAD_DIR)/irx
