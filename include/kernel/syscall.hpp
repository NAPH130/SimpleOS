#pragma once

#include <arch/x86_64/trap.hpp>

namespace kernel::syscall {

// 从用户态现场读取请求和参数，成功返回时把结果写回 RAX。
// TODO: 调度器就绪后接入 sys_yield，并将 sys_exit 交给任务退出路径。
void dispatch(x86_64::TrapFrame& frame);

}
