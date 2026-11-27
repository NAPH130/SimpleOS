# SimpleOS

SimpleOS 是一个面向操作系统课程实践的 x86_64 教学内核，使用 freestanding C++17 和 x86_64 汇编。项目在同一个内核中逐步实现异常与系统调用、内存管理、任务调度等功能。

## 当前状态

当前使用 Limine 在 QEMU BIOS 环境启动，通过 COM1 串口显示字符画和分级日志。内核支持 `int3` 断点返回、缺页诊断、双重故障应急栈、物理页帧分配和内核堆。内核可以在独立页表中逐个启动用户程序。程序可通过系统调用输出和申请内存，正常退出或发生缺页后由内核回收地址空间。 [内核架构](docs/architecture/overview.md) · [QEMU 验收命令](docs/guides/getting-started.md)

## 快速启动

在 x86_64 的 WSL Ubuntu 中安装依赖，然后在仓库根目录运行以下命令。 [依赖安装与验证指南](docs/guides/getting-started.md)

```bash
make run    # 构建启动镜像并启动 QEMU
make test   # 检查控制台输出和 QEMU 启动
```

首次生成镜像需要联网获取固定版本的 Limine。看到 `after int3` 表示断点自检已返回。运行 `make run` 时按 `Ctrl+C` 退出 QEMU，再运行测试。

## 项目目录

```text
src/       项目源码
include/   各模块头文件
tests/     宿主机测试与 QEMU 自检源码
scripts/   QEMU 启动验证脚本
docs/      开发文档与索引
build/     本地生成的镜像、日志及引导器缓存
```

## 开发文档

- 文档目录与维护方式。 [开发文档索引](docs/README.md)
- CPU 异常的触发条件、处理流程和诊断字段。 [Trap 功能介绍](docs/architecture/trap.md)
- 用户页怎样动态分配、写入页表和回收。 [动态用户页映射](docs/architecture/user-paging.md)
- 接下来的阶段目标和模块待办。 [开发路线](docs/plan/roadmap.md) · [Trap 开发计划](docs/plan/trap.md)
