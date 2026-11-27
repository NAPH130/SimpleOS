#pragma once

#include <stdint.h>

namespace mm {
class AddressSpace;
}

namespace task {

inline constexpr int page_fault_status = -14;

// 运行一个已经装载的用户程序。单 CPU 上一次运行一个，退出后回收其自有页表及映射。
// 调用前须准备 GDT、TSS 和 IDT，并保持可屏蔽中断关闭。
[[nodiscard]] bool run_process(mm::AddressSpace& space, uintptr_t entry, uintptr_t stack_top,
                               int& status);

// 仅在进程运行期间返回它的地址空间，供系统调用和异常处理使用。
mm::AddressSpace* current_address_space();

// 结束用户程序并把状态交还运行它的内核调用者。直接进入用户态的诊断会停机。
[[noreturn]] void exit_current(int status);

}
