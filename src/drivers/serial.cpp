#include <drivers/serial.hpp>
#include <stdint.h>

namespace {
constexpr uint16_t com1 = 0x3f8; // UART 的 I/O 端口基址。

void out8(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t in8(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void put_raw(char character) {
    // 等待发送缓冲区为空，避免连续输出时丢失字符。
    while ((in8(com1 + 5) & 0x20) == 0) {
        asm volatile("pause");
    }
    out8(com1, static_cast<uint8_t>(character));
}
}

void serial::init() {
    out8(com1 + 1, 0x00); // 禁用串口中断。
    out8(com1 + 3, 0x80); // 打开波特率除数寄存器。
    out8(com1 + 0, 0x03); // 除数为 3：38400 baud。
    out8(com1 + 1, 0x00); // 高字节为零。此时端口 1 仍处于除数寄存器模式。
    out8(com1 + 3, 0x03); // 8 数据位，无校验，1 停止位。
    out8(com1 + 2, 0xc7); // 启用并清空 FIFO。
    out8(com1 + 4, 0x0b); // 设置调制解调器控制信号，保持上面的中断关闭状态。
}

void serial::put(char character) {
    // 内核文本日志在串口终端上用 CRLF 换到下一行行首。
    if (character == '\n') {
        put_raw('\r');
    }
    put_raw(character);
}

void serial::write(const char* text) {
    while (*text != '\0') {
        put(*text++);
    }
}

void serial::write_bytes(const char* buffer, size_t size) {
    // 系统调用传入的是字节数量。按长度发送可以保留中间的零字节。
    for (size_t offset = 0; offset < size; ++offset) {
        put_raw(buffer[offset]);
    }
}
