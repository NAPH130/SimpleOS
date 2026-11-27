#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/paging.hpp>
#include <kernel/console.hpp>
#include <mm/memory.hpp>
#include <new>

namespace mm {
namespace {

using x86_64::paging::page_size;
constexpr size_t alignment = alignof(max_align_t);
constexpr size_t alignment_mask = alignment - 1;

struct alignas(max_align_t) Block {
    size_t size; // 块头之后的可用字节数。
    Block* next;
    bool used;
};

struct alignas(max_align_t) Arena {
    Arena* next;
    uintptr_t frame;
    size_t frame_count;

    Block* first() { return reinterpret_cast<Block*>(this + 1); }
};

Arena* arenas = nullptr;

Block* grow(size_t size) {
    constexpr size_t overhead = sizeof(Arena) + sizeof(Block);
    if (size > SIZE_MAX - overhead - (page_size - 1)) {
        return nullptr;
    }
    const size_t frames = (size + overhead + page_size - 1) / page_size;
    uintptr_t frame;
    if (!allocate_contiguous_frames(frames, frame)) {
        return nullptr;
    }
    // 直接映射保留物理连续性，整个内存段可以划分为多个可变大小块。
    auto* arena = ::new (physical_to_virtual(frame)) Arena{arenas, frame, frames};
    auto* first = ::new (arena->first()) Block{frames * page_size - overhead, nullptr, false};
    arenas = arena;
    return first;
}

void* claim(Block& block, size_t size) {
    if (block.size - size >= sizeof(Block) + alignment) {
        auto* remainder = reinterpret_cast<uint8_t*>(&block + 1) + size;
        block.next = ::new (remainder) Block{block.size - size - sizeof(Block), block.next, false};
        block.size = size;
    }
    block.used = true;
    return &block + 1;
}

void merge_next(Block& block) {
    if (block.next != nullptr && !block.next->used) {
        block.size += sizeof(Block) + block.next->size;
        block.next = block.next->next;
    }
}

void release_arena(Arena** link) {
    const auto* arena = *link;
    const uintptr_t frame = arena->frame;
    const size_t count = arena->frame_count;
    *link = arena->next;
    // 先取走全部元数据，再归还承载它的第一页。
    for (size_t index = 0; index < count; ++index) {
        if (!free_frame(frame + index * page_size)) {
            kprintf_log(LogLevel::ERROR, "Cannot release a kernel heap frame.\n");
            x86_64::halt();
        }
    }
}

}

void* kmalloc(size_t size) {
    if (size == 0 || size > SIZE_MAX - alignment_mask) {
        return nullptr;
    }
    size = (size + alignment_mask) & ~alignment_mask;
    for (auto* arena = arenas; arena != nullptr; arena = arena->next) {
        for (auto* block = arena->first(); block != nullptr; block = block->next) {
            if (!block->used && block->size >= size) {
                return claim(*block, size);
            }
        }
    }
    auto* block = grow(size);
    return block == nullptr ? nullptr : claim(*block, size);
}

void kfree(void* pointer) {
    if (pointer == nullptr) {
        return;
    }
    for (Arena** link = &arenas; *link != nullptr; link = &(*link)->next) {
        auto* first = (*link)->first();
        Block* previous = nullptr;
        for (Block* block = first; block != nullptr; block = block->next) {
            // 只比较地址，以免把传入的任意指针当成块头读取。
            if (block + 1 == pointer) {
                if (!block->used) {
                    return;
                }
                block->used = false;
                merge_next(*block);
                if (previous != nullptr && !previous->used) {
                    merge_next(*previous);
                }
                if (!first->used && first->next == nullptr) {
                    release_arena(link);
                }
                return;
            }
            previous = block;
        }
    }
}

}
