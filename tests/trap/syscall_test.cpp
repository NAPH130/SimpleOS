#include "test_boot.hpp"
#include "user_pages.hpp"
#include <mm/memory.hpp>
#include <task/process.hpp>

extern "C" const uint8_t syscall_user_begin[], syscall_user_end[];

namespace {

namespace paging = x86_64::paging;
// 覆盖两个程序的页表、代码、栈、系统调用分配的页和内核堆。
constexpr size_t probe_count = 32;

void sample_free_frames(uintptr_t (&frames)[probe_count]) {
    for (auto& frame : frames) {
        require_trap_test(mm::allocate_frame(frame), "Cannot sample free frames.");
    }
    for (auto frame : frames) {
        require_trap_test(mm::free_frame(frame), "Cannot return sampled frame.");
    }
}

void verify_reclamation(const uintptr_t (&before)[probe_count]) {
    // 分配器按物理地址找第一个空闲帧。前后的样本一致说明新分配的页都已归还。
    uintptr_t after[probe_count];
    sample_free_frames(after);
    for (size_t index = 0; index < probe_count; ++index) {
        require_trap_test(before[index] == after[index], "User mapping leaked a physical frame.");
    }
}

void write_stack_marker(const mm::AddressSpace& space, uintptr_t stack, uint64_t marker) {
    uintptr_t frame;
    require_trap_test(space.physical_frame(stack, frame), "Cannot get the user stack frame.");
    *static_cast<uint64_t*>(mm::physical_to_virtual(frame)) = marker;
}

int run_and_check(mm::AddressSpace& space, const user_pages::ProgramPages& pages, uintptr_t kernel_cr3) {
    int status;
    // 模拟 AMD64 函数入口的栈对齐。退出时由进程模块负责恢复 CR3 和回收内存。
    require_trap_test(task::run_process(space, pages.entry, pages.stack_top() - sizeof(uint64_t), status),
                      "Cannot launch user process.");
    require_trap_test(!space.owns_page_table() && paging::read_cr3() == kernel_cr3
                      && task::current_address_space() == nullptr
                      && !paging::user_buffer_readable(pages.entry, 1),
                      "Process exit did not restore the kernel address space.");
    return status;
}

void verify_processes() {
    user_pages::ProgramPages a, b;
    const uintptr_t kernel_cr3 = paging::read_cr3();
    uintptr_t before[probe_count];
    sample_free_frames(before);
    auto first = mm::AddressSpace::create_user(user_pages::arena_begin, user_pages::arena_end);
    auto second = mm::AddressSpace::create_user(user_pages::arena_begin, user_pages::arena_end);
    require_trap_test(first.owns_page_table() && second.owns_page_table()
                      && first.page_table_root() != second.page_table_root(),
                      "Cannot create separate page table roots.");
    require_trap_test(user_pages::prepare(first, syscall_user_begin, syscall_user_end, a)
                      && user_pages::prepare(second, syscall_user_begin, syscall_user_end, b),
                      "Cannot prepare syscall test pages.");

    uintptr_t first_frame, second_frame;
    require_trap_test(a.entry == b.entry && a.stack_page == b.stack_page
                      && first.physical_frame(a.stack_page, first_frame)
                      && second.physical_frame(b.stack_page, second_frame) && first_frame != second_frame
                      && !paging::user_buffer_readable(a.entry, 1),
                      "User processes did not get isolated mappings at identical addresses.");
    write_stack_marker(first, a.stack_page, 0x41);
    write_stack_marker(second, b.stack_page, 0x42);

    uintptr_t duplicate = 1;
    require_trap_test(!paging::map_user_page(first.page_table_root(), a.entry,
                      paging::UserPageAccess::read_write, duplicate) && duplicate == 1,
                      "Mapping replaced an existing user page.");

    kprintf_log(LogLevel::DEBUG, "before Ring 3 syscall test\n");
    const int normal = run_and_check(first, a, kernel_cr3);
    kprintf_log(LogLevel::INFO, "kernel resumed after sys_exit, status=%d\n", normal);
    require_trap_test(normal == 42, "Unexpected user exit status.");
    uintptr_t remaining;
    require_trap_test(second.physical_frame(b.stack_page, remaining) && remaining == second_frame,
                      "Exiting the first process damaged the second address space.");
    const int fault = run_and_check(second, b, kernel_cr3);
    require_trap_test(fault == task::page_fault_status, "User page fault did not end the process.");
    verify_reclamation(before);
    kprintf_log(LogLevel::INFO, "Isolated user address spaces reclaimed after exit and fault.\n");
}

void verify_region_reuse() {
    constexpr auto data = paging::UserPageAccess::read_write;
    constexpr size_t page = paging::page_size;
    constexpr uintptr_t begin = user_pages::arena_begin;
    constexpr uintptr_t external = begin + page;
    uintptr_t frame;
    require_trap_test(paging::map_user_page(external, data, frame), "Cannot map the external page.");
    {
        auto space = mm::AddressSpace::borrow_current_for_diagnostics(begin, begin + 5 * page);
        uintptr_t region, low, last;
        // 外部页让开头的空隙只剩一页。申请 4097 字节应取整并跳到外部页之后。
        require_trap_test(space.allocate(page + 1, data, region) && region == external + page
                          && paging::user_buffer_readable(region, 2 * page),
                          "Cannot find a contiguous two-page virtual range.");
        require_trap_test(space.allocate(page, data, low) && low == begin
                          && space.allocate(page, data, last) && last == begin + 4 * page,
                          "Cannot fill the remaining gaps.");
        uintptr_t result = 1;
        require_trap_test(!space.allocate(page, data, result) && result == 1
                          && !space.release(region + page) && !space.release(external),
                          "Accepted an invalid allocation or release.");
        require_trap_test(space.release(region) && !paging::user_buffer_readable(region, 1)
                          && !paging::user_buffer_readable(region + page, 1),
                          "Release did not remove both pages.");
        require_trap_test(space.allocate(2 * page, data, result) && result == region,
                          "Cannot reuse the freed two-page range.");
    }
    require_trap_test(!paging::user_buffer_readable(begin, 1)
                      && !paging::user_buffer_readable(external + page, 1)
                      && !paging::user_buffer_readable(begin + 4 * page, 1)
                      && paging::unmap_user_page(external), "Address space destruction lost ownership.");
}

void verify_allocation_rollback() {
    constexpr auto data = paging::UserPageAccess::read_write;
    constexpr uintptr_t anchor = user_pages::arena_begin;
    uintptr_t anchor_frame;
    require_trap_test(paging::map_user_page(anchor, data, anchor_frame), "Cannot map the anchor page.");
    uintptr_t frame, previous = 0, last = 0;
    while (mm::allocate_frame(frame)) {
        previous = last;
        last = frame;
    }
    auto unavailable = mm::AddressSpace::create_user(anchor, user_pages::arena_end);
    uintptr_t result = 1;
    require_trap_test(!unavailable.owns_page_table() && !unavailable.physical_frame(anchor, result)
                      && result == 1, "Failed address space creation borrowed the kernel page table.");
    // 归还两张页帧。它们不足以创建新分支，在已有分支中也只够一个描述符页和一个数据页。
    require_trap_test(mm::free_frame(previous) && mm::free_frame(last), "Cannot reserve test frames.");
    require_trap_test(!unavailable.allocate(paging::page_size, data, result) && result == 1,
                      "Failed address space construction did not leave an inert object.");
    constexpr uintptr_t other_branch = uintptr_t{1} << 39;
    require_trap_test(!paging::map_user_page(other_branch, data, result)
                      && result == 1 && !paging::user_buffer_readable(other_branch, 1),
                      "Failed mapping left a visible page or changed its result.");
    auto space = mm::AddressSpace::borrow_current_for_diagnostics(anchor, user_pages::arena_end);
    require_trap_test(!space.allocate(2 * paging::page_size, data, result) && result == 1
                      && !paging::user_buffer_readable(anchor + paging::page_size, 1)
                      && paging::user_buffer_readable(anchor, 1),
                      "Failed range allocation left a mapping or damaged the anchor page.");
    require_trap_test(mm::allocate_frame(result) && result == previous
                      && mm::allocate_frame(result) && result == last,
                      "Failed mapping did not return its data and metadata frames.");
    require_trap_test(paging::unmap_user_page(anchor), "Cannot release the anchor page.");
}

}

extern "C" [[noreturn]] void kernel_main() {
    initialize_trap_test();
    require_trap_test(boot::initialize_physical_memory(), "Cannot initialize physical memory.");
    verify_processes();
    verify_region_reuse();
    kprintf_log(LogLevel::INFO, "Virtual address ranges allocated and reused.\n");
    verify_allocation_rollback();
    kprintf_log(LogLevel::INFO, "User mapping rollback verified.\n");
    x86_64::halt();
}
