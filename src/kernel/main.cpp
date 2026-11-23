#include <boot/limine.hpp>
#include <drivers/serial.hpp>
#include <kernel/console.hpp>

// Limine 根据 ELF 入口地址进入此函数，C 链接避免 C++ 名字改编。
extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    if (boot::limine_base_revision_supported()) {
        kprintf("[SimpleOS] Hello, %s!\n", "kernel");
        // TODO: 接入中断和异常处理，随后初始化内存管理及任务调度。
    } else {
        kprintf("[SimpleOS] Unsupported Limine base revision\n");
    }

    // 当前没有中断处理器或调度器；禁止中断并保持 CPU 停止。
    for (;;) {
        asm volatile("cli; hlt" ::: "memory");
    }
}
