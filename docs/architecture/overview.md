# 内核架构概览

SimpleOS 是面向操作系统课程实践的 x86_64 单体内核。当前可以在 QEMU BIOS 环境启动，具备 COM1 串口控制台、分级日志，以及内核态 `int3` 断点异常的进入和返回路径。

## 从引导到内核

1. `Makefile` 编译 `SOURCES` 中的 C++ 文件和 `ASM_SOURCES` 中的汇编文件。链接脚本生成 `build/kernel.elf`，指定入口 `kernel_main` 及内核的高半区虚拟地址。 [Makefile](../../Makefile) · [linker.ld](../../linker.ld)
2. QEMU 的 BIOS 从光盘镜像启动 Limine。Limine 根据配置加载 ELF，准备 64 位执行环境、初始栈和分页，再把控制权交给内核。 [limine.conf](../../src/boot/limine.conf) · [Limine 请求](../../src/boot/limine.cpp)
3. `kernel_main()` 初始化串口，输出字符画和日志，然后检查引导协议版本。检查通过后，内核加载 IDT 并执行 `int3` 自检。 [内核入口](../../src/kernel/main.cpp) · [Trap 功能介绍](trap.md)

内核使用 freestanding C++17，入口与寄存器现场相关的底层操作用汇编实现。串口由内核直接访问 COM1 I/O 端口，QEMU 把这些输出转发到宿主机终端。 [串口驱动](../../src/drivers/serial.cpp) · [内核控制台](console.md)

## 源码位置与职责

| 目录 | 职责 | 当前进度 |
| --- | --- | --- |
| `src/boot/` | 引导配置及引导协议交互 | Limine BIOS 启动 |
| `src/arch/x86_64/` | 描述符表、异常入口及 CPU 底层操作 | IDT、断点处理和停机 |
| `src/kernel/` | 内核初始化、控制台及系统调用分发 | 启动入口和控制台已实现，系统调用分发已留接口 |
| `src/drivers/` | 串口、定时器等硬件驱动 | COM1 串口输出 |
| `src/mm/` | 物理内存、内核堆和地址空间 | 预留内存接口 |
| `src/task/` | 进程管理与调度 | 预留调度接口 |
| `src/sync/` | 自旋锁、互斥锁和信号量 | 预留同步接口 |
| `src/fs/`、`src/loader/` | 文件访问和 ELF64 程序装载 | 预留文件访问及装载接口 |
| `src/lib/`、`src/user/` | 内核基础库、用户运行库和应用 | 用户态系统调用封装已留接口 |

接口头文件按模块位于 `include/`。宿主机测试在 `tests/`，QEMU 启动验证由 `scripts/test-boot.sh` 执行。新增内核实现时，将对应的 `.cpp` 或 `.S` 文件加入 Makefile 的源码列表。 [开发路线](../development/roadmap.md)
