#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/trap.hpp>
#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>

namespace {
constexpr char startup_banner[] = R"( ____  _                 _       ___  ____
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

        // 启动时登记异常入口。运行中发生异常时，CPU 会自行查找对应入口。
        x86_64::initialize_traps();

        // 主动触发一次断点。能打印 after int3 就说明 CPU 已恢复现场并继续执行。
        kprintf_log(LogLevel::DEBUG, "before int3\n");
        asm volatile("int3" ::: "memory");
        kprintf_log(LogLevel::DEBUG, "after int3\n");

        if (!boot::initialize_physical_memory()) {
            kprintf_log(LogLevel::ERROR, "Physical frame allocator initialization failed.\n");
            x86_64::halt();
        }
        kprintf_log(LogLevel::INFO, "Physical frame allocator initialized.\n");

        // TODO: 初始化内核堆及任务调度。
        kprintf_log(LogLevel::INFO, "Minimal kernel ready. Halting CPU.\n");
    } else {
        kprintf_log(LogLevel::ERROR, "Unsupported Limine base revision.\n");
    }

    x86_64::halt();
}
