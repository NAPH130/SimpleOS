#include <boot/limine.hpp>
#include <mm/memory.hpp>
#include <stdint.h>

namespace {

// 链接脚本把请求及两端的标记放在同一段。Limine 在转入 kernel_main 前填好响应指针。
// 引导器将版本字段置零来确认支持该版本，volatile 保留对此字段的读取。
__attribute__((used, section(".limine_requests_start")))
volatile uint64_t requests_start[] = {
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf,
    0x785c6ed015d3e316, 0x181e920a7852b9d9
};

__attribute__((used, section(".limine_requests")))
volatile uint64_t base_revision[] = {
    0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 3
};

struct HhdmResponse {
    uint64_t revision;
    uint64_t offset;
};

struct HhdmRequest {
    uint64_t id[4];
    uint64_t revision;
    const HhdmResponse* response;
};

// 请求直接映射的偏移，供内核通过物理地址读取页表。
[[gnu::used, gnu::section(".limine_requests")]]
volatile HhdmRequest hhdm_request{
    {0xc7b1dd30df4c8b88, 0x0a82e883a194f07b, 0x48dcf1cb8ad2b852, 0x63984e959a98244b},
    0,
    nullptr,
};

struct MemoryMapEntry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

// entries 指向指针数组。内存区间可能不连续，遍历时必须逐项取指针。
struct MemoryMapResponse {
    uint64_t revision;
    uint64_t entry_count;
    const MemoryMapEntry* const* entries;
};

struct MemoryMapRequest {
    uint64_t id[4];
    uint64_t revision;
    const MemoryMapResponse* response;
};

[[gnu::used, gnu::section(".limine_requests")]]
volatile MemoryMapRequest memory_map_request{
    {0xc7b1dd30df4c8b88, 0x0a82e883a194f07b, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62},
    0,
    nullptr,
};

mm::PhysicalRange usable_range(size_t index) {
    constexpr uint64_t usable_memory = 0;
    const auto* entry = memory_map_request.response->entries[index];
    // 引导器、内核镜像和设备等占用的区间在此阶段全部保留。
    if (entry == nullptr || entry->type != usable_memory) {
        return {};
    }
    return {entry->base, entry->length};
}

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t requests_end[] = {
    0xadc0e0531bb10d03, 0x9572709f31764c62
};

}

bool boot::limine_base_revision_supported() {
    return base_revision[2] == 0;
}

bool boot::higher_half_direct_map_offset(uintptr_t& offset) {
    const auto* response = hhdm_request.response;
    if (response == nullptr) {
        return false;
    }
    offset = response->offset;
    return true;
}

bool boot::initialize_physical_memory() {
    const auto* response = memory_map_request.response;
    uintptr_t direct_map_offset;
    if (response == nullptr || response->entries == nullptr
        || !higher_half_direct_map_offset(direct_map_offset)) {
        return false;
    }
    // 通过按索引读取的回调传递可用区间，内核堆建立前也能扫描两遍。
    return mm::initialize(response->entry_count, usable_range, direct_map_offset);
}
