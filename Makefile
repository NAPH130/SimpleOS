SHELL := /bin/sh
.DELETE_ON_ERROR:

CXX := g++
LD := ld
BUILD := build
LIMINE := $(BUILD)/deps/limine
LIMINE_COMMIT := 7a9013f305de0bea1f6310e76e7baba30499fef0
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -g -ffreestanding \
    -fno-exceptions -fno-rtti -fno-stack-protector -fno-pie -fno-pic \
    -m64 -mno-red-zone -mcmodel=kernel -mgeneral-regs-only -Iinclude
OBJECTS := $(BUILD)/kernel/main.o $(BUILD)/kernel/console.o $(BUILD)/drivers/serial.o

.PHONY: all build run test clean
all: build
build: $(BUILD)/simpleos.iso

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T linker.ld $(OBJECTS) -o $@

# 拉取固定提交，避免上游分支变化导致构建行为改变。
$(LIMINE)/.ready:
	@mkdir -p $(LIMINE)
	git -C $(LIMINE) init
	git -C $(LIMINE) fetch --depth=1 https://github.com/limine-bootloader/limine.git $(LIMINE_COMMIT)
	git -C $(LIMINE) checkout --detach FETCH_HEAD
	$(MAKE) -C $(LIMINE)
	@touch $@

$(BUILD)/simpleos.iso: $(BUILD)/kernel.elf src/boot/limine.conf $(LIMINE)/.ready
	@mkdir -p $(BUILD)/iso/boot/limine
	cp $(BUILD)/kernel.elf $(BUILD)/iso/boot/kernel.elf
	cp src/boot/limine.conf $(BUILD)/iso/boot/limine/limine.conf
	cp $(LIMINE)/limine-bios.sys $(LIMINE)/limine-bios-cd.bin $(BUILD)/iso/boot/limine/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table $(BUILD)/iso -o $@
	$(LIMINE)/limine bios-install $@

run: build
	qemu-system-x86_64 -cdrom $(BUILD)/simpleos.iso -boot d -m 128M \
	    -display none -serial stdio -monitor none -no-reboot -no-shutdown

$(BUILD)/tests/console_test: tests/console/console_test.cpp src/kernel/console.cpp include/kernel/console.hpp include/drivers/serial.hpp
	@mkdir -p $(@D)
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Iinclude tests/console/console_test.cpp src/kernel/console.cpp -o $@

test: build $(BUILD)/tests/console_test
	$(BUILD)/tests/console_test
	sh scripts/test-boot.sh

clean:
	rm -rf $(BUILD)
