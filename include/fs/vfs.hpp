#pragma once

#include <stddef.h>

namespace fs {

// 预留文件读取接口，先接入只读内存文件，再添加 FAT32 后端。
// file_size 输出文件总字节数。read_file 从字节偏移 offset 起读入调用方的缓冲区。
// capacity 是缓冲区容量，bytes_read 用于接收实际读取的字节数。
[[nodiscard]] bool file_size(const char* path, size_t& size);
[[nodiscard]] bool read_file(const char* path, size_t offset, void* buffer,
                             size_t capacity, size_t& bytes_read);

}
