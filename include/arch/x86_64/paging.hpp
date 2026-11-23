#pragma once

#include <stddef.h>
#include <stdint.h>

namespace x86_64::paging {

inline constexpr size_t page_size = 4096;
inline constexpr uint64_t page_present = 1ull << 0;
inline constexpr uint64_t page_writable = 1ull << 1;
inline constexpr uint64_t page_user = 1ull << 2;
inline constexpr uint64_t page_no_execute = 1ull << 63;
inline constexpr uint64_t page_address_mask = 0x000ffffffffff000;

constexpr size_t page_index(uintptr_t address, unsigned shift) {
    return (address >> shift) & 0x1ff;
}

// 检查当前页表中的整个用户范围。调用方须保证校验和读取期间映射保持稳定。
[[nodiscard]] bool user_buffer_readable(uintptr_t address, size_t size);

}
