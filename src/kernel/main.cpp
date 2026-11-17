#include <drivers/serial.hpp>
#include <kernel/console.hpp>
#include <stdint.h>

// Limine 协议的请求标记与 base revision 3。引导器会将版本字段置零
// 表示支持该协议版本；volatile 确保读取引导器写入的值。
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = {
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf,
    0x785c6ed015d3e316, 0x181e920a7852b9d9
};

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = {
    0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 3
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = {
    0xadc0e0531bb10d03, 0x9572709f31764c62
};

// Limine 根据 ELF 入口地址进入此函数，C 链接避免 C++ 名字改编。
extern "C" [[noreturn]] void kernel_main() {
    serial::init();
    if (base_revision[2] != 0) {
        kprintf("[SimpleOS] Unsupported Limine base revision\n");
    } else {
        kprintf("[SimpleOS] Hello, %s!\n", "kernel");
    }

    // 当前没有中断处理器或调度器；禁止中断并保持 CPU 停止。
    for (;;) {
        asm volatile("cli; hlt" ::: "memory");
    }
}
