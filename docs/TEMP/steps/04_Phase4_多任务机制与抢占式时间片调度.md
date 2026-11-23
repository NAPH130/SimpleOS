# 04 - Phase 4：多任务机制与抢占式时间片调度

---

## 1. 详细的具体工程开发实施计划

### 1.1 阶段目标与交付物
实现多任务操作系统核心：进程控制块（PCB）生命周期管理、64 位无开销上下文切换（`switch_to`）、可编程定时器（PIT 8254）周期性时钟中断，以及**时间片轮转（Round-Robin）与多级反馈队列（MLFQ，进阶加分项）** 抢占调度器。
- **交付源文件**：
  - `task/process.cpp` & `process.hpp`：进程控制块（PCB）与状态流转机
  - `arch/x86_64/switch.S`：System V AMD64 ABI 寄存器级上下文切换汇编核心
  - `drivers/pit.cpp` & `pit.hpp`：Intel 8254 可编程间隔定时器（PIT）驱动
  - `task/scheduler.cpp` & `scheduler.hpp`：RR 轮转与 MLFQ 调度引擎
  - `apps/multitask_demo.cpp`：多任务交替并发运行测试程序

### 1.2 详细开发排期与子任务
1. **子任务 4.1：构建进程控制块 PCB (`task/process.cpp`)**
   - 字段设计：PID、任务状态（`Ready`, `Running`, `Blocked`, `Zombie`）、专有内核栈指针（`kernel_stack`）、上下文指针（`TaskContext*`）、私有根页表（`cr3`）、时间片（`time_ticks`）、优先级（`priority`）；
   - 实现进程工厂函数：`Process::create_kernel_thread(void (*entry)())` 与 `Process::create_user_process(...)`。
2. **子任务 4.2：实现底层寄存器上下文切换 (`arch/x86_64/switch.S`)**
   - 编写 `switch_to(TaskContext* prev, TaskContext* next)`；
   - 将当前 CPU 的 `rbx, rbp, r12, r13, r14, r15, rsp` 压入当前栈，并将更新后的栈顶指针写入 `prev`；
   - 将 `next` 的栈顶指针加载入 `rsp`，依次弹出寄存器并执行 `ret`，瞬时跳转入目标任务的执行上下文。
3. **子任务 4.3：配置 PIT 8254 时钟中断源 (`drivers/pit.cpp`)**
   - 通过 I/O 端口 `0x43` 写入控制字 `0x36`（方波发生器模式）；
   - 通过端口 `0x40` 写入分频计数值（基频 1193182Hz / 100Hz = 11931），设定时钟中断周期为 10ms（100Hz）；
   - 在 IDT 的 32 号门（IRQ 0）注册时钟中断处理函数，每次触发时发送 8259A EOI（中断结束）信号。
4. **子任务 4.4：实现抢占式调度器引擎 (`task/scheduler.cpp`)**
   - **基础算法：时间片轮转 (RR)**：维护就绪链表，时钟中断周期递减当前任务时间片；时间片归零时，将当前任务移至就绪队尾，调用 `schedule()` 切换至队首任务；
   - **进阶加分：多级反馈队列 (MLFQ)**：
     - 构建 Q0（高优/时间片 5ms）、Q1（中优/10ms）、Q2（低优/20ms）三级就绪队列；
     - 新进程进入 Q0，时间片用尽未主动放弃 CPU 则降级至次级队列；高优先级队列有就绪任务时始终抢占低优先级。
5. **子任务 4.5：协作式调度系统调用 (`sys_yield`)**
   - 允许任务在主动等待或空闲时主动让渡 CPU，重置自身时间片并触发调度器重新选路。

---

## 2. 我（用户）需要干什么

### 2.1 编译与执行命令
在工程根目录下执行构建并运行：

```bash
make run
```

### 2.2 验证结果与观察点
- 观察控制台输出：
  ```
  [SimpleOS] PIT 8254 Timer Configured: 100Hz (10ms tick interval).
  [SimpleOS] Scheduler Initialized: MLFQ Engine (3 Priority Queues).
  [SimpleOS] Spawning Task 1 ('A'), Task 2 ('B'), Task 3 ('C')...
  [SimpleOS] Entering Preemptive Scheduling Loop:
  [Task A] Tick: 1 ...
  [Task B] Tick: 1 ...
  [Task C] Tick: 1 ...
  [Task A] Tick: 2 ...
  [Task B] Tick: 2 ...
  [Task C] Tick: 2 ...
  ```
- **核心判定**：三个任务并没有按顺序将自己完全执行完，而是在没有手动调用任何 yield 的情况下，被 PIT 时钟中断规律打断，交替输出字符，证明**基于硬件定时器的抢占式分时调度**彻底成功运行。

---

## 3. 实现原理与方式

### 3.1 跨任务切换（`switch_to`）为什么不需要保存所有寄存器？
- **调用者保存（Caller-Saved）与被调用者保存（Callee-Saved）机制**：
  - 在 System V AMD64 ABI 约定中，`rax, rcx, rdx, rsi, rdi, r8~r11` 属于调用者保存寄存器，当 C++ 代码调用 `switch_to` 函数前，编译器已经自动在调用前把有用的临时变量保护到栈中了。
  - 因此，`switch_to` 汇编函数只需显式保存被调用者保存寄存器（`rbx, rbp, r12, r13, r14, r15`）以及关键的栈顶指针 `rsp`。
  - 当通过 `mov rsp, [next]` 切换栈指针后，一条 `ret` 指令便会自动从新栈中弹出先前保存的返回地址 `rip`，从而无缝跳入新任务继续执行，其切换开销仅需约 10 个时钟周期。

### 3.2 抢占调度发生的完整物理链路
1. PIT 8254 硬件定时器计数器倒计时归零，拉高 CPU 的 INTR 引脚；
2. CPU 硬件完成当前机器指令后，暂停取指，自动保存当前指令位置，跳转至 IDT 32 号中断门；
3. 汇编存根保存通用寄存器，调用 C++ 中断分发器 `timer_interrupt_handler()`；
4. 调度器递减当前进程 `current->time_slice`；
5. 若时间片消耗完毕，标记需要调度，在中断退出前夕调用 `schedule()` 执行 `switch_to`，将 CPU 控制权切换给下一个就绪进程。

---

## 4. 实现工具与依赖

| 依赖项 | 硬件 / 软件规范 |
| :--- | :--- |
| **Intel 8254 PIT** | 主板标准定时芯片，操作端口 `0x40`（通道 0 数据）与 `0x43`（模式/命令寄存器） |
| **System V AMD64 ABI** | 寄存器使用规范，规定了 64 位下函数传参（`rdi, rsi, rdx...`）与保留寄存器约定 |
| **双向循环就绪链表** | 自研轻量侵入式链表，保证进程入队出队操作为严格的 $O(1)$ 复杂度 |
