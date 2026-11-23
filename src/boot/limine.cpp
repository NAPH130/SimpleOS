#include <boot/limine.hpp>
#include <stdint.h>

namespace {

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

__attribute__((used, section(".limine_requests_end")))
volatile uint64_t requests_end[] = {
    0xadc0e0531bb10d03, 0x9572709f31764c62
};

}

bool boot::limine_base_revision_supported() {
    return base_revision[2] == 0;
}
