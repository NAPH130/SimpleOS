#pragma once

namespace x86_64 {

[[noreturn]] inline void halt() {
    asm volatile("cli" ::: "memory");
    for (;;) {
        asm volatile("hlt" ::: "memory");
    }
}

}
