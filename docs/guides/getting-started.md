# 构建、运行与验证

当前目标环境是 x86_64 的 WSL Ubuntu。内核使用 GCC 编译 freestanding C++17 和 x86_64 汇编，由 Limine 在 QEMU 的 BIOS 模式下加载。

## 安装依赖

```bash
sudo apt update
sudo apt install build-essential git xorriso qemu-system-x86
```

## 启动内核

在仓库根目录执行 `make run`。Make 会生成 `build/simpleos.iso`，启动 QEMU，并把 COM1 串口连接到当前终端。首次生成镜像需要联网获取固定版本的 Limine，之后使用 `build/deps/limine/` 中的缓存。

字符画下方会显示启动状态和断点自检日志。最后一条 `[INFO]` 出现后，内核执行 `hlt` 等待。按 `Ctrl+C` 结束 QEMU。

```text
[INFO] COM1 serial console initialized.
[INFO] Limine base revision accepted.
[DEBUG] before int3
[DEBUG] breakpoint handled
[DEBUG] after int3
[INFO] Minimal kernel ready. Halting CPU.
```

## 构建和测试命令

| 命令 | 用途 |
| --- | --- |
| `make build` | 编译并链接内核，生成 BIOS 启动镜像。 |
| `make build/kernel.elf` | 只生成内核 ELF，适合先检查编译和链接。此目标无需下载引导器。 |
| `make -j8 test` | 并行运行全部 QEMU 异常自检，再运行宿主机控制台测试。 |
| `make test-ud` | 只运行 `#UD` 自检，验证第 6 号异常的故障地址和停机路径。 |
| `make test-gp` | 只运行 `#GP` 自检，验证第 13 号异常的故障地址和非零硬件错误码。 |
| `make test-pf` | 核对缺页异常的 `CR2`、`RIP` 和错误码。 |
| `make test-de`、`make test-ss`、`make test-df` | 分别验证除零、栈段和双重故障，`#DF` 日志确认应急栈。 |
| `make test-ring3` | 在独立镜像中启动用户态测试代码，验证用户栈写入、`#UD` 和内核入口换栈。 |
| `make test-syscall` | 验证 Ring 3 通过 `sys_write` 输出、收到返回值、拒绝无效参数，再通过 `sys_exit` 把状态交回内核。 |
| `make clean` | 删除 `build/`，包括引导器缓存和测试日志。 |

每个 QEMU 自检会在约 15 秒后结束，再检查串口内容。正常启动记录保存在 `build/boot-test.log`，故障自检记录在 `build/tests/trap-<异常名>.log`，例如 `build/tests/trap-pf.log`。 [Makefile](../../Makefile) · [启动测试脚本](../../scripts/test-boot.sh)

Ring 3 自检的日志在 `build/tests/trap-ring3.log`。其中 `RSP` 是用户栈顶，`on RSP0 stack=yes` 表示异常现场落在内核入口栈。

```text
[DEBUG] before entering Ring 3, expected RIP=0x400002, RSP=0x401ff8
[DEBUG] Ring 3 trap: CS=0x2b, SS=0x33, RSP=0x401ff8, on RSP0 stack=yes.
[ERROR] #UD Invalid opcode (vector 6), RIP=0x400002. Halting CPU.
```

系统调用自检的日志在 `build/tests/trap-syscall.log`。两行用户输出之间，程序还会检查无效输出目标、未映射地址、内核地址、跨页缓冲区、过大长度和未知调用号。

```text
[DEBUG] before Ring 3 syscall test
Ring 3 wrote 100%
Ring 3 resumed after sys_write
[INFO] kernel resumed after sys_exit, status=42
```
