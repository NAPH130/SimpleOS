# SimpleOS

SimpleOS 是一个面向操作系统课程实践的教学项目，计划通过逐步实现最小内核、内存管理、任务调度等功能，理解操作系统各部分如何协作。

## 当前状态

当前支架使用 Limine 经 BIOS 启动 x86_64 内核，在 QEMU 中通过串口输出 `[SimpleOS] Hello, kernel!`。内核提供基础格式化输出 `kprintf`。其余子系统已在 `include/` 中预留 C++ 接口，并用 `// TODO` 标出后续实现点。这些接口目前只有声明，后续添加实现后再接入启动流程。

内核使用 freestanding C++17 和少量内联汇编，并在 QEMU 中运行。当前使用 Limine 引导器，而非自行编写 MBR；是否需要自研引导器仍需结合课程验收要求确认。

## 在 WSL 中构建和运行

在 Ubuntu 中安装所需工具：

```bash
sudo apt update
sudo apt install build-essential git xorriso qemu-system-x86 gdb
```

进入项目目录后执行：

```bash
make build  # 编译内核，下载固定版本的 Limine，生成 BIOS 启动 ISO
make run    # 启动 QEMU，在当前终端显示串口输出
make test   # 检查格式化输出及启动输出（启动测试最多等待 15 秒）
```

首次构建需要联网获取 Limine，后续构建使用 `build/deps/` 中的缓存。`make build/kernel.elf` 可以单独编译和链接内核，无需下载引导器。`make run` 启动后可通过 `Ctrl+C` 结束 QEMU；`make clean` 会删除整个 `build/`，包括生成文件和已下载的引导器。当前启动目标为 QEMU BIOS 光盘。

内核使用针对 freestanding 编译配置的 Ubuntu x86_64 GCC，不链接 Linux 标准库；这不等于一套独立的裸机交叉工具链。编译选项和目标架构假定开发环境为 x86_64 Ubuntu。

## 项目目录

```text
src/           项目源码
  boot/        引导配置与启动入口
  arch/x86_64/ x86_64 架构相关机制
  kernel/      内核初始化及核心服务
  drivers/     设备驱动
  mm/          内存管理
  task/        任务、进程及调度
  sync/        同步机制
  fs/          文件系统
  loader/      用户程序装载
  lib/         内核基础工具
  user/        用户态基础库与应用
include/       各模块头文件
tests/         各子系统的功能测试
scripts/       构建、运行及调试辅助脚本
docs/begin/    项目需求、设计和启动说明
docs/steps/    分阶段实施指南
docs/TEMP/     课程评分截图与参考实验材料
build/         本地构建产物（生成后不纳入版本管理）
```

目录职责、预留接口和对应阶段见 [项目目录结构说明](docs/begin/项目目录结构说明.md)。

## 下一步

启动流程和串口输出见 [最小内核启动说明](docs/begin/最小内核启动说明.md)。下一阶段从陷阱处理和系统调用接口开始，运行第一个独立编译的用户程序。
