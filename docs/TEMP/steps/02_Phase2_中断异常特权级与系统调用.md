# 02 - Phase 2：中断异常、特权级与系统调用

---

## 1. 详细的具体工程开发实施计划

### 1.1 阶段目标与交付物
建立完善的 x86_64 中断与异常处理机制（IDT），配置 GDT 与任务状态段（TSS），实现用户态（Ring 3）与内核态（Ring 0）特权级安全隔离，并通过 `int 0x80` 实现首批关键系统调用（`sys_write` 与 `sys_exit`），最后运行一个独立编译的用户态应用。
- **交付源文件**：
  - `arch/x86_64/gdt.cpp` & `gdt.hpp`：64 位全局描述符表与 TSS（Task State Segment）配置
  - `arch/x86_64/idt_entry.S`：256 个中断门通用汇编现场保存与恢复存根
  - `arch/x86_64/idt.cpp` & `idt.hpp`：中断描述符表初始化与 C++ 分发器
  - `arch/x86_64/trap.hpp`：`TrapContext` 寄存器上下文结构体
  - `kernel/syscall.cpp` & `syscall.hpp`：系统调用分发引擎（Syscall Dispatcher）
  - `user/libuser.cpp` & `libuser.hpp`：用户态系统调用封装库
  - `user/hello.cpp`：第一个独立运行的用户程序

### 1.2 详细开发排期与子任务
1. **子任务 2.1：构建 64 位 GDT 与 TSS 任务状态段 (`arch/x86_64/gdt.cpp`)**
   - 定义内核代码段（`0x08`）、内核数据段（`0x10`）、用户代码段（`0x1B`，RPL=3）、用户数据段（`0x23`，RPL=3）；
   - 在 TSS 中设置 `RSP0`（特权级由 Ring 3 切换到 Ring 0 时的内核栈顶地址）；
   - 加载 GDT（`lgdt`）并加载任务寄存器（`ltr`）。
2. **子任务 2.2：构建 256 门 IDT 中断描述符表 (`arch/x86_64/idt.cpp` & `idt_entry.S`)**
   - 编写汇编宏批量生成 0~255 号中断的入口；
   - 区分 CPU 硬件自带错误码的异常（如 8, 10, 11, 12, 13, 14 号）与无错误码的异常，统一压栈平衡；
   - 压入 15 个通用寄存器（`rax, rbx, rcx, rdx, rsi, rdi, rbp, r8~r15`），构成 `TrapContext`；
   - 调用 C++ 统一处理函数 `trap_dispatch(TrapContext* ctx)`；
   - 执行 `iretq` 恢复寄存器并返回。
3. **子任务 2.3：实现系统调用分发机制 (`kernel/syscall.cpp`)**
   - 注册 `0x80` 号中断门，设置描述符特权级 `DPL=3`（允许用户态直接调用）；
   - 定义基础系统调用号：
     - `SYS_WRITE (1)`：将用户缓冲区数据输出至控制台；
     - `SYS_EXIT (2)`：终止当前应用程序；
     - `SYS_YIELD (3)`：主动让渡 CPU 使用权；
   - 从 `TrapContext` 中提取 `rax`（系统调用号），`rdi, rsi, rdx`（参数），执行分发，返回值写入 `TrapContext.rax`。
4. **子任务 2.4：首次降权切换至 Ring 3 并执行用户程序**
   - 编写汇编切换代码，在内核栈上伪造中断返回现场（`SS=0x23, RSP=user_stack, RFLAGS=0x202, CS=0x1B, RIP=user_entry`）；
   - 执行 `iretq` 指令，硬件原子性完成特权级降权切换并跳转至用户代码执行。

---

## 2. 我（用户）需要干什么

### 2.1 编译与执行命令
在工程根目录下执行构建并运行：

```bash
make run
```

### 2.2 验证结果与观察点
- 观察控制台输出日志：
  ```
  [SimpleOS] Initializing GDT and TSS (RSP0 configured)... [OK]
  [SimpleOS] Initializing IDT (256 gates registered)... [OK]
  [SimpleOS] Registering Syscall Handler at INT 0x80 (DPL=3)... [OK]
  [SimpleOS] Dropping privilege to Ring 3 (User Space)...
  [UserApp] Hello, World! This is running in User Mode (Ring 3)!
  [UserApp] Invoking sys_write via INT 0x80 successfully.
  [UserApp] Exiting with code 0...
  [SimpleOS] Trap: Process exited cleanly via sys_exit. Returned to Kernel.
  ```
- **核心判定**：若用户态程序在未经允许访问特权寄存器或指令（如 `cli`）时，系统能捕获 General Protection Fault (13号异常)，则证明特权隔离机制 100% 生效。

---

## 3. 实现原理与方式

### 3.1 为什么必须配置 TSS 的 `RSP0`？
- **硬件特权切换机制**：当 CPU 在 Ring 3（用户态）运行并发生中断、异常或执行 `int 0x80` 时，CPU 硬件不允许继续使用不可信的用户栈（可能溢出或被恶意破坏）。
- **硬件自动查表操作**：CPU 内部硬件会自动查询当前加载的 TSS（Task State Segment）中的 `RSP0` 字段，将 `RSP` 指针无条件切换到 `RSP0` 指向的受信内核栈顶，并在该内核栈上自动压入 5 个关键寄存器：`SS, RSP, RFLAGS, CS, RIP`。
- **故障后果**：如果未正确配置 TSS 并执行 `ltr`，一旦触发特权级切换，CPU 将因找不到合法内核栈而产生 Double Fault (8号异常)，最终导致系统 Triple Fault 崩溃重启。

### 3.2 `iretq` 指令的原子特权跃迁原理
- `iretq`（Interrupt Return 64-bit）是 x86_64 硬件中最核心的降权原语。
- 当执行 `iretq` 时，CPU 依次从当前内核栈中弹出 `RIP`、`CS`、`RFLAGS`、`RSP`、`SS`。
- **特权级转变**：CPU 检查弹出的 `CS` 寄存器低 2 位（CPL，当前特权级）。若 `CS` 为用户代码段选择子（低 2 位为 3），CPU 硬件立即将当前运行级别切换至 Ring 3，并加载用户栈指针 `RSP` 与指令指针 `RIP`，实现原子性的安全特权级降权。

---

## 4. 实现工具与依赖

| 依赖项 | 作用与关键实现细节 |
| :--- | :--- |
| **`asm volatile`** | C++ 内联汇编，用于封装用户态系统调用 `int 0x80` 以及读取/写入段寄存器 |
| **`arch/x86_64/idt_entry.S`** | NASM 宏文件，使用 `%rep 256` 快速生成无冗余的中断跳转存根表 |
| **TSS 结构体规范** | 按照 Intel 64 and IA-32 Architectures Software Developer's Manual Vol 3A 严格排布 104 字节结构体 |
