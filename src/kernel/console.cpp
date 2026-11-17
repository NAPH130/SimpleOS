#include <kernel/console.hpp>
#include <drivers/serial.hpp>
#include <stdarg.h>
#include <stdint.h>

namespace {
void write_unsigned(uintptr_t value, unsigned base, bool uppercase = false) {
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    // 最多需要 uintptr_t 的位数个数字（加法无需堆内存）。
    char buffer[sizeof(uintptr_t) * 8];
    unsigned length = 0;
    do {
        buffer[length++] = digits[value % base];
        value /= base;
    } while (value != 0);
    while (length != 0) {
        serial::put(buffer[--length]);
    }
}

void write_signed(int value) {
    // 使用无符号运算取绝对值，避免 -INT_MIN 产生有符号溢出。
    unsigned magnitude = static_cast<unsigned>(value);
    if (value < 0) {
        serial::put('-');
        magnitude = 0u - magnitude;
    }
    write_unsigned(magnitude, 10);
}
}

void kprintf(const char* format, ...) {
    if (format == nullptr) {
        return;
    }

    va_list args;
    va_start(args, format);
    while (*format != '\0') {
        if (*format != '%') {
            serial::put(*format++);
            continue;
        }
        ++format;
        if (*format == '\0') {
            serial::put('%');
            break;
        }
        switch (*format++) {
            case '%': serial::put('%'); break;
            case 'c': serial::put(static_cast<char>(va_arg(args, int))); break;
            case 's': {
                const char* value = va_arg(args, const char*);
                serial::write(value != nullptr ? value : "(null)");
                break;
            }
            case 'd':
            case 'i': write_signed(va_arg(args, int)); break;
            case 'u': write_unsigned(va_arg(args, unsigned), 10); break;
            case 'x': write_unsigned(va_arg(args, unsigned), 16); break;
            case 'X': write_unsigned(va_arg(args, unsigned), 16, true); break;
            case 'p':
                serial::write("0x");
                write_unsigned(reinterpret_cast<uintptr_t>(va_arg(args, void*)), 16);
                break;
            default:
                // 未支持的格式原样输出，不读取额外参数。
                serial::put('%');
                serial::put(*(format - 1));
                break;
        }
    }
    va_end(args);
}
