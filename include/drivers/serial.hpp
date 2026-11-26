#pragma once

#include <stddef.h>

namespace serial {
// 初始化轮询发送的 COM1。其他串口函数需在其后调用。
void init();
// 输出一个字符；换行时会先补充回车。
void put(char character);
// 发送以零字节结尾的内核文本，换行与 put 的规则相同。
void write(const char* text);
// 按长度原样发送，保留缓冲区内的零字节与换行。
void write_bytes(const char* buffer, size_t size);
}
