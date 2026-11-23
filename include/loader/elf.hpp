#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mm {
class AddressSpace;
}

namespace loader {

// TODO: 校验 ELF64 头及程序头，映射可装载段，并返回用户程序入口。
[[nodiscard]] bool load_elf(const void* image, size_t image_size,
                            mm::AddressSpace& address_space, uintptr_t& entry);

}
