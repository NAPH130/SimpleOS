#include "test_boot.hpp"
#include "user_pages.hpp"
#include <arch/x86_64/context.hpp>
#include <mm/memory.hpp>

extern "C" const uint8_t syscall_user_begin[], syscall_user_end[];

namespace {

namespace paging = x86_64::paging;
// 覆盖两张用户数据页和五级页表下最多四张新页表页。
constexpr size_t probe_count = 6;

void sample_free_frames(uintptr_t (&frames)[probe_count]) {
    for (auto& frame : frames) {
        require_trap_test(mm::allocate_frame(frame), "Cannot sample free frames.");
    }
    for (auto frame : frames) {
        require_trap_test(mm::free_frame(frame), "Cannot return sampled frame.");
    }
}

void verify_reclamation(const uintptr_t (&before)[probe_count]) {
    require_trap_test(paging::unmap_user_page(user_pages::code_address), "Cannot unmap the user code.");
    require_trap_test(!paging::user_buffer_readable(user_pages::code_address, 1)
                      && paging::user_buffer_readable(user_pages::stack_address, 1),
                      "Unmapping user code affected the wrong page.");
    require_trap_test(paging::unmap_user_page(user_pages::stack_address), "Cannot unmap the user stack.");
    require_trap_test(!paging::user_buffer_readable(user_pages::stack_address, 1)
                      && !paging::unmap_user_page(user_pages::stack_address), "Stale user stack mapping.");

    // 分配器按物理地址找第一个空闲帧。前后的样本一致说明新分配的页都已归还。
    uintptr_t after[probe_count];
    sample_free_frames(after);
    for (size_t index = 0; index < probe_count; ++index) {
        require_trap_test(before[index] == after[index], "User mapping leaked a physical frame.");
    }
}

void verify_allocation_rollback() {
    uintptr_t frame, previous = 0, last = 0;
    while (mm::allocate_frame(frame)) {
        previous = last;
        last = frame;
    }
    // 空分支需要数据页和多张页表页。只归还两张，以触发中途分配失败。
    require_trap_test(mm::free_frame(previous) && mm::free_frame(last), "Cannot reserve test frames.");
    uintptr_t result = 1;
    constexpr uintptr_t address = user_pages::code_address;
    require_trap_test(!paging::map_user_page(address, paging::UserPageAccess::read_execute, result)
                      && result == 1 && !paging::user_buffer_readable(address, 1),
                      "Failed mapping left a visible page or changed its result.");
    require_trap_test(mm::allocate_frame(result) && result == previous
                      && mm::allocate_frame(result) && result == last,
                      "Failed mapping did not return its frames.");
}

}

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    require_trap_test(boot::initialize_physical_memory(), "Cannot initialize physical memory.");
    uintptr_t before[probe_count];
    sample_free_frames(before);
    require_trap_test(user_pages::prepare(syscall_user_begin, syscall_user_end),
                      "Cannot prepare syscall test pages.");

    uintptr_t duplicate = 1;
    require_trap_test(!paging::map_user_page(user_pages::code_address,
                      paging::UserPageAccess::read_write, duplicate) && duplicate == 1,
                      "Mapping replaced an existing user page.");

    // 模拟 AMD64 函数入口的栈对齐。入口函数最终调用 sys_exit。
    constexpr uintptr_t entry_stack = user_pages::stack_top - sizeof(uint64_t);
    kprintf_log(LogLevel::DEBUG, "before Ring 3 syscall test\n");
    const int status = x86_64::run_user_mode(user_pages::code_address, entry_stack);
    kprintf_log(LogLevel::INFO, "kernel resumed after sys_exit, status=%d\n", status);
    require_trap_test(status == 42, "Unexpected user exit status.");
    verify_reclamation(before);
    kprintf_log(LogLevel::INFO, "Dynamic user pages reclaimed.\n");
    verify_allocation_rollback();
    kprintf_log(LogLevel::INFO, "User mapping rollback verified.\n");
    x86_64::halt();
}
