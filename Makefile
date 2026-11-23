SHELL := /bin/sh
.DELETE_ON_ERROR:

CC := gcc
CXX := g++
LD := ld
BUILD := build
LIMINE := $(BUILD)/deps/limine
LIMINE_COMMIT := 7a9013f305de0bea1f6310e76e7baba30499fef0
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -g -ffreestanding \
    -fno-exceptions -fno-rtti -fno-stack-protector -fno-pie -fno-pic \
    -m64 -mno-red-zone -mcmodel=kernel -mgeneral-regs-only -Iinclude
SOURCES := src/boot/limine.cpp src/kernel/main.cpp src/kernel/console.cpp src/drivers/serial.cpp \
    src/arch/x86_64/idt.cpp src/arch/x86_64/trap.cpp
ASM_SOURCES := src/arch/x86_64/trap_entry.S
OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SOURCES)) \
    $(patsubst src/%.S,$(BUILD)/%.o,$(ASM_SOURCES))
UD_TEST_OBJECT := $(BUILD)/tests/trap/ud_test.o
UD_TEST_ISO := $(BUILD)/tests/trap-ud.iso

.PHONY: all build run test test-boot test-ud clean
all: build
build: $(BUILD)/simpleos.iso

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: src/%.S
	@mkdir -p $(@D)
	$(CC) -m64 -g -Iinclude -MMD -MP -c $< -o $@

$(UD_TEST_OBJECT): tests/trap/ud_test.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d) $(UD_TEST_OBJECT:.o=.d)

$(BUILD)/kernel.elf: $(OBJECTS)
$(BUILD)/tests/trap-ud.elf: $(filter-out $(BUILD)/kernel/main.o,$(OBJECTS)) $(UD_TEST_OBJECT)
$(BUILD)/kernel.elf $(BUILD)/tests/trap-ud.elf: linker.ld
	$(LD) -nostdlib -static -z max-page-size=0x1000 -T linker.ld $(filter %.o,$^) -o $@

# 拉取固定提交，避免上游分支变化导致构建行为改变。
$(LIMINE)/.ready:
	@mkdir -p $(LIMINE)
	git -C $(LIMINE) init
	git -C $(LIMINE) fetch --depth=1 https://github.com/limine-bootloader/limine.git $(LIMINE_COMMIT)
	git -C $(LIMINE) checkout --detach FETCH_HEAD
	$(MAKE) -C $(LIMINE)
	@touch $@

$(BUILD)/simpleos.iso: $(BUILD)/kernel.elf
$(UD_TEST_ISO): $(BUILD)/tests/trap-ud.elf
$(BUILD)/simpleos.iso $(UD_TEST_ISO): Makefile src/boot/limine.conf $(LIMINE)/.ready
	@mkdir -p $(@:.iso=-root)/boot/limine
	cp $(filter %.elf,$^) $(@:.iso=-root)/boot/kernel.elf
	cp src/boot/limine.conf $(@:.iso=-root)/boot/limine/limine.conf
	cp $(LIMINE)/limine-bios.sys $(LIMINE)/limine-bios-cd.bin $(@:.iso=-root)/boot/limine/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table $(@:.iso=-root) -o $@
	$(LIMINE)/limine bios-install $@

run: build
	qemu-system-x86_64 -cdrom $(BUILD)/simpleos.iso -boot d -m 128M \
	    -display none -serial stdio -monitor none -no-reboot -no-shutdown

$(BUILD)/tests/console_test: tests/console/console_test.cpp src/kernel/console.cpp include/kernel/console.hpp include/drivers/serial.hpp
	@mkdir -p $(@D)
	$(CXX) -std=c++17 -Wall -Wextra -Werror -Iinclude tests/console/console_test.cpp src/kernel/console.cpp -o $@

test: $(BUILD)/tests/console_test test-boot test-ud
	$(BUILD)/tests/console_test

test-boot: build
	sh scripts/test-boot.sh boot $(BUILD)

test-ud: $(UD_TEST_ISO)
	sh scripts/test-boot.sh ud $(BUILD)

clean:
	rm -rf $(BUILD)
