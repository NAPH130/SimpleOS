#pragma once

namespace serial {
// COM1 是第一版内核的调试输出通道。
void init();
// 输出一个字符；换行时会先补充回车。
void put(char character);
void write(const char* text);
}
