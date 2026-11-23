#include <arch/x86_64/cpu.hpp>
#include <arch/x86_64/trap.hpp>
#include <arch/x86_64/vectors.h>
#include <kernel/console.hpp>
#include <stddef.h>

namespace x86_64 {

static_assert(offsetof(TrapFrame, vector) == 120);
static_assert(offsetof(TrapFrame, error_code) == 128);
static_assert(offsetof(TrapFrame, rip) == 136);
static_assert(sizeof(TrapFrame) == 176);

extern "C" void trap_dispatch(TrapFrame* frame) {
    if (frame->vector == X86_64_VECTOR_BREAKPOINT) {
        kprintf_log(LogLevel::DEBUG, "breakpoint handled\n");
        return;
    }

    // TODO: 将其他异常、设备中断和系统调用交给对应模块。
    kprintf_log(LogLevel::ERROR, "Unhandled trap vector %u\n",
                static_cast<unsigned>(frame->vector));
    halt();
}

}
