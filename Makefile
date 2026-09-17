CROSS ?= mips64r5900el-ps2-elf-
CC := $(CROSS)gcc
NM := $(CROSS)nm
READELF := $(CROSS)readelf
OBJDUMP := $(CROSS)objdump

BUILD := build
ELF := $(BUILD)/ef2-boot.elf
MAP := $(BUILD)/ef2-boot.map

CFLAGS := -G0 -O2 -Wall -Wextra -Werror \
          -ffreestanding -fno-builtin -fno-stack-protector \
          -Iinclude

LDFLAGS := -nostdlib -nostartfiles -nodefaultlibs \
           -T ld/ee.ld \
           -Wl,-Map,$(MAP) \
           -Wl,-zmax-page-size=128 \
           -Wl,--build-id=none

OBJS := \
    $(BUILD)/start.o \
    $(BUILD)/boot.o

.PHONY: all clean check package toolchain-info

all: $(ELF)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: src/ee/runtime/start.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/boot.o: examples/boot/main.c include/ef2/base.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) ld/ee.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

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
	@echo "== Disassembly preview =="
	$(OBJDUMP) -d $(ELF) | head -n 80

package: clean all check
	./scripts/package.sh

toolchain-info:
	$(CC) --version
	$(READELF) --version | head -n 1

clean:
	rm -rf $(BUILD) dist
