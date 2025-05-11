//
// Created by shuzeyong on 2025/5/7.
//

#include "../include/my_net/Thread.h"

std::atomic_int Thread::threadNum_ = 0;

Thread::Thread(Thread::ThreadFunc func, std::string name)
    : started_(false),
      joined_(false),
      tid_(0),
      threadFunc_(std::move(func)),
      threadName_(std::move(name))
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        thread_->detach();
    }
}

/**
 * @brief 启动线程并执行绑定任务函数
 *
 * 功能说明：
 * 1. 初始化线程状态标志
 * 2. 使用信号量同步确保线程ID正确获取
 * 3. 创建实际执行线程并分离控制权
 *
 * 注意：
 * - 使用信号量确保子线程完成tid设置后才继续主线程
 * - func_将在新线程上下文中执行
 * - 存在潜在风险：局部信号量sem可能在线程运行时已超出作用域（需确保线程生命周期管理）
 */
void Thread::start()
{
    if (started_) {
        return; // 避免重复启动
    }

    // 初始化无名信号量用于线程间同步
    sem_t sem;
    sem_init(&sem, false, 0);

    /* 创建线程对象并转移所有权到shared_ptr：
     * 1. lambda捕获列表使用引用方式捕获局部信号量（需注意生命周期）
     * 2. 在新线程中：
     *    - 获取并存储系统级线程ID
     *    - 释放信号量通知主线程
     *    - 执行用户注册的任务函数
     */
    thread_ = std::make_shared<std::thread>([&]() {
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        threadFunc_();
    });

    // 主线程等待子线程完成tid设置
    sem_wait(&sem);
    started_ = true;
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

/**
 * @brief 设置线程的默认名称
 *
 * 当线程对象没有指定名称时，自动生成默认名称。命名规则为"Thread"前缀
 * 加上递增的线程编号。该函数会修改成员变量name_和静态计数器numCreated_
 *
 * @note 该函数没有参数和返回值，通过修改类成员变量实现功能：
 *   - numCreated_ 是静态成员变量，记录已创建线程总数
 *   - name_ 是成员变量，存储线程名称
 */
void Thread::setDefaultName()
{
    // 原子递增线程计数器并获取当前序号
    int num = ++threadNum_;

    // 仅当名称为空时生成默认名称
    if (threadName_.empty())
    {
        // 生成"ThreadX"格式的默认名称，X为递增序号
        // 使用固定大小缓冲区防止内存越界
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread%d", num);
        threadName_ = buf;
    }
}
bool Thread::started() const { return started_; }
pid_t Thread::getTid() const { return tid_; }
const std::string &Thread::getName() const { return threadName_; }
int Thread::getNumCreated() { return threadNum_; }
