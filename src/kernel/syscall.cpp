#include <abi/syscall.hpp>
#include <arch/x86_64/paging.hpp>
#include <drivers/serial.hpp>
#include <kernel/syscall.hpp>
#include <mm/address_space.hpp>
#include <task/process.hpp>

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

intptr_t allocate(size_t bytes) {
    auto* space = task::current_address_space();
    if (space == nullptr) {
        return abi::syscall::permission_denied;
    }
    if (bytes == 0 || bytes > SIZE_MAX - (x86_64::paging::page_size - 1)) {
        return abi::syscall::invalid_argument;
    }
    uintptr_t address;
    return space->allocate_user(bytes, address)
        ? static_cast<intptr_t>(address) : abi::syscall::out_of_memory;
}

intptr_t release(uintptr_t address) {
    auto* space = task::current_address_space();
    if (space == nullptr) {
        return abi::syscall::permission_denied;
    }
    return space->release_user(address) ? 0 : abi::syscall::invalid_argument;
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
            task::exit_current(static_cast<int>(frame.rdi));
        case abi::syscall::allocate:
            frame.rax = allocate(frame.rdi);
            return;
        case abi::syscall::release:
            frame.rax = release(frame.rdi);
            return;
        default:
            frame.rax = abi::syscall::not_implemented;
    }
}

}
