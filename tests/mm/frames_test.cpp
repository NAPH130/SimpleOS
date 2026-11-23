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

void verify_reuse(uintptr_t direct_map) {
    uintptr_t first;
    uintptr_t second;
    require(mm::allocate_frame(first) && mm::allocate_frame(second), "Cannot allocate two frames.");
    require(first < second && first % page_size == 0 && second % page_size == 0,
            "Expected distinct, aligned frames in ascending order.");

    // 通过引导器的直接映射写入两个页帧，确认它们提供独立的物理存储。
    auto* first_value = reinterpret_cast<volatile uint64_t*>(direct_map + first);
    auto* second_value = reinterpret_cast<volatile uint64_t*>(direct_map + second);
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

}

extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    x86_64::initialize_traps();
    require(boot::limine_base_revision_supported() && boot::initialize_physical_memory(),
            "Cannot initialize the physical frame allocator.");
    uintptr_t direct_map;
    require(boot::higher_half_direct_map_offset(direct_map), "Missing direct map offset.");
    verify_reuse(direct_map);
    verify_exhaustion();
    kprintf_log(LogLevel::INFO, "Physical frame allocator self-test passed.\n");
    x86_64::halt();
}
