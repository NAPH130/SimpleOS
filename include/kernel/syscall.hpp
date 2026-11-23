#pragma once

#include <arch/x86_64/trap.hpp>

namespace kernel::syscall {

// TODO: 从用户态陷阱现场读取调用号和参数，完成 sys_write、sys_exit、
// sys_yield 的分发，校验用户缓冲区并把返回值写回现场。
void dispatch(x86_64::TrapFrame& frame);

}
