#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/gdt.hpp>
#include <arch/x86_64/trap.hpp>
#include <arch/x86_64/vectors.h>
#include <kernel/console.hpp>
#include <kernel/syscall.hpp>
#include <task/process.hpp>
#include <stddef.h>

namespace x86_64 {

// 汇编直接在栈上构造现场，在编译期核对 C++ 看到的关键字段偏移。
static_assert(offsetof(TrapFrame, vector) == 120);
static_assert(offsetof(TrapFrame, error_code) == 128);
static_assert(offsetof(TrapFrame, rip) == 136);
static_assert(sizeof(TrapFrame) == 176);

namespace {

constexpr const char* exception_names[] = {
#define CPU_EXCEPTION(vector, error, name) name,
#include <arch/x86_64/exception_vectors.inc>
#undef CPU_EXCEPTION
};
static_assert(sizeof(exception_names) / sizeof(exception_names[0]) == 32);

[[noreturn]] void handle_fault(const TrapFrame& frame) {
    const auto vector = static_cast<unsigned>(frame.vector);
    uintptr_t fault_address = 0;
    if (vector == X86_64_VECTOR_PAGE_FAULT) {
        // RIP 定位出错的指令，CR2 记录它试图访问的地址。
        // 后续缺页会覆盖 CR2，因此在打印前先读取它。
        asm volatile("mov %%cr2, %0" : "=r"(fault_address));
        if ((frame.cs & 3) == 3 && task::current_address_space() != nullptr) {
            kprintf_log(LogLevel::ERROR, "User page fault at %p. Terminating process.\n",
                        reinterpret_cast<void*>(fault_address));
            task::exit_current(task::page_fault_status);
        }
    }

    // CPU 保存的 CS 低两位给出异常发生前的特权级，frame 的地址给出实际使用的内核栈。
    if ((frame.cs & 3) == 3) {
        kprintf_log(LogLevel::DEBUG, "Ring 3 trap: CS=0x%x, SS=0x%x, RSP=%p, on RSP0 stack=%s.\n",
                    static_cast<unsigned>(frame.cs), static_cast<unsigned>(frame.ss),
                    reinterpret_cast<void*>(frame.rsp),
                    on_ring0_entry_stack(&frame, sizeof(frame)) ? "yes" : "no");
    }

    const auto error = static_cast<unsigned>(frame.error_code);
    void* const rip = reinterpret_cast<void*>(frame.rip);
    const char* name = exception_names[vector];
    if (vector == X86_64_VECTOR_PAGE_FAULT) {
        kprintf_log(LogLevel::ERROR, "%s (vector %u), error=0x%x, RIP=%p, CR2=%p. Halting CPU.\n",
                    name, vector, error, rip, reinterpret_cast<void*>(fault_address));
    } else if (vector == X86_64_VECTOR_INVALID_OPCODE) {
        kprintf_log(LogLevel::ERROR, "%s (vector %u), RIP=%p. Halting CPU.\n", name, vector, rip);
    } else {
        kprintf_log(LogLevel::ERROR, "%s (vector %u), error=0x%x, RIP=%p. Halting CPU.\n",
                    name, vector, error, rip);
    }
    // 内核故障和单独的异常诊断走停机路径。
    halt();
}

}

extern "C" void trap_dispatch(TrapFrame* frame) {
    switch (frame->vector) {
        case X86_64_VECTOR_SYSCALL:
            // sys_write 等调用把结果留在 TrapFrame 中，由公共入口恢复给用户。
            kernel::syscall::dispatch(*frame);
            return;
        case X86_64_VECTOR_BREAKPOINT:
            kprintf_log(LogLevel::DEBUG, "breakpoint handled\n");
            // int3 保存的 RIP 指向下一条指令，交给汇编恢复现场并返回。
            return;
        case X86_64_VECTOR_DOUBLE_FAULT:
            // 用栈帧自身的地址确认 CPU 实际进入了预留的 IST 应急栈。
            kprintf_log(LogLevel::ERROR, "#DF Double fault (vector 8), error=0x%x, IST=%s. Halting CPU.\n",
                        static_cast<unsigned>(frame->error_code),
                        on_double_fault_stack(frame) ? "yes" : "no");
            halt();
        default:
            if (frame->vector < sizeof(exception_names) / sizeof(exception_names[0])) {
                handle_fault(*frame);
            }
            kprintf_log(LogLevel::ERROR, "Unhandled trap vector %u\n", static_cast<unsigned>(frame->vector));
            break;
    }
    halt();
}

}
