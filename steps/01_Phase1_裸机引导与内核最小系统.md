# 01 - Phase 1：裸机引导与内核最小系统

---

## 1. 详细的具体工程开发实施计划

### 1.1 阶段目标与交付物
构建一个符合标准 PC MBR 引导规范的最小启动内核，跨越 CPU 实模式、保护模式、长模式三道关隘，并实现基础的 C++ 裸机控制台格式化输出。
- **交付源文件**：
  - `boot/boot.S`：512 字节 MBR 引导扇区汇编源码
  - `linker.ld`：内核链接脚本（将内核代码定位到物理 1MB 内存）
  - `lib/cxx_runtime.cpp`：C++ 裸机运行时（Placement new / 异常桩 / 内存填充）
  - `drivers/vga.cpp` & `drivers/vga.hpp`：VGA 80x25 文本显存输出驱动
  - `drivers/serial.cpp` & `drivers/serial.hpp`：COM1 串口输入输出驱动
  - `kernel/kprintf.cpp` & `kernel/kprintf.hpp`：自研无依赖格式化内核打印库
  - `kernel/main.cpp`：内核 C++ 入口函数 `kmain()`
  - `Makefile`：顶级自动化编译与启动规则

### 1.2 详细开发排期与子任务
1. **子任务 1.1：编写 16 位实模式 MBR 扇区 (`boot/boot.S`)**
   - 初始化 CS/DS/ES/SS 段寄存器；
   - 利用 BIOS `INT 0x13, AH=0x42` 扩展读盘服务，将内核文件加载至物理地址 `0x100000`（1MB）；
   - 通过 8042 键盘控制器或 I/O 端口 `0x92` 快速激活 A20 地址线。
2. **子任务 1.2：跳入 32 位保护模式与 64 位长模式**
   - 构建临时 GDT（包含空描述符、32位代码段、32位数据段、64位代码段）；
   - 置位 `CR0.PE` 进入保护模式，执行 Far Jump 刷新段寄存器；
   - 建立初始 4 级页表（PML4, PDPT, PD, PT），设置恒等映射；
   - 开启 PAE（`CR4.PAE = 1`），置位 `EFER.LME = 1`，置位 `CR0.PG = 1` 正式激活 64 位长模式；
   - 跳转至 1MB 物理地址处执行 C++ 内核入口。
3. **子任务 1.3：实现 C++ 裸机运行时基础设施 (`lib/cxx_runtime.cpp`)**
   - 重载全局 Placement New 操作符；
   - 提供 `memset`、`memcpy`、`memcmp`、`strlen` 等基础内存与字符串内联实现；
   - 处理编译器要求的多态纯虚函数异常桩 `__cxa_pure_virtual()`。
4. **子任务 1.4：实现 VGA 与 COM1 双通道格式化输出 (`kernel/kprintf.cpp`)**
   - 将字符直接写入物理地址 `0xB8000`（VGA 文本显存），处理换行、滚屏与字符属性（黑底白字、色彩转义）；
   - 将字符写入 COM1 串口（I/O 端口 `0x3F8`），直通宿主机终端；
   - 实现无依赖的变参解析 `kprintf`（支持 `%d, %u, %x, %p, %s, %c`）。
5. **子任务 1.5：编写 `Makefile` 生成 Raw 软盘/硬盘启动镜像并启动 QEMU**
   - 编译 `boot.bin` 并校验其大小严格等于 512 字节且以 `0x55AA` 结尾；
   - 编译并链接 `kernel.bin`；
   - 使用 `dd` 将两者打包为 `simpleos.img`。

---

## 2. 我（用户）需要干什么

### 2.1 编译与执行命令
在工程根目录下执行一键构建与运行：

```bash
# 1. 编译全部阶段 1 代码并生成镜像
make build

# 2. 启动 QEMU 仿真器运行 SimpleOS
make run
```

### 2.2 验证结果与观察点
- **屏幕显示**：QEMU 弹出的 VGA 窗口中，第一行显示黑底绿字或彩色 Banner：
  ```
  [SimpleOS] ========================================
  [SimpleOS]  SimpleOS Kernel v0.1 (x86_64 Long Mode)
  [SimpleOS] ========================================
  [SimpleOS] Bootloader: MBR 512-byte Loaded.
  [SimpleOS] CPU State: 64-bit Long Mode Enabled.
  [SimpleOS] Console: VGA Text Mode 80x25 & COM1 Serial.
  ```
- **终端回显**：宿主机终端同时同步输出上述字符。
- **退出模拟器**：在终端按 `Ctrl + A` 随后按 `X` 即可正常退出 QEMU。

---

## 3. 实现原理与方式

### 3.1 MBR 引导规范与模式跳转物理过程
- **实模式局限**：开机时 CPU 处于 16 位实模式，仅能寻址 1MB 内存（通过 `Segment * 16 + Offset` 寻址），无法直接运行现代 64 位操作系统。
- **A20 地址线开启**：在 8086 时代由于地址回环特性，第 21 根地址线（A20）默认被拉低。必须通过向端口 `0x92` 写入相应位开启 A20，允许 CPU 寻址 1MB 以上物理内存。
- **长模式（Long Mode）激活关键步骤**：
  1. **PAE 开启**：将 `CR4` 的第 5 位（PAE，物理地址扩展）置 1，告知 CPU 内存分页将使用 64 位宽页表项；
  2. **EFER 寄存器配置**：通过 `rdmsr / wrmsr` 指令操作 MSR 寄存器 `0xC0000080`（IA32_EFER），将其第 8 位（LME，Long Mode Enable）置 1；
  3. **CR0.PG 开启分页**：加载 PML4 根页表物理地址到 `CR3`，随后将 `CR0.PG` 置 1。此时 CPU 硬件内部状态跃迁至 Compatibility / 64-bit 模式；
  4. **远跳转加载 64 位代码段**：执行 `jmp 0x08:long_mode_entry`，段选择子切换为 64 位描述符，CPU 正式以完整 64 位指令集执行代码。

### 3.2 VGA 显存映射与串口通信原理
- **VGA 文本显存**：PC 硬件将屏幕 80 列 × 25 行字符缓冲映射在物理内存 `0xB8000 ~ 0xBFFFF`。每个字符占 2 个字节：低字节是 ASCII 字符编码，高字节是前景/背景颜色属性位。修改该内存区域即直接驱动显示器成像。
- **COM1 串口寄存器**：标准的 16550 UART 芯片，通过 CPU 端口 I/O 指令 `inb / outb` 与端口 `0x3F8 ~ 0x3FF` 通信。初始化波特率后，当发送保持寄存器为空（LSR 的第 5 位置 1）时，向 `0x3F8` 写入字节即可通过串口输出至宿主机控制台。

---

## 4. 实现工具与依赖

| 依赖组件 | 参数 / 选项 | 作用说明 |
| :--- | :--- | :--- |
| `nasm` | `-f bin boot.S -o boot.bin` | 生成纯二进制 512 字节引导扇区，严格限制包含分区表与 0x55AA 签名 |
| `g++` | `-m64 -std=c++17 -ffreestanding -fno-rtti -fno-exceptions -fno-builtin -mno-red-zone` | 编译 64 位独立内核对象；`-mno-red-zone` 关键参数防止中断打坏栈顶 128 字节红色区域 |
| `ld` | `-n -T linker.ld` | 按照链接脚本布局 `.text`、`.rodata`、`.data`、`.bss` |
| `dd` | `conv=notrunc` | 将二进制段精确拼写入虚拟软盘/硬盘镜像文件 `simpleos.img` |
| `qemu-system-x86_64` | `-drive file=simpleos.img,format=raw -serial stdio` | 将磁盘挂载为启动盘，并将串口输出重定向至当前终端控制台 |
