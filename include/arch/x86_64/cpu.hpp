#pragma once

namespace x86_64 {

// 关闭可屏蔽中断，并在 CPU 因其他事件唤醒后继续停机。调用者先输出需要的诊断。
[[noreturn]] inline void halt() {
    asm volatile("cli" ::: "memory");
    for (;;) {
        asm volatile("hlt" ::: "memory");
    }
}

}
