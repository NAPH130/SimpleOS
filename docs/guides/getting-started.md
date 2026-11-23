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
| `make test` | 运行宿主机控制台测试，再启动 QEMU 检查字符画、日志和 `int3` 的返回路径。 |
| `make clean` | 删除 `build/`，包括引导器缓存和测试日志。 |

启动测试会在约 15 秒后结束 QEMU，再检查串口内容。看到 `Boot test passed.` 就表示检查通过。原始串口记录在 `build/boot-test.log`。 [Makefile](../../Makefile) · [启动测试脚本](../../scripts/test-boot.sh)
