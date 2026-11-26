#include <arch/x86_64/paging.hpp>
#include <mm/memory.hpp>
#include <new>

namespace mm {
namespace {

using x86_64::paging::page_size;
constexpr uintptr_t page_mask = page_size - 1;
constexpr size_t bits_per_byte = 8;

struct FrameRegion {
    uintptr_t base;   // 此区间第一张可分配页帧的物理地址。
    size_t frame_count;
    uint8_t* bitmap;   // 内核虚拟指针。每页一位，0 空闲，1 已分配。
    // 比该下标更小的页帧均已占用。释放较低地址的页帧会把起点移回去。
    size_t first_candidate;
};

static_assert(sizeof(FrameRegion) + 1 < page_size);

FrameRegion* regions = nullptr;
size_t region_count = 0;

PhysicalRange whole_pages(PhysicalRange range) {
    // 先把区间起点向上对齐，再截掉末尾的不足一页部分，确保只接管完整的页帧。
    const size_t leading = (-range.base) & page_mask;
    if (range.size <= leading) {
        return {};
    }
    return {range.base + leading, (range.size - leading) & ~page_mask};
}

size_t bitmap_size(size_t frame_count) {
    return (frame_count + bits_per_byte - 1) / bits_per_byte;
}

uint8_t frame_mask(size_t index) {
    return static_cast<uint8_t>(1u << (index % bits_per_byte));
}

}

bool initialize(size_t range_count, PhysicalRangeReader read_range, uintptr_t direct_map_offset) {
    if (regions != nullptr || read_range == nullptr || (direct_map_offset & page_mask) != 0) {
        return false;
    }

    size_t usable_count = 0;
    size_t bitmap_bytes = 0;
    uintptr_t previous_end = 0;
    PhysicalRange metadata_range{};
    for (size_t index = 0; index < range_count; ++index) {
        const PhysicalRange original = read_range(index);
        if (original.size > UINTPTR_MAX - original.base) {
            return false;
        }
        const PhysicalRange range = whole_pages(original);
        if (range.size == 0) {
            continue;
        }
        if (range.base < previous_end) {
            return false;
        }

        previous_end = range.base + range.size;
        ++usable_count;
        bitmap_bytes += bitmap_size(range.size / page_size);
        if (range.size > metadata_range.size) {
            metadata_range = range;
        }
    }
    if (usable_count == 0) {
        return false;
    }

    // 区间互不重叠，元数据最多为每个物理页一份描述符加一个位图字节。
    // 预留尺寸按原区间估算。扣掉元数据页后，用剩余页帧数建立各自的位图。
    const size_t descriptor_bytes = usable_count * sizeof(FrameRegion);
    const size_t metadata_bytes = (descriptor_bytes + bitmap_bytes + page_mask) & ~page_mask;
    if (metadata_bytes > metadata_range.size || metadata_range.base > UINTPTR_MAX - direct_map_offset) {
        return false;
    }
    const uintptr_t metadata_virtual = direct_map_offset + metadata_range.base;
    if (metadata_virtual == 0 || metadata_bytes - 1 > UINTPTR_MAX - metadata_virtual) {
        return false;
    }

    // 先从最大区间划出整页用于元数据，任何描述符都不会覆盖这些物理页。
    auto* descriptors = ::new (reinterpret_cast<void*>(metadata_virtual)) FrameRegion[usable_count];
    auto* bitmap = ::new (reinterpret_cast<void*>(metadata_virtual + descriptor_bytes))
        uint8_t[bitmap_bytes];
    size_t managed_count = 0;
    for (size_t index = 0; index < range_count; ++index) {
        PhysicalRange range = whole_pages(read_range(index));
        if (range.size == 0) {
            continue;
        }
        if (range.base == metadata_range.base) {
            range.base += metadata_bytes;
            range.size -= metadata_bytes;
        }
        if (range.size == 0) {
            continue;
        }

        const size_t frames = range.size / page_size;
        const size_t bytes = bitmap_size(frames);
        for (size_t byte = 0; byte < bytes; ++byte) {
            bitmap[byte] = 0;
        }
        descriptors[managed_count++] = {range.base, frames, bitmap, 0};
        bitmap += bytes;
    }

    regions = descriptors;
    region_count = managed_count;
    return true;
}

bool allocate_frame(uintptr_t& physical_address) {
    // 区间按起始地址排序，首个空位对应当前物理地址最低的空闲页。
    for (size_t region_index = 0; region_index < region_count; ++region_index) {
        auto& region = regions[region_index];
        for (size_t index = region.first_candidate; index < region.frame_count; ++index) {
            uint8_t& byte = region.bitmap[index / bits_per_byte];
            const uint8_t mask = frame_mask(index);
            if ((byte & mask) == 0) {
                byte |= mask;
                region.first_candidate = index + 1;
                physical_address = region.base + index * page_size;
                return true;
            }
        }
        region.first_candidate = region.frame_count;
    }
    return false;
}

bool free_frame(uintptr_t physical_address) {
    // 位图以页帧起点为索引。页内任意字节地址都不能用来释放页帧。
    if ((physical_address & page_mask) != 0) {
        return false;
    }
    for (size_t region_index = 0; region_index < region_count; ++region_index) {
        auto& region = regions[region_index];
        if (physical_address < region.base) {
            return false;
        }
        const size_t index = (physical_address - region.base) / page_size;
        if (index >= region.frame_count) {
            continue;
        }

        uint8_t& byte = region.bitmap[index / bits_per_byte];
        const uint8_t mask = frame_mask(index);
        if ((byte & mask) == 0) {
            return false;
        }
        byte &= static_cast<uint8_t>(~mask);
        if (index < region.first_candidate) {
            region.first_candidate = index;
        }
        return true;
    }
    return false;
}

}
