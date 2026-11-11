CC = gcc
AS = as
LD = ld
OBJCOPY = objcopy
ARCH ?= 64
BUILD = build$(ARCH)
CFLAGS = -std=c11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables
ifeq ($(ARCH),64)
CFLAGS += -m64 -mno-red-zone -msse2 -mfpmath=sse
ENTRY = src/entry64.S
LDFLAGS = -m elf_x86_64
QEMU ?= qemu-system-x86_64
else
CFLAGS += -m32 -mno-sse -mno-mmx -mfpmath=387
ENTRY = src/entry.S
LDFLAGS = -m elf_i386
QEMU ?= qemu-system-i386
endif
.PHONY: all compat32 test test-host test-boot test-nn test-gui run
all: $(BUILD)/origin.img
compat32:
	$(MAKE) ARCH=32 all
$(BUILD) build:
	mkdir -p $@
$(BUILD)/boot.o: src/boot.S | $(BUILD)
	$(AS) --32 $< -o $@
$(BUILD)/boot.bin: $(BUILD)/boot.o
	$(LD) -m elf_i386 -Ttext 0x7c00 --oformat binary -e _start $< -o $@
$(BUILD)/entry.o: $(ENTRY) | $(BUILD)
	$(AS) --$(ARCH) $< -o $@
$(BUILD)/%.o: src/%.c src/core.h src/gui.h src/nn.h src/lang.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/kernel.elf: $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/core.o $(BUILD)/gui.o $(BUILD)/nn.o $(BUILD)/lang.o src/linker.ld
	$(LD) $(LDFLAGS) -nostdlib -T src/linker.ld -Map $(BUILD)/kernel.map $(filter %.o,$^) -o $@
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@
$(BUILD)/origin.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin tools/mkimage.py
	python3 tools/mkimage.py $(BUILD)
build/test_core: tests/test_core.c src/core.c src/core.h | build
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc tests/test_core.c src/core.c -o $@
build/test_nn: tests/test_nn.c src/nn.c src/nn.h | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc tests/test_nn.c src/nn.c -o $@
build/test_gui: tests/test_gui.c src/gui.c src/gui.h src/core.c | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -g -DORIGIN_GUI_TEST -Isrc tests/test_gui.c src/gui.c src/core.c -o $@
test-host: build/test_core all
	ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=0} ./build/test_core
	python3 tests/test_image.py $(BUILD)
test-nn: build/test_nn
	ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=0} ./build/test_nn
test-gui: build/test_gui
	ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=0} ./build/test_gui
test-boot: all
	python3 tests/test_boot.py --qemu $(QEMU) --build-dir $(BUILD) --arch $(ARCH)
test: test-host test-nn test-gui test-lang test-boot
run: all
	$(QEMU) -accel tcg -machine pc -m 32M -drive file=$(BUILD)/origin.img,format=raw,if=ide -display none -serial stdio -monitor none -nic none -no-reboot

build/test_lang: tests/test_lang.c src/lang.c src/lang.h src/core.c | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc tests/test_lang.c src/lang.c src/core.c -o $@
test-lang: build/test_lang
	ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=0} ./build/test_lang
