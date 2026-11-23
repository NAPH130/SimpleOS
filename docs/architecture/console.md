# 内核控制台

内核通过 `kprintf` 输出格式化文本，通过 `kprintf_log` 输出带级别的日志。两者都使用 COM1 串口，调用前需要执行 `serial::init()`。当前接口用于串行输出。以后从多任务或中断处理中并发写日志时，需要考虑输出交错。 [控制台接口](../../include/kernel/console.hpp) · [控制台实现](../../src/kernel/console.cpp)

```cpp
kprintf("task %u ready\n", 3u);
kprintf_log(LogLevel::DEBUG, "trap vector %u\n", 3u);
```

## 格式化参数

| 格式 | 参数或含义 |
| --- | --- |
| `%d`、`%i` | `int`，按十进制输出 |
| `%u`、`%x`、`%X` | `unsigned int`，按十进制或十六进制输出 |
| `%c` | 提升后的 `int`，输出一个字符 |
| `%s` | `const char*`，空指针显示为 `(null)` |
| `%p` | `void*`，输出带 `0x` 的地址 |
| `%%` | 输出一个 `%` |

格式解析按 `%` 后的单个字符匹配。未识别的格式及末尾单独的 `%` 会原样输出。换行由调用方使用 `\n` 指定。

## 日志级别

| 级别 | 颜色 | 典型用途 |
| --- | --- | --- |
| `DEBUG` | 蓝色 | `int3` 自检等诊断信息 |
| `INFO` | 绿色 | 串口初始化、内核就绪等启动状态 |
| `WARN` | 黄色 | 可继续执行的异常情况 |
| `ERROR` | 红色 | 引导协议拒绝或无法处理的事件 |

日志格式为 `[DEBUG] trap vector 3`。ANSI 颜色只作用于方括号内的级别文字，正文使用终端默认颜色。`make test` 会在宿主机逐字节检查格式及颜色序列。 [控制台测试](../../tests/console/console_test.cpp)
