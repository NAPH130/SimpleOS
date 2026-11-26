#pragma once

namespace kernel::sync {

// 拿不到锁时在当前 CPU 上等待。TODO: 用原子指令取得锁并实现内存顺序。
class SpinLock {
public:
    SpinLock();
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock();
    void unlock();
};

// 拿不到锁时让当前任务进入等待。TODO: 记录锁的所有者并接入调度器。
class Mutex {
public:
    Mutex();
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    void lock();
    void unlock();
};

// TODO: 实现计数与等待队列，供银行存取款并发演示调用。
class Semaphore {
public:
    explicit Semaphore(unsigned initial_count);
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    void wait();   // P 操作，取得一个配额，或等待其他任务释放配额。
    void signal(); // V 操作，归还一个配额并按需唤醒等待者。
};

}
