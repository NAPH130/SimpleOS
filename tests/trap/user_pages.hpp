#pragma once

#include "test_boot.hpp"
#include <arch/x86_64/paging.hpp>

namespace user_pages {

inline constexpr uintptr_t code_address = 0x400000;
inline constexpr uintptr_t stack_address = code_address + x86_64::paging::page_size;
inline constexpr uintptr_t stack_top = stack_address + x86_64::paging::page_size;

// 两个测试镜像都使用内核的正常页帧分配器。在退出到内核后可分别撤销两张映射。
inline bool prepare(const uint8_t* program_begin, const uint8_t* program_end) {
    namespace paging = x86_64::paging;
    const size_t size = reinterpret_cast<uintptr_t>(program_end)
        - reinterpret_cast<uintptr_t>(program_begin);
    uintptr_t direct_map;
    if (size == 0 || size > paging::page_size || !boot::higher_half_direct_map_offset(direct_map)) {
        return false;
    }

    uintptr_t code_frame, stack_frame;
    if (!paging::map_user_page(code_address, paging::UserPageAccess::read_execute, code_frame)) {
        return false;
    }
    if (!paging::map_user_page(stack_address, paging::UserPageAccess::read_write, stack_frame)) {
        require_trap_test(paging::unmap_user_page(code_address), "Cannot roll back user code page.");
        return false;
    }

    // 用户地址上的代码页只读。内核通过物理页的直接映射地址装入测试指令。
    auto* code = reinterpret_cast<uint8_t*>(direct_map + code_frame);
    for (size_t offset = 0; offset < size; ++offset) {
        code[offset] = program_begin[offset];
    }
    return true;
}

}
