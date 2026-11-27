# 内核架构概览

SimpleOS 是面向操作系统课程实践的 x86_64 单体内核。当前可以在 QEMU BIOS 环境启动，具备串口控制台、CPU 异常诊断、物理页帧分配和内核堆。独立自检已验证用户进程在各自的页表下运行、通过系统调用分配内存、在正常退出或缺页后回收。 [运行与验证](../guides/getting-started.md)

## 从引导到内核

1. `Makefile` 编译 `SOURCES` 中的 C++ 文件和 `ASM_SOURCES` 中的汇编文件。链接脚本生成 `build/kernel.elf`，指定入口 `kernel_main` 及内核的高半区虚拟地址。 [Makefile](../../Makefile) · [linker.ld](../../linker.ld)
2. QEMU 的 BIOS 从光盘镜像启动 Limine。Limine 根据配置加载 ELF，准备 64 位执行环境、初始栈和分页，再把控制权交给内核。 [limine.conf](../../src/boot/limine.conf) · [Limine 请求](../../src/boot/limine.cpp)
3. `kernel_main()` 初始化串口，输出字符画和日志，然后检查引导协议版本。检查通过后，内核加载 IDT 并执行 `int3` 自检，再用 Limine 内存地图初始化物理页帧分配器。 [内核入口](../../src/kernel/main.cpp) · [Trap 功能介绍](trap.md)

内核使用 freestanding C++17，入口与寄存器现场相关的底层操作用汇编实现。串口由内核直接访问 COM1 I/O 端口，QEMU 把这些输出转发到宿主机终端。 [串口驱动](../../src/drivers/serial.cpp) · [内核控制台](console.md)

## 物理页帧分配

启动时，`boot::initialize_physical_memory()` 将可用区间和高半区直接映射偏移交给 `mm`。区间按物理地址升序提供，空区间和不足一页的两端会被跳过。分配器从最大可用区间的开头留出整页，用来存放描述符和位图，再从可分配范围扣除这些页。 [引导适配](../../src/boot/limine.cpp) · [Limine 内存地图协议](https://github.com/limine-bootloader/limine-protocol/blob/trunk/PROTOCOL.md#memory-map-feature)

初始化完成后，页表和内核堆统一调用 `mm::physical_to_virtual()`，把已由高半区直接映射覆盖的物理地址转成内核指针。偏移在初始化时保存，后续调用只依赖内存模块。新用户页的实际映射由页表模块建立。 [内存接口](../../include/mm/memory.hpp) · [直接映射实现](../../src/mm/physical_frames.cpp)

每个可分配区间各有一张位图，每个 4 KiB 页帧占一位。`allocate_frame()` 选择物理地址最小的空闲页帧，将该位置为 `1`，并输出物理地址。页帧耗尽时返回 `false`，保留原输出值。`free_frame()` 检查地址对齐、区间归属和当前占用状态，释放成功则清零并返回 `true`。位图的搜索起点会随分配和释放移动。当前由调用方串行访问分配器。 [内存接口](../../include/mm/memory.hpp) · [位图分配实现](../../src/mm/physical_frames.cpp)

`make test-frames` 使用真实的 Limine 内存地图，验证两个页帧能存放彼此独立的数据、无效释放会失败、耗尽后仍可释放并重新分配。用户页映射已复用这套接口来分配数据页和下级页表。 [页帧自检源码](../../tests/mm/frames_test.cpp) · [动态用户页映射](user-paging.md)

## 内核堆

`kmalloc()` 在页帧分配器初始化后按需申请内存。它返回满足 `max_align_t` 对齐的内核指针。内核堆沿高半区直接映射取连续物理页帧，建立内存段，再把空闲块拆成所需的大小。 `kfree()` 合并相邻空闲块，整段空闲后把页帧归还给分配器。较大的单次申请需要有足够长的连续物理页帧。地址空间的区间记录已经使用内核堆分配。 [内核堆实现](../../src/mm/kernel_heap.cpp) · [连续页帧分配](../../src/mm/physical_frames.cpp)

## 源码位置与职责

| 目录 | 职责 | 当前进度 |
| --- | --- | --- |
| `src/boot/` | 引导配置及引导协议交互 | Limine BIOS 启动 |
| `src/arch/x86_64/` | 描述符表、异常入口、页表和 CPU 底层操作 | 异常处理、Ring 3 入口、动态单页映射和回收 |
| `src/kernel/` | 内核初始化、控制台及系统调用分发 | 启动入口、控制台、用户输出、退出与内存系统调用 |
| `src/drivers/` | 串口、定时器等硬件驱动 | COM1 串口输出 |
| `src/mm/` | 物理内存、内核堆和地址空间 | 物理页帧、内核堆与独立用户地址空间 |
| `src/task/` | 进程管理与调度 | 顺序启动用户进程、退出回收，预留调度接口 |
| `src/sync/` | 自旋锁、互斥锁和信号量 | 预留同步接口 |
| `src/fs/`、`src/loader/` | 文件访问和 ELF64 程序装载 | 预留文件访问及装载接口 |
| `src/lib/`、`src/user/` | 内核基础库、用户运行库和应用 | 用户态输出、退出、申请和释放内存的封装 |

接口头文件按模块位于 `include/`。宿主机测试和异常自检源码在 `tests/`，QEMU 启动验证由 `scripts/test-boot.sh` 执行。新增内核实现时，将对应的 `.cpp` 或 `.S` 文件加入 Makefile 的源码列表。 [开发路线](../plan/roadmap.md)
