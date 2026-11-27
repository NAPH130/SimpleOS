#pragma once

#include <arch/x86_64/paging.hpp>

namespace mm {

// 管理按页对齐的用户虚拟区间 [begin, end)。调用方串行修改映射，并在回收前结束用户访问。
class AddressSpace {
public:
    // 建立进程自有的页表。页表分配失败时返回无效对象，映射操作会返回 false。
    [[nodiscard]] static AddressSpace create_user(uintptr_t begin, uintptr_t end);
    // 诊断代码可借用当前页表，析构时只回收通过这个对象创建的映射。
    [[nodiscard]] static AddressSpace borrow_current_for_diagnostics(uintptr_t begin, uintptr_t end);
    ~AddressSpace();

    AddressSpace(const AddressSpace&) = delete;
    AddressSpace& operator=(const AddressSpace&) = delete;

    uintptr_t page_table_root() const { return root_; }
    bool owns_page_table() const { return mode_ == Mode::owned; }
    // 回收本对象建立的映射和自有页表，成功后对象失效。仍在使用自有页表时返回 false。
    [[nodiscard]] bool destroy();

    // bytes 向上取整到整页。成功时返回连续虚拟区间的起点，失败时保留输出值。
    [[nodiscard]] bool allocate(size_t bytes, x86_64::paging::UserPageAccess access,
                                uintptr_t& virtual_address);
    // 按 allocate 返回的起点释放整个区间。范围内的其他地址会被拒绝。
    [[nodiscard]] bool release(uintptr_t virtual_address);

    // 系统调用分配可读写页，且只可释放通过该接口建立的区域。
    [[nodiscard]] bool allocate_user(size_t bytes, uintptr_t& virtual_address);
    [[nodiscard]] bool release_user(uintptr_t virtual_address);
    [[nodiscard]] bool physical_frame(uintptr_t virtual_address, uintptr_t& frame) const;

private:
    enum class Mode { invalid, borrowed, owned };
    struct Region;

    AddressSpace(uintptr_t begin, uintptr_t end, uintptr_t root, Mode mode);

    bool allocate_region(size_t bytes, x86_64::paging::UserPageAccess access,
                         bool user_allocation, uintptr_t& virtual_address);
    bool release_region(uintptr_t virtual_address, bool user_only);
    void unmap_range(uintptr_t begin, uintptr_t end);

    uintptr_t begin_;
    uintptr_t end_;
    uintptr_t root_;
    Mode mode_;
    Region* regions_ = nullptr;
};

}
