#pragma once

// 支持 %s、%c、%d、%i、%u、%x、%X、%p 和 %%；不支持宽度或长度修饰。
// 调用前须初始化串口；暂不支持并发调用。
void kprintf(const char* format, ...);

enum class LogLevel {
    INFO,
    WARN,
    ERROR,
};

// 使用与 kprintf 相同的格式，先输出带颜色的级别标签。换行由 format 指定。
void kprintf_log(LogLevel level, const char* format, ...);
