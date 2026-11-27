#include <arch/x86_64/cpu.hpp>
#include <kernel/console.hpp>
#include <mm/address_space.hpp>
#include <mm/memory.hpp>
#include <new>

namespace mm {
namespace {

namespace paging = x86_64::paging;
constexpr uintptr_t page_mask = paging::page_size - 1;

[[noreturn]] void ownership_error() {
    kprintf_log(LogLevel::ERROR, "Address space ownership is inconsistent.\n");
    x86_64::halt();
}

}

struct AddressSpace::Region {
    uintptr_t base;
    size_t size; // 对齐后的字节数。
    Region* next;
    bool user_allocation;
};

AddressSpace::AddressSpace(uintptr_t begin, uintptr_t end, uintptr_t root, Mode mode)
    : begin_(begin), end_(end), root_(root), mode_(mode) {}

AddressSpace::~AddressSpace() {
    if (!destroy()) {
        ownership_error();
    }
}

AddressSpace AddressSpace::create_user(uintptr_t begin, uintptr_t end) {
    uintptr_t root = 0;
    const auto mode = paging::create_user_page_table(root) ? Mode::owned : Mode::invalid;
    return AddressSpace(begin, end, root, mode);
}

AddressSpace AddressSpace::borrow_current_for_diagnostics(uintptr_t begin, uintptr_t end) {
    return AddressSpace(begin, end, paging::current_page_table_root(), Mode::borrowed);
}

bool AddressSpace::destroy() {
    if (owns_page_table() && root_ == paging::current_page_table_root()) {
        return false;
    }
    while (regions_ != nullptr) {
        if (!release(regions_->base)) {
            ownership_error();
        }
    }
    if (owns_page_table() && !paging::destroy_user_page_table(root_)) {
        ownership_error();
    }
    mode_ = Mode::invalid;
    root_ = 0;
    return true;
}

bool AddressSpace::allocate(size_t bytes, paging::UserPageAccess access, uintptr_t& virtual_address) {
    return allocate_region(bytes, access, false, virtual_address);
}

bool AddressSpace::allocate_user(size_t bytes, uintptr_t& virtual_address) {
    return allocate_region(bytes, paging::UserPageAccess::read_write, true, virtual_address);
}

bool AddressSpace::allocate_region(size_t bytes, paging::UserPageAccess access,
                                   bool user_allocation, uintptr_t& virtual_address) {
    if (mode_ == Mode::invalid || bytes == 0 || bytes > SIZE_MAX - page_mask) {
        return false;
    }
    const size_t size = (bytes + page_mask) & ~page_mask;

    // 已分配区间按虚拟地址排序。只检查区间之间的空隙，页表层会跳过其中的外部映射。
    uintptr_t begin = begin_;
    uintptr_t address;
    Region** slot = &regions_;
    while (*slot != nullptr && !paging::find_user_range(root_, begin, (*slot)->base, size, address)) {
        begin = (*slot)->base + (*slot)->size;
        slot = &(*slot)->next;
    }
    if (*slot == nullptr && !paging::find_user_range(root_, begin, end_, size, address)) {
        return false;
    }

    auto* storage = kmalloc(sizeof(Region));
    if (storage == nullptr) {
        return false;
    }

    // 等整段映射都成功才公布描述符。失败时撤销此前已经建立的页并归还元数据。
    for (uintptr_t page = address; page < address + size; page += paging::page_size) {
        uintptr_t frame;
        if (!paging::map_user_page(root_, page, access, frame)) {
            unmap_range(address, page);
            kfree(storage);
            return false;
        }
    }

    *slot = ::new (storage) Region{address, size, *slot, user_allocation};
    virtual_address = address;
    return true;
}

bool AddressSpace::release(uintptr_t virtual_address) {
    return release_region(virtual_address, false);
}

bool AddressSpace::release_user(uintptr_t virtual_address) {
    return release_region(virtual_address, true);
}

bool AddressSpace::release_region(uintptr_t virtual_address, bool user_only) {
    Region** slot = &regions_;
    while (*slot != nullptr && (*slot)->base < virtual_address) {
        slot = &(*slot)->next;
    }
    if (*slot == nullptr || (*slot)->base != virtual_address
        || (user_only && !(*slot)->user_allocation)) {
        return false;
    }

    Region* record = *slot;
    unmap_range(record->base, record->base + record->size);
    *slot = record->next;
    kfree(record);
    return true;
}

bool AddressSpace::physical_frame(uintptr_t virtual_address, uintptr_t& frame) const {
    return mode_ != Mode::invalid && paging::user_page_frame(root_, virtual_address, frame);
}

void AddressSpace::unmap_range(uintptr_t begin, uintptr_t end) {
    while (end > begin) {
        end -= paging::page_size;
        if (!paging::unmap_user_page(root_, end)) {
            ownership_error();
        }
    }
}

}
