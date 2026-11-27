#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/paging.hpp>
#include <arch/x86_64/trap.hpp>
#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>
#include <mm/memory.hpp>

namespace {

using x86_64::paging::page_size;

void require(bool passed, const char* message) {
    if (!passed) {
        kprintf_log(LogLevel::ERROR, "%s\n", message);
        x86_64::halt();
    }
}

void verify_reuse() {
    uintptr_t first;
    uintptr_t second;
    require(mm::allocate_frame(first) && mm::allocate_frame(second), "Cannot allocate two frames.");
    require(first < second && first % page_size == 0 && second % page_size == 0,
            "Expected distinct, aligned frames in ascending order.");

    // 通过引导器的直接映射写入两个页帧，确认它们提供独立的物理存储。
    auto* first_value = static_cast<volatile uint64_t*>(mm::physical_to_virtual(first));
    auto* second_value = static_cast<volatile uint64_t*>(mm::physical_to_virtual(second));
    *first_value = 0x123456789abcdef0ull;
    *second_value = 0xfedcba9876543210ull;
    require(*first_value == 0x123456789abcdef0ull && *second_value == 0xfedcba9876543210ull,
            "Allocated frames do not hold distinct values.");

    require(!mm::free_frame(second + 1), "Accepted an unaligned address.");
    require(!mm::free_frame(UINTPTR_MAX & ~(page_size - 1)), "Accepted an unmanaged address.");
    require(mm::free_frame(first), "Cannot free the first frame.");
    require(!mm::free_frame(first), "Accepted a double free.");
    uintptr_t reused;
    require(mm::allocate_frame(reused) && reused == first, "Did not reuse the first free frame.");
    require(mm::free_frame(reused) && mm::free_frame(second), "Cannot release both frames.");
}

void verify_exhaustion() {
    // 独立镜像会在测试后停机，可以耗尽整个分配器来验证失败与释放后的行为。
    size_t count = 0;
    uintptr_t last = 0;
    uintptr_t frame;
    while (mm::allocate_frame(frame)) {
        require(frame % page_size == 0 && (count == 0 || frame > last),
                "Frame walk returned a duplicate, unaligned or unordered address.");
        ++count;
        last = frame;
    }
    require(count >= 2, "Expected multiple frames before exhaustion.");
    frame = 1;
    require(!mm::allocate_frame(frame) && frame == 1, "Exhaustion changed the output address.");
    require(mm::free_frame(last), "Cannot free a frame after exhaustion.");
    require(mm::allocate_frame(frame) && frame == last, "Cannot reuse a frame after exhaustion.");
}

void verify_kernel_heap() {
    // 2 页载荷加块头需要 3 张页帧。多取一张以检查整个内存段是否归还。
    uintptr_t before[4];
    for (auto& frame : before) {
        require(mm::allocate_frame(frame), "Cannot sample free heap frames.");
    }
    for (auto frame : before) {
        require(mm::free_frame(frame), "Cannot return sampled heap frames.");
    }

    void* first = mm::kmalloc(128);
    void* second = mm::kmalloc(128);
    void* last = mm::kmalloc(3600);
    require(first != nullptr && second != nullptr && last != nullptr
            && reinterpret_cast<uintptr_t>(first) % alignof(max_align_t) == 0,
            "Kernel heap allocations failed or were not aligned.");
    mm::kfree(first);
    mm::kfree(second);
    void* merged = mm::kmalloc(240);
    require(merged == first, "Kernel heap did not merge adjacent free blocks.");
    mm::kfree(merged);
    mm::kfree(last);

    auto* large = static_cast<volatile uint8_t*>(mm::kmalloc(2 * page_size));
    require(large != nullptr, "Kernel heap could not allocate a multi-page block.");
    large[0] = 0x12;
    large[2 * page_size - 1] = 0x34;
    require(large[0] == 0x12 && large[2 * page_size - 1] == 0x34,
            "Kernel heap multi-page block is not writable throughout.");
    mm::kfree(const_cast<uint8_t*>(large));
    require(mm::kmalloc(0) == nullptr && mm::kmalloc(SIZE_MAX) == nullptr,
            "Kernel heap accepted an invalid size.");
    uintptr_t after[4];
    for (size_t index = 0; index < 4; ++index) {
        require(mm::allocate_frame(after[index]) && after[index] == before[index],
                "Kernel heap leaked its pages.");
    }
    for (auto frame : after) {
        require(mm::free_frame(frame), "Cannot return sampled heap frames.");
    }
}

}

extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    x86_64::initialize_traps();
    require(boot::limine_base_revision_supported() && boot::initialize_physical_memory(),
            "Cannot initialize the physical frame allocator.");
    verify_reuse();
    verify_kernel_heap();
    kprintf_log(LogLevel::INFO, "Kernel heap self-test passed.\n");
    verify_exhaustion();
    kprintf_log(LogLevel::INFO, "Physical frame allocator self-test passed.\n");
    x86_64::halt();
}
