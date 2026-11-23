#pragma once

#include <stddef.h>

namespace fs {

// TODO: 先接入只读内存文件，再添加 FAT32 后端。
[[nodiscard]] bool file_size(const char* path, size_t& size);
[[nodiscard]] bool read_file(const char* path, size_t offset, void* buffer,
                             size_t capacity, size_t& bytes_read);

}
