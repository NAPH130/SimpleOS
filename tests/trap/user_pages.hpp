#pragma once

#include "test_boot.hpp"
#include <arch/x86_64/paging.hpp>
#include <mm/address_space.hpp>
#include <mm/memory.hpp>

namespace user_pages {

inline constexpr uintptr_t arena_begin = 0x400000;
inline constexpr uintptr_t arena_end = 0x800000;

struct ProgramPages {
    uintptr_t entry;
    uintptr_t stack_page;

    uintptr_t stack_top() const {
        return stack_page + x86_64::paging::page_size;
    }
};

// 由地址空间选择代码和栈的虚拟地址，填好后才把入口与栈的位置交给调用者。
inline bool prepare(mm::AddressSpace& space, const uint8_t* program_begin,
                    const uint8_t* program_end, ProgramPages& result) {
    namespace paging = x86_64::paging;
    const size_t size = reinterpret_cast<uintptr_t>(program_end)
        - reinterpret_cast<uintptr_t>(program_begin);
    if (size == 0 || size > paging::page_size) {
        return false;
    }

    ProgramPages pages;
    if (!space.allocate(size, paging::UserPageAccess::read_execute, pages.entry)) {
        return false;
    }
    uintptr_t code_frame;
    if (!space.physical_frame(pages.entry, code_frame)
        || !space.allocate(paging::page_size, paging::UserPageAccess::read_write, pages.stack_page)) {
        require_trap_test(space.release(pages.entry), "Cannot roll back user code page.");
        return false;
    }

    // 用户地址上的代码页只读。内核通过物理页的直接映射地址装入测试指令。
    auto* code = static_cast<uint8_t*>(mm::physical_to_virtual(code_frame));
    for (size_t offset = 0; offset < size; ++offset) {
        code[offset] = program_begin[offset];
    }
    result = pages;
    return true;
}

}
