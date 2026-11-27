# 构建、运行与验证

当前目标环境是 x86_64 的 WSL Ubuntu。内核使用 GCC 编译 freestanding C++17 和 x86_64 汇编，由 Limine 在 QEMU 的 BIOS 模式下加载。

## 安装依赖

```bash
sudo apt update
sudo apt install build-essential git xorriso qemu-system-x86
```

## 启动内核

在仓库根目录执行 `make run`。Make 会生成 `build/simpleos.iso`，启动 QEMU，并把 COM1 串口连接到当前终端。首次生成镜像需要联网获取固定版本的 Limine，之后使用 `build/deps/limine/` 中的缓存。

字符画下方会显示启动状态、断点自检和物理页帧分配器的初始化日志。最后一条 `[INFO]` 出现后，内核执行 `hlt` 等待。按 `Ctrl+C` 结束 QEMU。

```text
[INFO] COM1 serial console initialized.
[INFO] Limine base revision accepted.
[DEBUG] before int3
[DEBUG] breakpoint handled
[DEBUG] after int3
[INFO] Physical frame allocator initialized.
[INFO] Minimal kernel ready. Halting CPU.
```

## 构建和测试命令

| 命令 | 用途 |
| --- | --- |
| `make build` | 编译并链接内核，生成 BIOS 启动镜像。 |
| `make build/kernel.elf` | 只生成内核 ELF，适合先检查编译和链接。此目标无需下载引导器。 |
| `make -j8 test` | 并行运行全部 QEMU 自检，再运行宿主机控制台测试。 |
| `make test-ud` | 只运行 `#UD` 自检，验证第 6 号异常的故障地址和停机路径。 |
| `make test-gp` | 只运行 `#GP` 自检，验证第 13 号异常的故障地址和非零硬件错误码。 |
| `make test-pf` | 核对缺页异常的 `CR2`、`RIP` 和错误码。 |
| `make test-de`、`make test-ss`、`make test-df` | 分别验证除零、栈段和双重故障，`#DF` 日志确认应急栈。 |
| `make test-ring3` | 用动态分配的用户页验证取指、用户栈写入、`#UD` 和内核入口换栈。 |
| `make test-syscall` | 验证独立页表、用户输出、用户内存分配与释放、正常退出与缺页退出后的回收，以及页帧耗尽时创建地址空间和部分建表的失败路径。 |
| `make test-frames` | 验证 4 KiB 页帧分配与耗尽，以及内核堆的对齐、拆分、合并、跨页分配和页帧回收。日志保存在 `build/tests/frames-test.log`。 |
| `make clean` | 删除 `build/`，包括引导器缓存和测试日志。 |

每个 QEMU 自检会在约 15 秒后结束，再检查串口内容。正常启动记录保存在 `build/boot-test.log`，故障自检记录在 `build/tests/trap-<异常名>.log`，例如 `build/tests/trap-pf.log`。 [Makefile](../../Makefile) · [启动测试脚本](../../scripts/test-boot.sh)

内核堆和页帧自检通过后分别输出 `[INFO] Kernel heap self-test passed.` 和 `[INFO] Physical frame allocator self-test passed.`。 [内核堆](../architecture/overview.md#内核堆) · [物理页帧分配](../architecture/overview.md#物理页帧分配)

Ring 3 自检的日志在 `build/tests/trap-ring3.log`。其中 `RSP` 是用户栈顶，`on RSP0 stack=yes` 表示异常现场落在内核入口栈。

```text
[DEBUG] before entering Ring 3, expected RIP=0x400002, RSP=0x401ff8
[DEBUG] Ring 3 trap: CS=0x2b, SS=0x33, RSP=0x401ff8, on RSP0 stack=yes.
[ERROR] #UD Invalid opcode (vector 6), RIP=0x400002. Halting CPU.
```

系统调用自检的日志在 `build/tests/trap-syscall.log`。两个用户程序先各自建立页表，再顺序运行。它们从相同的虚拟栈地址读到各自的值。第一个程序申请并释放跨页内存，然后以 `42` 退出。第二个程序访问未映射的 `0x1000`，由内核结束。自检在每次返回后检查原页表是否恢复，最后核对两次运行释放的页帧。 [动态用户页映射](../architecture/user-paging.md)

```text
[DEBUG] before Ring 3 syscall test
Ring 3 wrote 100%
Ring 3 resumed after sys_write
User memory allocated, released and reused
[INFO] kernel resumed after sys_exit, status=42
Process B read its own stack data
[ERROR] User page fault at 0x1000. Terminating process.
[INFO] Isolated user address spaces reclaimed after exit and fault.
[INFO] Virtual address ranges allocated and reused.
[INFO] User mapping rollback verified.
```
