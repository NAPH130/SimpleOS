#include <abi/syscall.hpp>
#include <arch/x86_64/context.hpp>
#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/paging.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>
#include <kernel/syscall.hpp>

namespace kernel::syscall {
namespace {

intptr_t write(uint64_t fd, uintptr_t buffer, size_t size) {
    if (fd != abi::syscall::console_fd) {
        return abi::syscall::bad_file_descriptor;
    }
    if (size > INTPTR_MAX) {
        return abi::syscall::invalid_argument;
    }
    // 先检查全部页面，再发送数据，以免跨入无效页后发生部分输出。
    if (!x86_64::paging::user_buffer_readable(buffer, size)) {
        return abi::syscall::bad_address;
    }
    // CPU 仍使用当前页表。校验后可直接用原用户虚拟地址读出指定长度的字节。
    serial::write_bytes(reinterpret_cast<const char*>(buffer), size);
    return static_cast<intptr_t>(size);
}

[[noreturn]] void exit(int status) {
    // 恢复 run_user_mode 的内核调用现场，随后由启动方负责撤销用户映射。
    x86_64::exit_user_mode(status);
    // 通过底层入口直接进入 Ring 3 的程序没有内核调用现场可恢复。
    kprintf_log(LogLevel::INFO, "User program exited with status %d. Halting CPU.\n", status);
    x86_64::halt();
}

}

void dispatch(x86_64::TrapFrame& frame) {
    // CS 的低两位是进入中断前的特权级，可用来限制系统调用的来源。
    if ((frame.cs & 3) != 3) {
        frame.rax = abi::syscall::permission_denied;
        return;
    }

    switch (frame.rax) {
        case abi::syscall::write:
            // trap_entry.S 恢复 RAX 时会把这里写入的结果交还用户程序。
            frame.rax = write(frame.rdi, frame.rsi, frame.rdx);
            return;
        case abi::syscall::exit:
            exit(static_cast<int>(frame.rdi));
        default:
            frame.rax = abi::syscall::not_implemented;
    }
}

}
