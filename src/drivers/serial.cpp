#include <drivers/serial.hpp>
#include <stdint.h>

namespace {
constexpr uint16_t com1 = 0x3f8;

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
    out8(com1 + 1, 0x00);
    out8(com1 + 3, 0x03); // 8 数据位，无校验，1 停止位。
    out8(com1 + 2, 0xc7); // 启用并清空 FIFO。
    out8(com1 + 4, 0x0b);
}

void serial::put(char character) {
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
