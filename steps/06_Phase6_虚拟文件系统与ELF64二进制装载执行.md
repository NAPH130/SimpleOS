# 06 - Phase 6：虚拟文件系统与 ELF64 二进制装载执行

---

## 1. 详细的具体工程开发实施计划

### 1.1 阶段目标与交付物
构建操作系统虚拟文件系统（VFS）抽象模型，实现只读内存文件系统（RamFS）或磁盘目录解析，编写通用的 **ELF64 二进制文件头与程序头（Program Header）装载器**，并实现一个支持键盘交互的用户命令行终端（Shell）。
- **交付源文件**：
  - `fs/vfs.cpp` & `vfs.hpp`：VFS 统一文件操作抽象接口（`VNode`, `FileDescriptor`）
  - `fs/ramfs.cpp` & `ramfs.hpp`：内嵌内存只读文件系统驱动
  - `loader/elf.cpp` & `elf.hpp`：标准 ELF64 二进制格式解析与段加载映射器
  - `user/sh.cpp`：交互式用户命令行 Shell 程序（支持 `ls`, `cat`, `ps`, `bank_test`, `gui`, `reboot`）
  - `apps/hello_elf.cpp`：作为外部独立 ELF 目标文件编译并打包进镜像的测试程序

### 1.2 详细开发排期与子任务
1. **子任务 6.1：设计 VFS 虚拟文件系统抽象层 (`fs/vfs.cpp`)**
   - 定义面向对象节点类 `VNode`：支持 `read()`, `write()`, `readdir()`, `stat()` 等纯虚函数；
   - 维护进程的文件描述符表 `FileDescriptorTable`（0: stdin, 1: stdout, 2: stderr）。
2. **子任务 6.2：实现只读内存文件系统 RamFS (`fs/ramfs.cpp`)**
   - 使用链接器符号（`extern char _binary_app_start[]`）将独立编译的应用程序作为二进制资源直接嵌入内核只读段；
   - 在内存中构建目录树，支持通过文件名按路径检索文件（如 `/hello.elf`）。
3. **子任务 6.3：编写 ELF64 目标文件装载器 (`loader/elf.cpp`)**
   - 验证 ELF Magic（`\x7fELF`），验证 Class=2（64-bit），Machine=0x3E（AMD x86-64）；
   - 定位 `e_phoff`（程序头表偏移）与 `e_phnum`（表项数量）；
   - 遍历 `Elf64_Phdr`，筛选 `p_type == PT_LOAD`（可装载段）；
   - 根据段的虚拟基地址 `p_vaddr` 与内存尺寸 `p_memsz`，在进程页表中分配物理页并建立映射；
   - 将二进制代码/数据精确拷贝至对应物理页，若 `p_memsz > p_filesz`，将未初始化的 BSS 区域自动补零；
   - 提取 `e_entry` 入口地址写入进程上下文中，启动新进程。
4. **子任务 6.4：编写交互式命令行终端 Shell (`user/sh.cpp`)**
   - 编写键盘字符输入处理与退格支持；
   - 显示命令提示符 `SimpleOS# `；
   - 内置命令解析：
     - `help`：打印命令帮助；
     - `ls`：列出文件系统所有可执行文件与尺寸；
     - `ps`：显示当前系统的任务进程状态与内存用量；
     - `cat <file>`：打印文件内容；
     - `./<app.elf>`：动态加载并运行指定的 ELF 二进制程序；
     - `bank_test`：一键触发阶段 5 的银行并发测试；
     - `gui`：触发进入图形桌面。

---

## 2. 我（用户）需要干什么

### 2.1 编译与执行命令
在工程根目录下执行构建并运行：

```bash
make run
```

### 2.2 验证结果与观察点
- 系统启动后自动进入交互式命令行：
  ```
  [SimpleOS] RamFS mounted at '/' successfully.
  [SimpleOS] Launching interactive user shell...
  
  SimpleOS# help
  Supported commands:
    help          - Display this help message
    ls            - List files in current directory
    cat <file>    - Display file contents
    ps            - Show process status
    bank_test     - Run Bank P-V Concurrency benchmark
    gui           - Launch Graphical User Interface
    ./<elf_file>  - Execute independent ELF binary
  
  SimpleOS# ls
  -rwxr-xr-x    1 root  4096  hello.elf
  -rwxr-xr-x    1 root  8192  gui_app.elf
  
  SimpleOS# ./hello.elf
  [ELF Loader] Checking ELF Magic: 0x7F 'E' 'L' 'F' [OK]
  [ELF Loader] Mapping Segment: VAddr 0x400000 -> Allocating Physical Pages [OK]
  [ELF Loader] Entry Point Address: 0x4000e0
  Hello, World from an independently compiled ELF binary running in User Space!
  ```
- **核心判定**：能够在 Shell 中流畅敲击键盘执行命令，并能通过自研解析器正确加载、映射并运行外部标准 ELF64 文件。

---

## 3. 实现原理与方式

### 3.1 ELF64 规范与加载映射原理
- **ELF（Executable and Linkable Format）结构**：
  ```
  +──────────────────────────────────────────────────+
  |              ELF 64-bit Header (64 bytes)        |
  |  - Magic: \x7fELF                                |
  |  - Type: EXEC / ET_DYN                           |
  |  - Entry Point: e_entry                          |
  |  - Program Header Offset: e_phoff                |
  +──────────────────────────────────────────────────+
  |             Program Header Table (Phdr)          |
  |  - Segment 1: PT_LOAD (Code, RX, p_vaddr, memsz) |
  |  - Segment 2: PT_LOAD (Data, RW, p_vaddr, memsz) |
  +──────────────────────────────────────────────────+
  |                  Code / Data Segments            |
  +──────────────────────────────────────────────────+
  ```
- **虚拟地址按需映射**：链接器在编译用户程序时预设了基地址（如 `0x400000`）。内核装载器的核心工作就是在新进程的私有 PML4 页表中，为 `0x400000 ~ 0x400000 + p_memsz` 的每一个 4KB 页分配物理页帧并建立页表映射，随后将二进制镜像中的数据拷贝至对应的物理内存，从而让用户代码像在标准 Linux 下一样正常寻址运行。

---

## 4. 实现工具与依赖

| 依赖项 | 规范与工具说明 |
| :--- | :--- |
| **标准 ELF64 Header 结构定义** | 自研 `loader/elf.hpp`，精确匹配 System V Application Binary Interface AMD64 规范 |
| **`objcopy`** | 宿主 GNU Binutils 工具，用于将独立编译的应用程序打包嵌入内核数据段 |
