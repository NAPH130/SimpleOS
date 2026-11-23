#include <kernel/console.hpp>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <string>

// 在宿主机捕获字符，测试内核中使用的同一份格式化代码。
namespace {
std::string output;
int failures = 0;

void expect(const char* name, const char* expected) {
    if (output != expected) {
        std::fprintf(stderr, "%s: expected '%s', got '%s'\n", name, expected, output.c_str());
        ++failures;
    }
    output.clear();
}
}

namespace serial {
void put(char character) { output += character; }
void write(const char* text) { output += text; }
}

int main() {
    kprintf("Hello, %s! %c %%", "kernel", 'A');
    expect("text", "Hello, kernel! A %");
    kprintf("%d %i %d %u %x %X", -42, 0, INT_MIN, UINT_MAX, 0xabcdefu, 0xabcdefu);
    expect("integers", "-42 0 -2147483648 4294967295 abcdef ABCDEF");
    kprintf("%p %p", static_cast<void*>(nullptr), reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234)));
    expect("pointers", "0x0 0x1234");
    kprintf("%s %q %", static_cast<const char*>(nullptr));
    expect("edge cases", "(null) %q %");
    kprintf(nullptr);
    expect("null format", "");

    kprintf_log(LogLevel::DEBUG, "trap vector %u\n", 3u);
    expect("debug log", "[\x1b[34mDEBUG\x1b[0m] trap vector 3\n");
    kprintf_log(LogLevel::INFO, "%s %u\n", "task", 3u);
    expect("info log", "[\x1b[32mINFO\x1b[0m] task 3\n");
    kprintf_log(LogLevel::WARN, "queue nearly full");
    expect("warn log", "[\x1b[33mWARN\x1b[0m] queue nearly full");
    kprintf_log(LogLevel::ERROR, "code %d", -1);
    expect("error log", "[\x1b[31mERROR\x1b[0m] code -1");
    kprintf_log(LogLevel::INFO, nullptr);
    expect("null log format", "");

    if (failures != 0) return 1;
    std::puts("Console formatting tests passed.");
    return 0;
}
