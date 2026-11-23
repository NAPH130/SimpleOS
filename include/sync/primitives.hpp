#pragma once

namespace kernel::sync {

// TODO: 使用原子指令实现自旋互斥与内存顺序。
class SpinLock {
public:
    SpinLock();
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock&) = delete;

    void lock();
    void unlock();
};

// TODO: 记录锁的所有者，接入调度器的阻塞与唤醒机制。
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

    void wait();   // P
    void signal(); // V
};

}
