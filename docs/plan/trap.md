# Trap 开发计划

Trap 模块负责把 CPU 异常、已配置设备的硬件中断，以及项目选定的 `int 0x80` 系统调用接入内核。它管理 IDT、保存和恢复 `TrapFrame`，并把事件交给相应的处理函数。内存管理决定缺页能否恢复，设备驱动和中断控制器处理 IRQ，系统调用分发器和调度器分别处理服务请求和任务切换。

## 当前基线

- [x] 初始化 IDT 并注册第 3 号断点向量 `#BP`。
- [x] 从汇编保存的 `TrapFrame` 读取向量号，在 C++ 中记录日志，再用 `iretq` 恢复执行。
- [x] `make test` 在 QEMU 中验证内核态 `int3` 的触发、处理和返回。启动自检期间保持可屏蔽中断关闭。

现有入口、栈布局和日志见 [Trap 功能介绍](../architecture/trap.md)。

## 1. CPU 异常诊断

- [x] **实现无效指令异常 `#UD`。** 已注册第 6 号向量并复用寄存器保存流程。`make test-ud` 用独立镜像执行 `ud2`，核对异常名称、向量号、指令地址与 `RIP`，并验证诊断停机路径。
- [x] **统一有错误码和无错误码的入口。** 无硬件错误码时由汇编补 `0`，有硬件错误码时保留 CPU 压入的值。两种入口都传递相同布局的 `TrapFrame`。`make test-gp` 已通过实际的段保护故障核对第 13 号向量、非零硬件错误码和 `RIP`。
- [x] **实现缺页异常 `#PF` 诊断。** 复用带硬件错误码的入口，并在打印前读取 `CR2`。`make test-pf` 核对未映射的地址 `0x1000`、错误码 `0` 和触发指令的 `RIP`。 [Intel 系统编程手册，第 4、6 章](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
- [x] **补齐目标环境的 CPU 异常入口。** `0–31` 的向量由同一张表生成入口及诊断名称。`int3` 返回，其他向量由专用处理函数或通用诊断处理。`make test-de` 验证无硬件错误码的除零，`make test-ss` 验证栈段异常及硬件错误码。

## 2. 内核栈与特权级入口

- [x] **为双重故障 `#DF` 配置应急栈。** 加载包含内核段与 TSS 的最小 GDT，并通过 IST 为第 8 号向量切换独立栈。`make test-df` 验证双重故障日志中的 `IST=yes`，普通启动继续回归 `int3`。 [Intel 系统编程手册，第 6 章](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
- [x] **为 Ring 3 扩展 GDT 和 TSS。** 已加入用户代码段和数据段，`RSP0` 指向独立内核入口栈。`make -j8 test` 回归内核异常路径，在 QEMU 中用 GDB 核对加载的 GDT、任务寄存器、用户段描述符和 `RSP0`。
- [x] **验证 Ring 3 的首次进入和换栈。** `make test-ring3` 在独立镜像中映射用户代码和栈，通过 `enter_user_mode()` 从 Ring 0 进入 Ring 3。测试代码压栈后触发 `#UD`，脚本核对 `CS=0x2b`、`SS=0x33`、用户栈指针、故障地址和 `on RSP0 stack=yes`。在 QEMU 中用 GDB 读取保存的 `RSP` 指向的内存，确认栈顶值为 `0x2a`。`make -j8 test` 包含此自检。 [Ring 3 自检源码](../../tests/trap/ring3_test.cpp)
- [x] **接通 Ring 3 的 `int 0x80`。** `make test-syscall` 验证 `sys_write` 的串口输出、返回值与无效参数处理。测试程序继续在 Ring 3 运行，最后用 `sys_exit(42)` 把控制权和状态交回内核启动点。该自检已加入 `make test`。 [调用约定](../../include/abi/syscall.hpp) · [系统调用自检](../../tests/trap/syscall_user.cpp)
- [x] **接通正常退出和用户缺页的回收。** `sys_exit` 和已启动任务的用户态 `#PF` 共用退出路径。`make test-syscall` 核对两种退出的状态、内核页表的恢复及页帧的复用。 [进程启动与退出](../../src/task/process.cpp) · [回收自检](../../tests/trap/syscall_test.cpp)
- [ ] **扩展用户态致命异常的退出处理。** 将 `#UD` 等其他用户态致命异常接入任务退出。验收时让用户程序触发 `#UD`，确认程序退出后内核可以继续运行。
- [ ] **接入调度恢复与 `sys_yield`。** 调度器就绪后，验证任务可以主动让权并在稍后继续执行，退出后可以切换到下一个可运行任务。

## 3. 硬件中断与定时器

- [ ] **安装 IRQ 入口并接入中断控制器。** 计划先用传统 PIC 和 PIT。将 IRQ 映射到 CPU 异常区间以外的向量，只开放已安装处理程序的 IRQ。Trap 按向量调用设备处理函数，设备确认和中断结束通知（EOI）由相应的驱动或控制器代码处理。
- [ ] **验证周期中断和重复返回。** 完成 IRQ 配置后执行 `sti`，让内核在允许中断唤醒的循环中等待。PIT 处理函数先增加 `ticks`。验收时确认 `ticks` 能持续增长，普通内核代码仍能在中断返回后执行。调度阶段再把时钟事件交给调度器。

## 维护与验收

- 保留 `make test` 的 `int3`、致命异常、Ring 3 换栈和系统调用回归。接入周期 IRQ 时添加可重复的 QEMU 自检。对预期停机的自检，核对异常日志和运行结果后再判定通过。
- 每完成一项，勾选对应 TODO，写下实际测试命令和观察结果，并更新 [Trap 功能介绍](../architecture/trap.md) 中的当前能力。
