#pragma once

namespace task {

// TODO: 定义进程控制块，记录进程 ID、状态、地址空间和执行现场。
struct Process;

// TODO: 初始化就绪队列，将新建或唤醒的进程放入队列。
// process 指向任务模块持有的进程控制块，排队期间需保持有效。
void initialize_scheduler();
void enqueue_ready(Process& process);

// yield 表示任务主动让出 CPU，on_timer_tick 由时钟中断调用。
// TODO: 接入底层上下文切换，支持协作式让权和时钟抢占。
void yield();
void on_timer_tick();

// TODO: 保存退出码并调度下一个任务，后续回收当前进程资源。
[[noreturn]] void exit_current(int status);

}
