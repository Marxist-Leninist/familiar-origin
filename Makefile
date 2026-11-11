CC = gcc
AS = as
LD = ld
OBJCOPY = objcopy
QEMU ?= qemu-system-i386
CFLAGS = -m32 -std=c11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-pie -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-sse -mno-mmx -msoft-float
.PHONY: all clean run test test-host test-boot
all: build/origin.img
build:
	mkdir -p build
build/boot.o: src/boot.S | build
	$(AS) --32 $< -o $@
build/boot.bin: build/boot.o
	$(LD) -m elf_i386 -Ttext 0x7c00 --oformat binary -e _start $< -o $@
build/entry.o: src/entry.S | build
	$(AS) --32 $< -o $@
build/%.o: src/%.c src/core.h src/gui.h | build
	$(CC) $(CFLAGS) -c $< -o $@
build/kernel.elf: build/entry.o build/kernel.o build/core.o build/gui.o src/linker.ld
	$(LD) -m elf_i386 -nostdlib -T src/linker.ld -Map build/kernel.map build/entry.o build/kernel.o build/core.o build/gui.o -o $@
build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
build/origin.img: build/boot.bin build/kernel.bin tools/mkimage.py
	python3 tools/mkimage.py
build/test_core: tests/test_core.c src/core.c src/core.h | build
	$(CC) -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Isrc tests/test_core.c src/core.c -o $@
test-host: build/test_core build/origin.img
	ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=0} ./build/test_core
	python3 tests/test_image.py
test-boot: all
	python3 tests/test_boot.py --qemu $(QEMU)
test: test-host test-boot
run: all
	$(QEMU) -accel tcg -machine pc -m 32M -drive file=build/origin.img,format=raw,if=ide -display none -serial stdio -monitor none -nic none -no-reboot
clean:
	rm -rf build
