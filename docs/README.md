# SimpleOS 开发文档

| 要查的内容 | 文档 |
| --- | --- |
| 代码注释、模块设计和文档维护要求 | [SimpleOS 开发约定](AGENTS.MD) |
| 在 WSL 中安装依赖、启动 QEMU 或运行测试 | [构建、运行与验证](guides/getting-started.md) |
| 学过操作系统概念，想对照源码了解内核是怎样实现的 | [从操作系统课到动手写内核](guides/from-concepts-to-kernel.md) |
| 内核怎么启动、源码各目录负责什么 | [内核架构概览](architecture/overview.md) |
| 物理内存从哪里来、4 KiB 页帧怎样分配和释放 | [物理页帧分配](architecture/overview.md#物理页帧分配) |
| 用户虚拟地址怎样动态映射、使用和回收 | [动态用户页映射](architecture/user-paging.md) |
| CPU 异常如何处理、系统调用怎样进入内核，以及怎样读诊断日志 | [Trap 功能介绍](architecture/trap.md) |
| `kprintf` 支持哪些格式、怎样打印分级日志 | [内核控制台](architecture/console.md) |
| 接下来做什么、各阶段怎样验收 | [开发路线与阶段验收](plan/roadmap.md) |
| Trap 还需要实现哪些入口、怎样验证完成 | [Trap 开发计划](plan/trap.md) |

## 目录结构

```text
docs/
├── README.md       文档索引
├── AGENTS.MD       开发与文档维护约定
├── guides/         入门、环境、构建、运行与验证指南
├── architecture/   模块职责、接口和已实现机制
├── plan/           开发路线、模块 TODO 和验收方法
└── TEMP/           临时资料、课程截图与参考实验
```

## 文档维护

- 功能合入时，同步更新对应的架构说明和实际验证方法。启动命令或串口输出有变化时，核对运行指南和启动测试。
- 在 `plan/` 维护未完成事项和验收目标。完成一项后更新 TODO 和对应的实现文档。新增正式文档时，在本页补上索引和相关源码的相对链接。
- 根目录 README 提供项目概况和启动入口。临时草案和参考材料集中放在 `TEMP/`，可用于整理正式文档。
