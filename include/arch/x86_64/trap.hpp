#pragma once

namespace x86_64 {

// TODO: 与中断入口汇编一起定义寄存器现场及异常错误码的布局。
struct TrapFrame;

// TODO: 安装 GDT、TSS、IDT，并建立用户态可访问的系统调用入口。
void initialize_traps();

// TODO: 按向量分发 CPU 异常、外设中断和系统调用。
extern "C" void trap_dispatch(TrapFrame* frame);

}
