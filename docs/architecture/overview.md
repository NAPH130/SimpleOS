# 内核架构概览

SimpleOS 是面向操作系统课程实践的 x86_64 单体内核。当前可以在 QEMU BIOS 环境启动，具备串口控制台、CPU 异常诊断和物理页帧分配。独立自检已验证首次进入 Ring 3、用户异常换栈，以及用户程序通过系统调用输出和退出。 [运行与验证](../guides/getting-started.md)

## 从引导到内核

1. `Makefile` 编译 `SOURCES` 中的 C++ 文件和 `ASM_SOURCES` 中的汇编文件。链接脚本生成 `build/kernel.elf`，指定入口 `kernel_main` 及内核的高半区虚拟地址。 [Makefile](../../Makefile) · [linker.ld](../../linker.ld)
2. QEMU 的 BIOS 从光盘镜像启动 Limine。Limine 根据配置加载 ELF，准备 64 位执行环境、初始栈和分页，再把控制权交给内核。 [limine.conf](../../src/boot/limine.conf) · [Limine 请求](../../src/boot/limine.cpp)
3. `kernel_main()` 初始化串口，输出字符画和日志，然后检查引导协议版本。检查通过后，内核加载 IDT 并执行 `int3` 自检，再用 Limine 内存地图初始化物理页帧分配器。 [内核入口](../../src/kernel/main.cpp) · [Trap 功能介绍](trap.md)

内核使用 freestanding C++17，入口与寄存器现场相关的底层操作用汇编实现。串口由内核直接访问 COM1 I/O 端口，QEMU 把这些输出转发到宿主机终端。 [串口驱动](../../src/drivers/serial.cpp) · [内核控制台](console.md)

## 物理页帧分配

启动时，`boot::initialize_physical_memory()` 读取 Limine 内存地图，只把标记为可用的区间交给 `mm`。区间按物理地址升序提供，空区间和不足一页的两端会被跳过。分配器从最大可用区间的开头留出整页，用来存放区间描述符和位图，再从可分配范围扣除这些保留页。内核通过已有的高半区直接映射写入这些元数据。 [引导适配](../../src/boot/limine.cpp) · [Limine 内存地图协议](https://github.com/limine-bootloader/limine-protocol/blob/trunk/PROTOCOL.md#memory-map-feature)

每个可分配区间各有一张位图，每个 4 KiB 页帧占一位。`allocate_frame()` 选择物理地址最小的空闲页帧，将该位置为 `1`，并输出物理地址。页帧耗尽时返回 `false`，保留原输出值。`free_frame()` 检查地址对齐、区间归属和当前占用状态，释放成功则清零并返回 `true`。位图的搜索起点会随分配和释放移动。当前由调用方串行访问分配器。 [内存接口](../../include/mm/memory.hpp) · [位图分配实现](../../src/mm/physical_frames.cpp)

`make test-frames` 使用真实的 Limine 内存地图，验证两个页帧能存放彼此独立的数据、无效释放会失败、耗尽后仍可释放并重新分配。同一套接口也供日后的内核堆和用户页表使用。 [页帧自检源码](../../tests/mm/frames_test.cpp)

## 源码位置与职责

| 目录 | 职责 | 当前进度 |
| --- | --- | --- |
| `src/boot/` | 引导配置及引导协议交互 | Limine BIOS 启动 |
| `src/arch/x86_64/` | 描述符表、异常入口及 CPU 底层操作 | 异常 IDT、断点返回、缺页诊断、双重故障应急栈和 Ring 3 入口 |
| `src/kernel/` | 内核初始化、控制台及系统调用分发 | 启动入口、控制台、`sys_write` 和 `sys_exit` 分发 |
| `src/drivers/` | 串口、定时器等硬件驱动 | COM1 串口输出 |
| `src/mm/` | 物理内存、内核堆和地址空间 | 已实现 4 KiB 物理页帧的分配和释放，预留内核堆和地址空间接口 |
| `src/task/` | 进程管理与调度 | 预留调度接口 |
| `src/sync/` | 自旋锁、互斥锁和信号量 | 预留同步接口 |
| `src/fs/`、`src/loader/` | 文件访问和 ELF64 程序装载 | 预留文件访问及装载接口 |
| `src/lib/`、`src/user/` | 内核基础库、用户运行库和应用 | 已实现 `sys_write` 和 `sys_exit` 用户态封装 |

接口头文件按模块位于 `include/`。宿主机测试和异常自检源码在 `tests/`，QEMU 启动验证由 `scripts/test-boot.sh` 执行。新增内核实现时，将对应的 `.cpp` 或 `.S` 文件加入 Makefile 的源码列表。 [开发路线](../plan/roadmap.md)
