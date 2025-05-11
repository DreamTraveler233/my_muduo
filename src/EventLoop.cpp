//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/my_net/EventLoop.h"

// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

/**
 * @brief 创建一个eventfd事件通知文件描述符
 *
 * 该函数封装了系统调用eventfd()，用于创建一个可用于事件通知的文件描述符。
 * 创建时设置非阻塞模式(EFD_NONBLOCK)和close-on-exec标志(EFD_CLOEXEC)
 *
 * @return int 成功返回有效的文件描述符(>=0)，失败时进程直接终止不会返回
 *
 * @note 1. 初始计数器值为0
 *       2. EFD_CLOEXEC标志保证执行exec时自动关闭描述符
 *       3. EFD_NONBLOCK设置非阻塞模式
 *       4. 错误处理采用LOG_FATAL会终止程序
 */
int createEventfd()
{
    // 创建eventfd并设置非阻塞和close-on-exec标志
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    // 错误处理：当创建失败时记录致命错误并终止进程
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

/**
 * @brief EventLoop 构造函数 - 创建事件循环对象
 *
 * 核心功能：
 * 1. 初始化事件循环核心组件
 * 2. 强制单线程单实例约束
 * 3. 构建IO多路复用和线程唤醒机制
 *
 * 成员初始化说明：
 *   looping_             : 循环状态标志（初始未开始循环）
 *   quit_                : 退出标志（初始未设置退出）
 *   callingPendingFunctors_: 待执行任务队列处理状态标志
 *   threadId_            : 记录当前线程ID
 *   poller               : 创建默认的IO复用器(EPollPoller)
 *   wakeupFd_            : 创建事件通知文件描述符
 *   wakeupChannel_       : 创建用于唤醒的事件通道
 */
EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    // 打印调试日志，包含对象地址和所属线程信息
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);

    /* 线程单例检查：
     * - 通过线程局部变量 t_loopInThisThread 保证
     * - 每个线程最多只能创建一个 EventLoop 实例
     * - 重复创建将触发致命错误日志并终止程序
     */
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    /* 唤醒通道配置
     * 设置读事件回调为handleRead方法：
     * - 当wakeupFd_有数据到达时自动触发
     * - 通过enableReading注册EPOLLIN事件监听
     */
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();// 禁用所有事件监听
    wakeupChannel_->remove();    // 从poller移除
    close(wakeupFd_);            // 关闭文件描述符
    t_loopInThisThread = nullptr;
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

bool EventLoop::isInLoopThread() const
{
    return threadId_ == CurrentThread::tid();
}

/**
 * @brief EventLoop的主循环函数，负责事件循环的处理。
 *
 * 该函数启动事件循环，持续监听和处理事件，直到收到退出信号。
 * 在每次循环中，它会调用poll函数获取活跃的事件通道，并处理这些通道的事件。
 * 同时，它还会执行待处理的函数对象。
 */
void EventLoop::loop()
{
    // 标记事件循环开始
    looping_ = true;
    quit_ = false;

    // 记录事件循环启动的日志信息
    LOG_INFO("EventLoop::loop %p start Looping \n", this);

    // 主事件循环，持续运行直到收到退出信号
    while (!quit_)
    {
        // 清空活跃事件通道列表，准备接收新的事件
        activeChannels_.clear();

        // 调用poll函数获取活跃的事件通道，并记录poll的返回时间
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        // 遍历所有活跃的事件通道，处理它们的事件
        for (auto channel: activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }

        // 执行所有待处理的函数对象
        doPendingFunctors();
    }

    LOG_INFO("EventLoop::loop %p stop looping \n", this);
    looping_ = false;
}

/**
 * @brief 退出事件循环
 *
 * 该函数用于设置退出标志 `quit_` 为 true，表示事件循环需要退出。
 * 如果当前线程不是事件循环所在的线程，则调用 `wakeup()` 函数唤醒事件循环线程。
 *
 * @note 该函数不会立即停止事件循环，而是通过设置标志位和唤醒线程来通知事件循环退出。
 */
void EventLoop::quit()
{
    // 设置退出标志为 true，表示事件循环需要退出
    quit_ = true;

    // 如果当前线程不是事件循环所在的线程，则唤醒事件循环线程
    if (!isInLoopThread())
    {
        wakeup();
    }
}

Timestamp EventLoop::pollReturnTime() const
{
    return pollReturnTime_;
}

/**
 * 在事件循环中执行一个任务
 *
 * @param cb 一个 Functor 类型的任务，代表一段可执行的代码
 *
 * 此函数用于将一个任务提交到事件循环线程中执行。如果调用此函数的线程就是事件循环线程，
 * 则直接执行该任务；否则，将任务加入到事件循环的队列中，以供事件循环线程稍后执行。
 */
void EventLoop::runInLoop(const EventLoop::Functor &cb)
{
    // 判断当前线程是否为事件循环线程
    if (isInLoopThread())
    {
        // 如果是事件循环线程，则直接执行任务
        cb();
    }
    else
    {
        // 如果不是事件循环线程，则将任务加入队列，以供事件循环线程执行
        queueInLoop(cb);
    }
}

/**
 * @brief 将回调函数添加到事件循环的待执行队列中。
 *
 * 该函数用于将回调函数 `cb` 添加到事件循环的待执行队列 `pendingFunctors_` 中。
 * 如果当前线程不是事件循环所在的线程，或者正在执行待执行队列中的回调函数，
 * 则调用 `wakeup()` 函数唤醒事件循环线程。
 *
 * @param cb 要添加到待执行队列的回调函数，类型为 `EventLoop::Functor`。
 *            该参数应为可调用对象，通常为函数指针或lambda表达式。
 */
void EventLoop::queueInLoop(const EventLoop::Functor &cb)
{
    // 临界区开始：通过互斥锁保护共享资源 pendingFunctors_
    {
        // 自动加锁，RAII方式保证线程安全。离开作用域时自动解锁
        std::unique_lock<std::mutex> lock(mutex_);

        // 将回调函数添加到待执行队列尾部
        // 使用emplace_back避免不必要的拷贝操作
        pendingFunctors_.emplace_back(cb);
    }
    // 临界区结束：锁在此处自动释放

    /*
     * 唤醒条件判断：
     * 1. 当前非事件循环线程操作时需要唤醒
     * 2. 当前正在执行回调队列时，新任务需要立即唤醒处理
     * 通过wakeup()向事件循环发送通知，确保及时处理新加入的回调
     */
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        // 调用底层唤醒机制（如eventfd写入）
        wakeup();
    }
}

/**
 * @brief 唤醒事件循环
 *
 * 该函数通过向 `wakeupFd_` 文件描述符写入一个 64 位的整数值（1）来唤醒事件循环。
 * 通常用于在事件循环处于阻塞状态时，强制其立即处理新的事件或任务。
 *
 * @note 如果写入的字节数不等于预期的 8 字节，将记录错误日志。
 */
void EventLoop::wakeup() const
{
    uint64_t one = 1;
    // 向 wakeupFd_ 写入一个 64 位的整数值（1）
    ssize_t n = write(wakeupFd_, &one, sizeof(one));

    // 检查写入的字节数是否正确，如果不正确则记录错误日志
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() write %zd bytes instead of 8", n);
    }
}

/**
 * @brief 处理事件循环中的读事件。
 *
 * 该函数用于从 `wakeupFd_` 文件描述符中读取数据，通常用于唤醒事件循环。
 * 读取的数据是一个 64 位无符号整数，用于确认唤醒事件的发生。
 * 如果读取的字节数不等于预期的 8 字节，则记录错误日志。
 */
void EventLoop::handleRead() const
{
    uint64_t one = 1;
    // 从 wakeupFd_ 中读取数据，预期读取 8 字节
    ssize_t n = read(wakeupFd_, &one, sizeof(one));

    // 检查读取的字节数是否符合预期，如果不符合则记录错误日志
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %zd bytes instead of 8", n);
    }
}

/**
 * @brief 执行事件循环中积压的任务函数
 *
 * 本函数通过交换容器方式批量获取待执行任务，在保证线程安全的前提下最小化临界区范围。
 * 执行期间通过callingPendingFunctors_标志位防止重入，任务执行在锁外进行以避免死锁。
 *
 * @note 此函数无参数且无返回值，属于EventLoop类的内部状态管理方法
 */
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;// 原子标记进入任务处理状态

    // 临界区开始：通过RAII锁保护任务队列交换操作
    // 交换操作保证后续的任务执行过程不需要持有锁，提升并发性能
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    // 临界区结束：unique_lock离开作用域自动释放锁

    // 批量执行所有已转移的任务函数
    // 注意：任务可能产生新的pendingFunctors_，这些新任务将在下次循环处理
    for (const Functor &functor: functors)
    {
        functor();
    }

    // 必须最后更新状态标志，保证可见性（假设存在内存屏障机制）
    callingPendingFunctors_ = false;
}