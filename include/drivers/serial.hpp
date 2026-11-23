#pragma once

#include <stddef.h>

namespace serial {
// COM1 是第一版内核的调试输出通道。
void init();
// 输出一个字符；换行时会先补充回车。
void put(char character);
void write(const char* text);
// 按长度原样发送，保留缓冲区内的零字节与换行。
void write_bytes(const char* buffer, size_t size);
}
