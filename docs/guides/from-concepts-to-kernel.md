# 从操作系统课到动手写内核

你在课上学过的「进程」「虚拟内存」和「系统调用」，最终都要落实到几件具体的事上。CPU 接下来执行哪条指令？它用哪块内存当栈？程序以什么权限访问内存？发生异常时，CPU 去哪里找处理代码？这篇指南用 SimpleOS 的启动过程和一次断点异常回答这些问题，再说明怎样从当前内核走到第一个用户程序。

读代码时可以先记住四个位置。**`RIP` 指向执行位置，`RSP` 指向当前栈顶，IDT 给出异常处理入口，栈上的 `TrapFrame` 保存这一次的现场。** 第一次阅读可以先跑一遍启动，再按文中的顺序打开源码。

## 1. 内核怎样开始执行

平时写 C++ 程序时，已有的操作系统会加载可执行文件，运行时再准备好调用 `main()` 的环境。现在我们要自己实现提供这些服务的**内核**。开机后，内核管理 CPU 时间、内存和设备，并通过系统调用向用户程序提供服务。之后还可以在内核之上添加用户库、命令解释器和应用程序。

SimpleOS 从虚拟机启动，入口是 `kernel_main()`。它使用 **freestanding C++17**，也就是在由自己管理运行环境的情况下使用 C++。入门阶段可以照常用函数、结构体和命名空间。输出文本时，内核通过自己的 `kprintf_log()` 和串口驱动访问设备。 [构建规则](../../Makefile) · [内核控制台](../architecture/console.md)

1. **编译和链接。** GCC 把 `.cpp` 和 `.S` 编译成目标文件。链接器依据 `linker.ld` 生成 `build/kernel.elf`，其中的 ELF 程序头描述了要加载的内容，ELF 文件头记录入口地址。本项目把 `kernel_main` 作为入口，将内核链接到高地址区域。 [Makefile](../../Makefile) · [链接脚本](../../linker.ld)
2. **引导。** 构建规则把内核和 Limine 打包到 `build/simpleos.iso`。QEMU 启动虚拟机，虚拟机的 BIOS 从镜像启动 Limine。Limine 加载内核，准备 64 位执行环境、初始栈和页表，然后把执行交给 ELF 入口。 [Limine 配置](../../src/boot/limine.conf) · [Limine 引导协议](https://github.com/limine-bootloader/limine-protocol/blob/trunk/PROTOCOL.md)
3. **内核接管。** `kernel_main()` 设置 COM1 串口，打印字符画，检查引导协议版本，然后初始化异常入口。QEMU 将串口输出转发到启动它的终端。这让我们在图形界面和用户程序开发之前就能看到内核做了什么。 [内核入口](../../src/kernel/main.cpp) · [串口驱动](../../src/drivers/serial.cpp)

在项目根目录执行 `make run`，可以看到 SimpleOS 字符画和以下日志片段。内核随后执行 `hlt`，可以按 `Ctrl+C` 退出 QEMU。首次配置环境时参照 [构建、运行与验证](getting-started.md)。

```text
[DEBUG] before int3
[DEBUG] breakpoint handled
[DEBUG] after int3
[INFO] Physical frame allocator initialized.
[INFO] Minimal kernel ready. Halting CPU.
```

## 2. 读底层代码时先认出三个状态

**执行位置。** CPU 根据 `RIP` 取机器指令。编译后的每个 C++ 函数都对应一段机器指令。调用函数时，x86_64 的 `call` 会把返回地址压栈，再把执行位置转到被调用函数。执行 `ret` 时，CPU 从栈上取出返回地址并继续执行。这就是「函数执行完回到哪里」在硬件上的体现。

**栈的位置。** `RSP` 保存当前栈顶地址。栈是内存的一部分，可以保存返回地址、临时变量和寄存器。x86_64 的栈向低地址增长。比如 `RSP` 为 `0x8000` 时，`pushq` 一个 8 字节的值会先把 `RSP` 减到 `0x7ff8`，再把值写在那里。当前启动代码使用 Limine 准备的栈。后面看到 `gdt.cpp` 把栈指针设在数组末尾，也是为了让第一次压栈写到数组内部。 [栈配置](../../src/arch/x86_64/gdt.cpp)

**当前权限。** CPU 在内核态（Ring 0）可以执行管理机器的操作，例如加载 IDT。在用户态（Ring 3）执行时，CPU 限制特权指令和内存访问。两种状态都由同一套 CPU 指令执行机制支撑，硬件根据当前特权级和访问对象的权限决定这次操作是否允许。内核可通过预先配置的入口接住异常或服务请求。 [Intel 系统编程手册，保护机制与中断处理](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)

代码里的指针通常表示**虚拟地址**。CPU 根据当前页表把它转换成物理地址，并检查是否允许这次访问。例如将虚拟页 `0x400000` 映射到一个物理页框后，程序可通过 `0x400008` 访问那个页框内偏移为 8 的字节。Limine 设置启动所需的映射，SimpleOS 为每个进程建立独立的用户映射。两个进程访问同一个虚拟地址时，CPU 可以根据各自的页表找到不同的物理页帧。 [Intel 系统编程手册，分页](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf) · [动态用户页映射](../architecture/user-paging.md)

## 3. GDT、IDT、TSS 和 TrapFrame 分别存什么

| 名称 | 保存的内容 | SimpleOS 中从哪里读 |
| --- | --- | --- |
| GDT（全局描述符表） | 代码段、数据段以及 TSS 的描述符。段描述符提供段类型、特权级等属性。 | [gdt.cpp](../../src/arch/x86_64/gdt.cpp) |
| IDT（中断描述符表） | 从向量号到处理入口的映射。表项还包含目标代码段选择子、门的权限和可选的应急栈编号。 | [idt.cpp](../../src/arch/x86_64/idt.cpp) |
| TSS（任务状态段） | CPU 进入内核时可使用的栈顶地址，其中有 `RSP0`，也有为特定事件准备的 IST 栈地址。 | [gdt.cpp](../../src/arch/x86_64/gdt.cpp) |
| TrapFrame（异常现场） | 某一次事件的寄存器值，以及向量号和错误码。C++ 结构体描述这些值在栈上的排列。 | [trap.hpp](../../include/arch/x86_64/trap.hpp) · [trap_entry.S](../../src/arch/x86_64/trap_entry.S) |

### 段配置就是填写 CPU 能读懂的描述符

打开编辑器里的 `segments.h`，可以看到 `X86_64_USER_CODE_SELECTOR` 是 `0x2b`。这个数称为**段选择子**。它的第 2 位为 0，表示使用 GDT。把它右移 3 位得到 5，表示读取从 0 开始编号的 `gdt[5]`。低两位为 3，表示选择子携带的请求特权级 RPL 为 3。`gdt[5]` 的描述符声明这是一个 64 位用户代码段，描述符特权级 DPL 为 3。对应的用户数据选择子是 `0x33`，索引是 6，可用于用户栈段。 [段选择子常量](../../include/arch/x86_64/segments.h) · [GDT 表项](../../src/arch/x86_64/gdt.cpp)

可以把 `CS:RIP` 分开理解。**`CS` 选取代码段配置，`RIP` 决定执行位置。** 未来两个用户程序可以共用同一个 `CS=0x2b`，并分别从自己的入口地址执行。`SS` 是栈段选择子，栈顶地址由 `RSP` 提供。SimpleOS 使用 64 位模式，普通代码和栈的地址范围与按页隔离主要由分页机制处理。GDT 中的描述符仍用于确定代码执行模式和特权级。 [Intel 系统编程手册，内存管理与保护机制](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)

### TSS 提供进内核时可用的栈

Ring 3 程序触发异常后，普通中断门可以把执行切到 Ring 0。CPU 要保存用户执行现场，用户栈也可能正是访问出错的地方。在门未选择 IST 栈的情况下，CPU 从 TSS 的 `RSP0` 取得内核栈顶，换到那里再保存现场。SimpleOS 把 `tss.rsp[0]` 指向 16 KiB 的 `ring0_entry_stack` 末尾。`make test-ring3` 可以观察这次换栈。 [TSS 初始化](../../src/arch/x86_64/gdt.cpp) · [运行与验证](getting-started.md)

双重故障 `#DF` 走另一条选栈路径。它的 IDT 门设置了 `IST=1`，CPU 从 TSS 的第一个 IST 表项取一个独立的应急栈，即使事件发生时已经在 Ring 0 也会换栈。这个路径已用 `make test-df` 验证。正常启动的内核态 `int3` 保持 Ring 0，门的 IST 为 0，继续使用当前内核栈。 [IDT 初始化](../../src/arch/x86_64/idt.cpp) · [Trap 功能介绍](../architecture/trap.md)

`initialize_traps()` 会先调用 `initialize_segments()`。其中 `lgdt` 让 CPU 知道新 GDT 的位置，汇编通过远返回重新加载内核 `CS`，再用 `ltr` 加载 TSS 选择子。接下来初始化 IDT，并用 `lidt` 告诉 CPU 这张表的位置。**先把表和栈准备好，CPU 遇到事件时才有可用的入口与现场保存位置。** [GDT 加载汇编](../../src/arch/x86_64/gdt_load.S) · [IDT 初始化](../../src/arch/x86_64/idt.cpp)

## 4. 顺着一次 `int3` 跟进源码

启动入口执行 `asm volatile("int3" ::: "memory");` 时，真正运行的是 x86_64 的 `int3` 指令。CPU 把它识别为断点异常 `#BP`，向量号固定为 3。项目在启动时配置了 IDT 第 3 项，硬件现在直接使用它。这个例子可以观察一次完整的「打断代码、处理事件、继续执行」。 [内核入口](../../src/kernel/main.cpp) · [异常向量表](../../include/arch/x86_64/exception_vectors.inc)

1. **CPU 查 IDT 并保存返回信息。** 它根据向量 3 找到处理入口，把返回所需的 `RIP`、`CS`、`RFLAGS`、`RSP` 和 `SS` 保存在栈上，然后进入 `x86_64_exception_3`。对于 `int3`，保存的 `RIP` 指向断点指令之后。 [IDT 初始化](../../src/arch/x86_64/idt.cpp) · [Intel 系统编程手册，64 位中断与异常处理](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
2. **汇编把现场整理成统一布局。** `#BP` 没有硬件错误码，入口压入一个 `0` 占位，接着压入向量号 3，再保存通用寄存器。最后压入的是 `r15`，它位于栈顶，所以 `TrapFrame` 的第一个字段就是 `r15`。汇编将现场的起始地址放进 `rdi`，作为 C++ 函数的第一个参数。 [异常入口汇编](../../src/arch/x86_64/trap_entry.S) · [TrapFrame 定义](../../include/arch/x86_64/trap.hpp)
3. **C++ 根据事件决定怎么处理。** `trap_dispatch(TrapFrame* frame)` 收到一个指向栈上现场的指针。`frame->vector` 表示读取该结构里的 `vector` 字段。第 3 号向量打印 `breakpoint handled` 后返回。缺页等其他异常进入诊断路径。 [C++ 分发函数](../../src/arch/x86_64/trap.cpp)
4. **汇编和 CPU 分工恢复现场。** 汇编弹出通用寄存器，跳过向量号和错误码，执行 `iretq`。CPU 从栈上恢复它先前保存的返回信息，从断点后的指令继续。随后打印的 `after int3` 表明返回路径走通了。 [异常入口汇编](../../src/arch/x86_64/trap_entry.S)

再看一次**由 CPU 检测到的故障**。运行 `make test-pf` 时，自检执行 `movq (%rdi), %rax`，尝试从未映射的 `0x1000` 读取数据。CPU 在地址转换时触发第 14 号异常 `#PF`。日志里的 `RIP` 是执行读取的那条机器指令的地址，`CR2` 是它想访问的虚拟地址 `0x1000`。前者用于找是哪条指令出错，后者用于找哪个地址的映射出了问题。当前处理函数打印诊断后停机。后续内存管理可以在确认某类缺页可修复后补建映射，让 CPU 重试故障指令。 [缺页测试源码](../../tests/trap/pf_test.cpp) · [缺页处理函数](../../src/arch/x86_64/trap.cpp)

这条路径也展示了项目怎样划分职责。汇编入口负责硬件要求的栈布局与指令。头文件用 C++ 结构体描述这份布局，并声明模块接口。C++ 实现根据字段决定如何处理。在以后增加设备中断时，入口可以继续负责保存和恢复现场，设备驱动可以专注于处理设备事件。 [源码目录与职责](../architecture/overview.md)

## 5. 怎样从这里走到第一个用户程序

`make test-ring3` 观察用户程序发生异常后怎样换到内核栈。`make test-syscall` 在独立页表中顺序运行两个程序，观察用户程序怎样向内核请求输出和内存，以及内核怎样在退出后回收进程。 [运行与验证](getting-started.md)

1. **准备指令和栈。** 自检通过页帧分配器取得独立的代码页和栈页，并把它们映射到用户虚拟地址 `0x400000` 和 `0x401000`。页表记录访问权限，Ring 3 可以从代码页取指，也可以向栈页写入数据。 [动态用户页映射](../architecture/user-paging.md) · [Ring 3 自检源码](../../tests/trap/ring3_test.cpp)
2. **第一次进入 Ring 3。** `enter_user_mode()` 在内核栈上填好用户 `RIP`、`RSP`、`CS`、`SS` 和 `RFLAGS`，再用异常返回指令 `iretq` 读取这份现场。CPU 依照这些值进入 Ring 3，执行 `pushq` 和 `ud2`。随后 CPU 递送 `#UD`，使用 TSS 的 `RSP0` 换回内核栈。 [用户态入口汇编](../../src/arch/x86_64/enter_user_mode.S)
3. **用户程序发出系统调用。** 用户侧的 `sys_write` 封装把调用号和参数放入约定的寄存器，然后执行 `int 0x80`。IDT 的 `0x80` 号门设为 `DPL=3`，允许用户程序执行这条指令，处理入口位于 Ring 0。CPU 使用 TSS 的 `RSP0` 换栈并保存用户现场，汇编再把现场交给内核分发器。 [用户态封装](../../src/user/syscall.cpp) · [Trap 功能介绍](../architecture/trap.md)
4. **内核执行服务并返回。** `sys_write` 接收用户缓冲区的地址和长度。检查整段缓冲区后，内核按长度输出字节，再把数量写入现场的 `rax` 字段。汇编用 `iretq` 返回，用户程序读取返回值并继续执行。调用 `sys_exit` 时，内核回到启动程序的位置，并得到退出状态。 [内核系统调用分发](../../src/kernel/syscall.cpp)

一个用户程序跑通后，调度器才需要在多个任务之间保存和恢复执行状态。可以让每个任务带有独立的内核栈。任务 A 在内核里被切走时，保留它的内核栈和恢复位置。任务 B 有自己的栈与恢复位置。调度到 A 时恢复 A 的内核栈指针，继续它先前的内核执行路径。如果切换发生在一次用户态 Trap 中，最终由 `iretq` 使用 A 栈上的返回现场返回用户态。调度器还要在切换时安排好下一次用户态进入内核所用的 `RSP0`。这些属于后续任务管理设计，当前的静态入口栈用于单次用户态往返的准备。 [调度接口](../../include/task/scheduler.hpp) · [开发路线](../plan/roadmap.md)

## 6. 课本里的其他机制怎样落到代码里

| 课本概念 | 实现时具体做什么 | 第一次能看到的结果 |
| --- | --- | --- |
| 物理内存管理 | 读取 Limine 的可用内存区间，用位图记录每个 4 KiB 页帧是否已占用。 | `make test-frames` 验证申请两个不同的页帧，释放其中一个后重新分配。 [位图分配实现](../../src/mm/physical_frames.cpp) |
| 虚拟地址空间 | 创建和修改页表，将选定的虚拟地址映射到物理页框，设置用户权限。 | 用户程序可以执行代码、读写自己的栈，访问受保护的页会触发异常。 [内存管理接口](../../include/mm/memory.hpp) |
| 定时器和调度 | 先建立任务状态和上下文切换，再让定时器周期性发出硬件中断，给调度器提供时钟事件。 | 先看到两个任务主动让出 CPU 并交替打印，再看到时间片轮转。 [调度接口](../../include/task/scheduler.hpp) · [Trap 开发计划](../plan/trap.md) |
| 锁和信号量 | 保护共享数据。自旋锁用原子操作竞争锁，阻塞式互斥锁和信号量需要任务等待与唤醒。 | 两个任务反复更新同一个余额，运行结果符合预期。 [同步接口](../../include/sync/primitives.hpp) |
| 文件系统和程序装载 | 文件系统提供读取接口。装载器读取 ELF 程序头，准备代码和数据映射，再从 ELF 入口启动用户程序。 | 先读出一个文件，再从文件装载并运行用户程序。 [文件系统接口](../../include/fs/vfs.hpp) · [ELF 装载接口](../../include/loader/elf.hpp) |

两个自检进程各有自己的页表，都把用户栈映射到从 `0x401000` 开始的位置。它们读到各自栈里的数据，退出后由内核拆除各自的映射并回收物理页。定时器中断可以先单独验证，让处理函数每次中断只增加计数，等任务切换跑通后再用于抢占。 [进程自检](../../tests/trap/syscall_test.cpp) · [开发路线与阶段验收](../plan/roadmap.md) · [Trap 开发计划](../plan/trap.md)

## 7. 用现有实验练习读代码

1. **跟一次正常返回。** 执行 `make test-boot`，打开 `build/boot-test.log`，找到三条和断点有关的 DEBUG 日志。沿着 `kernel_main()` → `idt.cpp` → `trap_entry.S` → `trap_dispatch()` → `iretq` 找到每次交接发生的位置。从最后一条 `after int3` 反推 CPU 最后恢复了哪个执行地址。
2. **区分执行地址和访问地址。** 执行 `make test-pf`，打开 `build/tests/trap-pf.log`，在 `#PF` 日志里圈出 `RIP` 和 `CR2`。和测试里打印的 `before unmapped read` 对照，确认一个对应读取指令，一个对应 `0x1000`。
3. **观察两种内核换栈。** 执行 `make test-df`，查看第 8 号向量选用的应急栈对应的 `IST=yes`。再执行 `make test-ring3`，在 `build/tests/trap-ring3.log` 中找到 `on RSP0 stack=yes`。对照 `gdt.cpp` 找到两块栈的地址分别写到了 TSS 的哪里。 [运行命令及日志位置](getting-started.md) · [GDT 和 TSS](../../src/arch/x86_64/gdt.cpp)
4. **跟一次系统调用往返。** 执行 `make test-syscall`，查看 `build/tests/trap-syscall.log`。找到用户程序第二次打印的位置，确认它从第一次 `sys_write` 返回后仍能执行。`status=42` 表示第一个程序成功退出，后面的缺页日志来自第二个程序。 [用户程序自检源码](../../tests/trap/syscall_user.cpp) · [内核启动自检源码](../../tests/trap/syscall_test.cpp)

读完这几段后，可以用同样的办法读新模块。先确定硬件或前一个模块会提供什么，再找 C++ 中的接口和数据，最后用可观察的日志或测试检查这条路径是否走通。
