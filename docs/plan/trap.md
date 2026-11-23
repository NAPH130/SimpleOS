# Trap 开发计划

Trap 模块负责把 CPU 异常、已配置设备的硬件中断，以及项目选定的 `int 0x80` 系统调用接入内核。它管理 IDT、保存和恢复 `TrapFrame`，并把事件交给相应的处理函数。内存管理决定缺页能否恢复，设备驱动和中断控制器处理 IRQ，系统调用分发器和调度器分别处理服务请求和任务切换。

## 当前基线

- [x] 初始化 IDT 并注册第 3 号断点向量 `#BP`。
- [x] 从汇编保存的 `TrapFrame` 读取向量号，在 C++ 中记录日志，再用 `iretq` 恢复执行。
- [x] `make test` 在 QEMU 中验证内核态 `int3` 的触发、处理和返回。启动自检期间保持可屏蔽中断关闭。

现有入口、栈布局和日志见 [Trap 功能介绍](../architecture/trap.md)。

## 1. CPU 异常诊断

- [x] **实现无效指令异常 `#UD`。** 已注册第 6 号向量并复用寄存器保存流程。`make test-ud` 用独立镜像执行 `ud2`，核对异常名称、向量号、指令地址与 `RIP`，并验证诊断停机路径。
- [ ] **统一有错误码和无错误码的入口。** 为两种异常分别生成入口。无硬件错误码时由汇编补 `0`，有硬件错误码时保留 CPU 压入的值。两种入口都传递相同布局的 `TrapFrame`。验收时触发实际的 `#GP` 或 `#PF`，检查向量、错误码和 `RIP` 对应正确。缺页诊断还应尽早读取 `CR2`，记录触发异常的线性地址。 [Intel 系统编程手册，第 4、6 章](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
- [ ] **补齐目标环境的 CPU 异常入口。** 对照处理器手册核对 `0–31` 的向量定义、保留项和硬件错误码。为有效向量提供专用处理或通用诊断，按可恢复、不可恢复配置返回或停机。保留 `int3` 的正常返回测试，并用代表性故障验证致命异常停机路径。

## 2. 内核栈与特权级入口

- [ ] **配置 GDT 和 64 位 TSS。** 定义内核与用户代码段，加载任务寄存器，为 Ring 3 进入 Ring 0 设置有效的 `RSP0`。确认 IDT 使用新 GDT 中的内核代码段选择子。验收时先回归 `int3`，再通过后续 Ring 3 自检核对异常现场和内核栈的位置。
- [ ] **为双重故障 `#DF` 配置应急栈。** 使用 TSS 的 IST 提供独立的异常栈，处理函数输出最小诊断后停机。验收时在专用测试中验证进入 `#DF` 处理函数并使用预期栈。 [Intel 系统编程手册，第 6 章](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
- [ ] **接通 Ring 3 的 `int 0x80`。** 在最小用户代码与栈映射就绪后，为 `0x80` 门设置 `DPL=3`，内核专用门保留 `DPL=0`。约定系统调用号、参数和返回值使用的寄存器，由 Trap 入口把 `TrapFrame` 交给系统调用分发器。验收时运行首个用户程序，通过 `sys_write` 输出文本并返回用户态，再通过 `sys_exit` 结束测试程序。 [系统调用接口](../../include/kernel/syscall.hpp) · [Intel 系统编程手册，第 6 章](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
- [ ] **区分内核态和用户态的致命异常。** 根据保存的 `CS` 判断异常来源。内核态致命异常走诊断停机路径。任务退出机制就绪后，将用户态的致命异常交给任务模块处理。验收时让测试用户程序触发 `#UD`，确认程序退出后内核可以继续运行。

## 3. 硬件中断与定时器

- [ ] **安装 IRQ 入口并接入中断控制器。** 计划先用传统 PIC 和 PIT。将 IRQ 映射到 CPU 异常区间以外的向量，只开放已安装处理程序的 IRQ。Trap 按向量调用设备处理函数，设备确认和中断结束通知（EOI）由相应的驱动或控制器代码处理。
- [ ] **验证周期中断和重复返回。** 完成 IRQ 配置后执行 `sti`，让内核在允许中断唤醒的循环中等待。PIT 处理函数先增加 `ticks`。验收时确认 `ticks` 能持续增长，普通内核代码仍能在中断返回后执行。调度阶段再把时钟事件交给调度器。

## 维护与验收

- 保留 `make test` 的 `int3` 回归。为致命异常、周期 IRQ 和 Ring 3 分别加入可重复的 QEMU 自检。对预期停机的自检，核对异常日志和运行结果后再判定通过。
- 每完成一项，勾选对应 TODO，写下实际测试命令和观察结果，并更新 [Trap 功能介绍](../architecture/trap.md) 中的当前能力。
