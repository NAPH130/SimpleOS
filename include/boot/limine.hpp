#pragma once

namespace boot {

// 确认引导器已接受内核声明的 Limine 基础协议版本。
bool limine_base_revision_supported();
// 将 Limine 的可用区间和直接映射偏移交给内存模块。
[[nodiscard]] bool initialize_physical_memory();

}
