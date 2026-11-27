#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/paging.hpp>
#include <kernel/console.hpp>
#include <mm/address_space.hpp>
#include <task/process.hpp>

namespace task {
namespace {

struct ProcessRun {
    mm::AddressSpace& space;
    uintptr_t previous_cr3;
    x86_64::UserReturnContext context;
};

// 系统调用和异常处理从这里找到当前的运行记录。记录本身由 run_process 持有。
ProcessRun* current_run = nullptr;

}

mm::AddressSpace* current_address_space() {
    return current_run == nullptr ? nullptr : &current_run->space;
}

bool run_process(mm::AddressSpace& space, uintptr_t entry, uintptr_t stack_top, int& status) {
    if (current_run != nullptr || !space.owns_page_table()
        || space.page_table_root() == x86_64::paging::current_page_table_root()) {
        return false;
    }

    ProcessRun run{space, x86_64::paging::read_cr3(), {}};
    current_run = &run;
    x86_64::paging::write_cr3(space.page_table_root());
    const int result = x86_64::run_user_mode(run.context, entry, stack_top);

    // sys_exit 和用户缺页都回到内核栈。切离进程页表之后才能归还页表根。
    x86_64::paging::write_cr3(run.previous_cr3);
    current_run = nullptr;
    if (!space.destroy()) {
        kprintf_log(LogLevel::ERROR, "Cannot destroy process address space.\n");
        x86_64::halt();
    }
    status = result;
    return true;
}

[[noreturn]] void exit_current(int status) {
    if (current_run != nullptr) {
        x86_64::exit_user_mode(current_run->context, status);
    }
    // 仅通过 enter_user_mode 启动的诊断没有保存内核调用现场。
    kprintf_log(LogLevel::INFO, "User program exited with status %d. Halting CPU.\n", status);
    x86_64::halt();
}

}
