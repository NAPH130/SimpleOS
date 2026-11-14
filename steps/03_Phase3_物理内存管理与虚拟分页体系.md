# 03 - Phase 3：物理内存管理与虚拟分页体系

---

## 1. 详细的具体工程开发实施计划

### 1.1 阶段目标与交付物
构建完整的物理内存分配体系与现代虚拟内存管理机制。实现 **伙伴系统（Buddy System，进阶加分项）** 物理页分配器、**Slab/Kmalloc** 内核小对象缓存池，以及 **x86_64 4级分页（CR3 管理）** 虚拟地址空间强隔离机制。
- **交付源文件**：
  - `mm/frame.cpp` & `frame.hpp`：物理内存探测与基础物理页帧管理器
  - `mm/buddy.cpp` & `buddy.hpp`：高阶伙伴系统物理内存分配器（进阶加分核心代码）
  - `mm/slab.cpp` & `slab.hpp`：内核堆小对象缓存池（重载 `operator new / delete`）
  - `mm/pagetable.cpp` & `pagetable.hpp`：x86_64 四级页表管理与虚拟地址空间封装类（`MemorySet`）
  - `mm/mem_test.cpp`：物理/虚拟内存与非法地址防御测试用例

### 1.2 详细开发排期与子任务
1. **子任务 3.1：物理内存探测与边界定义 (`mm/frame.cpp`)**
   - 解析 MBR 传递的 BIOS E820 内存映射表，计算实际可用物理内存上限（如 128MB）；
   - 将物理内存按 4KB 划分页帧，排除内核镜像自身占用的物理区间。
2. **子任务 3.2：实现伙伴系统物理页分配器 (`mm/buddy.cpp`)**
   - 建立 order 0（1页=4KB）到 order 10（1024页=4MB）的 11 个空闲块双向链表数组；
   - 实现 `alloc_pages(size_t order)`：查找满足条件的最小阶块，若无则递归将高阶块一分为二；
   - 实现 `free_pages(void* paddr, size_t order)`：释放物理页，通过位运算 `buddy_paddr = paddr ^ (1 << (order + 12))` 计算伙伴地址，若伙伴空闲则自动递归合并。
3. **子任务 3.3：实现 Slab 内核小对象分配器 (`mm/slab.cpp`)**
   - 预设 32B, 64B, 128B, 256B, 512B, 1024B, 2048B 的固定对象缓存池；
   - 提供 `kmalloc(size)` 与 `kfree(ptr)`；
   - 重载全局 C++ `operator new` 与 `operator delete`，让内核全面拥有动态面向对象创建能力。
4. **子任务 3.4：构建 x86_64 4级分页体系 (`mm/pagetable.cpp`)**
   - 封装 PML4（Page Map Level 4）、PDPT（Page Directory Pointer Table）、PD（Page Directory）、PT（Page Table）；
   - 实现虚拟地址到物理地址的映射函数：`map_page(vaddr, paddr, flags)` 与 `unmap_page(vaddr)`；
   - 严格配置页表标志位：`PTE_P`（存在位）、`PTE_W`（可写）、`PTE_U`（用户态可访问）、`PTE_NX`（不可执行）；
   - 实现内核恒等映射（Identity Mapping）与用户空间隔离映射，刷新 TLB（通过 `invlpg` 或重新载入 `CR3`）。
5. **子任务 3.5：编写内存测试与非法越界防御测试 (`mm/mem_test.cpp`)**
   - 单元测试伙伴系统高频分配与碎片合并正确性；
   - 触发用户态非法访问内核高地址空间的测试，验证 CPU 立即抛出 14 号 Page Fault 异常并被内核优雅捕获。

---

## 2. 我（用户）需要干什么

### 2.1 编译与执行命令
在工程根目录下执行构建并运行：

```bash
make run
```

### 2.2 验证结果与观察点
- 观察控制台输出：
  ```
  [SimpleOS] Physical Memory Detected: 128 MB available.
  [SimpleOS] Buddy Allocator Initialized: order 0..10 ready.
  [SimpleOS] Slab Cache Initialized (32B..2048B objects).
  [SimpleOS] Testing Buddy Allocation:
    Allocated 16 pages (order 4) at 0x200000 -> SUCCESS
    Freed 16 pages at 0x200000 -> Buddies merged cleanly!
  [SimpleOS] Testing Kmalloc & C++ new/delete:
    Kmalloc(128) returned valid heap ptr.
    Object constructed dynamically via new!
  [SimpleOS] Activating Process CR3 4-Level Paging:
    Kernel Space (0xFFFF800000000000+): Supervisor only (U=0).
    User Space   (0x0000000000400000+): User Accessible (U=1).
  [SimpleOS] Security Test: Simulating user rogue access to 0xFFFFFFFF80000000...
  [SimpleOS] [PageFault] Caught Vector 14! Protection Violation! User isolated safely.
  ```
- **核心判定**：动态分配无内存泄漏，伙伴块能正常自动合并，页表权限能 100% 抵御非法越界。

---

## 3. 实现原理与方式

### 3.1 伙伴系统（Buddy System）数学原理
- **外部碎片消除机制**：若任意释放和申请不同大小的内存块，会产生大量无法利用的小缝隙（外部碎片）。伙伴算法将所有块强制对齐为 $2^k \times 4\text{KB}$ 大小。
- **快速寻址伙伴的位运算**：
  $$\text{BuddyPaddr} = \text{Paddr} \oplus (2^{\text{order} + 12})$$
  仅需一次异或运算，即可在 $O(1)$ 时间复杂度内求出其伙伴块的物理地址，判断其空闲状态并合并为 $2^{\text{order}+1}$ 阶大块，算法效率极高。

### 3.2 x86_64 四级分页硬件寻址过程
- 虚拟地址（48位 Canonical 地址）被硬件 CPU 划分为 5 个字段：
  ```
  | 47...39 (9位) | 38...30 (9位) | 29...21 (9位) | 20...12 (9位) | 11...0 (12位) |
  |    PML4 索引   |   PDPT 索引   |    PD 索引    |    PT 索引    |   页内偏移    |
  ```
- CPU 寻址时，首先从 `CR3` 读取 PML4 物理基地址，通过 PML4 索引找到对应的 PDPT；层层索引，最终在 PT 中找到目标物理页的基地址，加上 12 位页内偏移形成物理内存地址。
- **硬件特权检查**：四级页表项中只要有任意一级的 `U/S` 位为 0，Ring 3 代码便绝对无法读取或执行该页，这是实现现代操作系统安全沙箱的底层硬件基石。

---

## 4. 实现工具与依赖

| 依赖项 | 作用说明 |
| :--- | :--- |
| **`CR3` 寄存器** | 控制寄存器，存放当前进程的根页表（PML4）物理地址，切换进程地址空间的核心凭证 |
| **`invlpg` 汇编指令** | 刷新 CPU 内部快表（TLB），在页表映射发生改变时使 CPU 缓存立即失效 |
| **双向循环侵入式链表** | 自研 `lib/intrusive_list.hpp`，伙伴系统空闲块队列不依赖任何动态内存分配 |
