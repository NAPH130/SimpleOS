#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mm {
class AddressSpace;
}

namespace loader {

// 预留 ELF64 装载接口。image 指向已有的文件内容，address_space 是目标进程地址空间。
// TODO: 校验文件头及程序头，映射可装载段，并通过 entry 返回用户态入口地址。
[[nodiscard]] bool load_elf(const void* image, size_t image_size,
                            mm::AddressSpace& address_space, uintptr_t& entry);

}
