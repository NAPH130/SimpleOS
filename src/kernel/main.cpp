#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>

namespace {
constexpr char startup_banner[] = R"(
 ____  _                 _       ___  ____
/ ___|(_)_ __ ___  _ __ | | ___ / _ \/ ___|
\___ \| | '_ ` _ \| '_ \| |/ _ \ | | \___ \
 ___) | | | | | | | |_) | |  __/ |_| |___) |
|____/|_|_| |_| |_| .__/|_|\___|\___/|____/
                  |_|

)";
}

// Limine 根据 ELF 入口地址进入此函数，C 链接避免 C++ 名字改编。
extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    serial::write(startup_banner);
    kprintf_log(LogLevel::INFO, "COM1 serial console initialized.\n");

    if (boot::limine_base_revision_supported()) {
        kprintf_log(LogLevel::INFO, "Limine base revision accepted.\n");
        // TODO: 接入中断和异常处理，随后初始化内存管理及任务调度。
        kprintf_log(LogLevel::INFO, "Minimal kernel ready. Halting CPU.\n");
    } else {
        kprintf_log(LogLevel::ERROR, "Unsupported Limine base revision.\n");
    }

    // 当前没有中断处理器或调度器；禁止中断并保持 CPU 停止。
    for (;;) {
        asm volatile("cli; hlt" ::: "memory");
    }
}
