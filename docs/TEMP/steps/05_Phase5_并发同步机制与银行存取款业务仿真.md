# 05 - Phase 5：并发同步机制与银行存取款业务仿真

---

## 1. 详细的具体工程开发实施计划

### 1.1 阶段目标与交付物
实现多任务操作系统最核心的同步与互斥原语：**硬件原子自旋锁（`SpinLock`）**、**睡眠信号量（`Semaphore`，P-V 原语）** 与 **互斥锁（`Mutex`）**。
严格依照课程大纲要求，构建**经典银行存取款业务并发仿真程序（`apps/bank_demo.cpp`）**，直观对比“无锁竞态产生数据灾难”与“P-V 锁保护确保账目绝对平衡”，为答辩提供决定性质检证据。
- **交付源文件**：
  - `sync/spinlock.cpp` & `spinlock.hpp`：基于 x86 `lock cmpxchg` 的原子自旋锁及 RAII 守卫类（`LockGuard`）
  - `sync/semaphore.cpp` & `semaphore.hpp`：带阻塞等待队列的计数信号量（P-V 原语）
  - `sync/mutex.cpp` & `mutex.hpp`：睡眠互斥锁
  - `apps/bank_demo.cpp`：银行并发存取款业务仿真测试程序

### 1.2 详细开发排期与子任务
1. **子任务 5.1：实现硬件级原子自旋锁 (`sync/spinlock.cpp`)**
   - 使用 x86 内联汇编指令 `lock bts` 或 `lock cmpxchg` 实现 `acquire()`，结合 CPU 节能指令 `pause` 避免总线争用热循环；
   - 实现 `release()`，释放锁并执行内存屏障；
   - 编写 C++ RAII 类 `SpinLockGuard`，在构造函数加锁，析构函数自动解锁，彻底消除因中途 `return` 或异常遗漏解锁导致的系统死锁。
2. **子任务 5.2：实现带阻塞队列的计数信号量 (`sync/semaphore.cpp`)**
   - 字段设计：整型计数器 `int value`，以及阻塞等待队列 `WaitQueue`；
   - 实现 `wait()`（P 原语）：
     - 关中断或加自旋锁保护内部状态；
     - `value--`；若 `value < 0`，将当前运行任务状态设为 `Blocked`，挂入 `WaitQueue` 尾部，调用调度器主动让出 CPU；
   - 实现 `signal()`（V 原语）：
     - `value++`；若 `value <= 0`，从 `WaitQueue` 头部弹出一个阻塞任务，将其状态修改为 `Ready` 并挂入调度就绪队列。
3. **子任务 5.3：设计银行存取款业务并发仿真模型 (`apps/bank_demo.cpp`)**
   - 建立银行账户模型：
     ```cpp
     struct BankAccount {
         int64_t balance{10000}; // 初始本金 10000 元
         Semaphore sem{1};       // 互斥信号量 (初值=1)
     };
     ```
   - 创建 10 个并发线程，每个线程循环执行 1000 次“存入 10 元”与“取出 10 元”：
     - **实验组 A（无锁竞争环境）**：直接读写 `balance`，故意放大临界区指令窗口；
     - **实验组 B（P-V 信号量互斥环境）**：在存取款操作前调用 `sem.wait()`，操作完成后调用 `sem.signal()`。
4. **子任务 5.4：进阶加分拓展：死锁避免与银行家算法演示**
   - 模拟多账户交叉转账（如账户 A 向 B 转账，同时账户 B 向 A 转账）可能产生的死锁；
   - 演示“资源按序分配策略”与“银行家算法”安全状态检测，有效预防死锁。

---

## 2. 我（用户）需要干什么

### 2.1 编译与执行命令
在工程根目录下执行构建并运行：

```bash
make run
```

### 2.2 验证结果与观察点
- 观察控制台输出的现场对比数据：
  ```
  [SimpleOS] ========================================================
  [SimpleOS]    Phase 5: Concurrency & Synchronization Benchmark     
  [SimpleOS] ========================================================
  [SimpleOS] Initial Bank Account Balance: $10,000.00
  [SimpleOS] Spawning 10 Concurrent Worker Threads (1,000 tx/thread)...
  
  [Experiment A: Unsafe Concurrency (No Locks)]
    -> Worker threads running without synchronization...
    -> Race conditions detected during preemptive timer ticks!
    -> Expected Balance : $10,000.00
    -> Actual Balance   : $7,420.00   <-- [FATAL ERROR: Lost Update Bug Reproduced!]
  
  [Experiment B: Safe Concurrency (Protected by Semaphore P-V)]
    -> Resetting Account Balance to $10,000.00...
    -> Worker threads running with Semaphore(1) Critical Section...
    -> Expected Balance : $10,000.00
    -> Actual Balance   : $10,000.00  <-- [SUCCESS: 100% Perfectly Balanced!]
  [SimpleOS] ========================================================
  ```
- **核心判定**：实验组 A 必然出现账目差额（如少了几千元），实验组 B 无论重试多少次均绝对等于 10000 元。该对比结果可直接截图放入答辩 PPT。

---

## 3. 实现原理与方式

### 3.1 为什么无锁存取款必然发生账目丢失（Race Condition 原理）？
- 在 C++ 源码中看似只有一行 `balance += amount`，但在汇编机器码层面由 3 条独立微指令组成：
  ```nasm
  mov rax, [balance]   ; 1. 读内存到寄存器
  add rax, 10          ; 2. 寄存器计算加法
  mov [balance], rax   ; 3. 写回寄存器到内存
  ```
- **时钟中断切断临界区**：假设线程 1 执行完第 1 步读出 10000，此时 PIT 时钟中断发生，CPU 被抢占切换给线程 2。线程 2 完整执行了“存 10 元写回 10010”，随后切回线程 1。线程 1 继续执行其第 2、3 步，将其私有寄存器中的 10010 写回，直接覆盖并丢失了线程 2 的修改！这就是典型的“丢失更新”（Lost Update）数据竞争。

### 3.2 P-V 原语保证互斥的数学逻辑
- 信号量 $S$ 的初始值为 1（二值信号量/互斥量）。
- **$P(S)$ 操作**：$S \leftarrow S - 1$。
  - 若 $S \ge 0$，代表资源空闲，当前线程独占进入临界区；
  - 若 $S < 0$，代表临界区已有线程在执行，当前线程被放入等待队列睡眠，不再消耗 CPU 时间。
- **$V(S)$ 操作**：$S \leftarrow S + 1$。
  - 若 $S \le 0$，代表队列中还有正在等待的线程，硬件原语唤醒其中一个就绪。
- **结论**：任意时刻进入临界区执行存取款操作的线程数至多为 1，从数学上消除了并发指令重叠，从而确保账目绝对一致。

---

## 4. 实现工具与依赖

| 依赖项 | 硬件 / 软件原理 |
| :--- | :--- |
| **`lock` 前缀** | x86 CPU 硬件总线锁前缀，确保多核/多任务下 `cmpxchg` 指令具有原子内存事务性 |
| **`pause` 指令** | 提示 CPU 当前处于自旋等待状态，优化流水线推测执行，大幅降低芯片功耗与总线发热 |
| **RAII 模式** | C++ 析构函数确定性释放锁，避免开发人员因逻辑遗漏产生死锁 |
