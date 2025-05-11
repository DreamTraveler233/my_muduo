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

    LOG_INFO("%s : %p start Looping \n", __FUNCTION__, this);

    // 主事件循环，持续运行直到收到退出信号
    while (!quit_)
    {
        activeChannels_.clear();// 清空活跃事件通道列表，准备接收新的事件

        // 核心阻塞调用：通过Poller监听I/O事件，最长阻塞kPollTimeMs(10秒)
        // 返回值pollReturnTime_用于定时器系统的时间补偿
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        // 事件处理阶段：严格顺序执行所有活跃通道的回调
        // 1. 此处不允许添加/删除Channel，需通过queueInLoop延迟操作
        // 2. handleEvent可能修改Channel的监听事件状态
        for (Channel *channel: activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }

        // 异步任务执行阶段：执行所有通过queueInLoop提交的异步任务（Functor）
        // 潜在风险：长时间任务会阻塞事件循环
        doPendingFunctors();
    }

    LOG_INFO("%s : %p stop looping \n", __FUNCTION__, this);
    looping_ = false;
}

/**
 * @brief 退出事件循环
 *
 * 该函数用于设置退出标志 `quit_` 为 true，表示事件循环需要退出。
 * 如果当前线程不是事件循环所在的线程，则调用 `wakeup()` 函数唤醒事件循环线程。
 *
 * 场景一：事件循环线程自身调用 quit()
 *      此时，线程正在运行事件循环（即在 loop() 的 while (!quit_) 循环中）。
 *      设置 quit_ = true 后，事件循环会在当前循环迭代结束后自然退出，无需额外唤醒。
 *      示例：事件循环线程处理到某个条件（如接收到关闭信号），主动调用 quit() 退出。
 * 场景二：其他线程调用 quit()
 *      例如，主线程需要关闭工作线程的事件循环。
 *      事件循环线程可能正阻塞在 poll() 中等待I/O事件，无法立即检测到 quit_ 的变化。
 *      示例：主线程通过 std::thread::join() 等待工作线程退出前，调用工作线程的 EventLoop::quit()。
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


/* 异步任务处理联动过程：

     外部线程调用 queueInLoop(cb)
                ↓
             wakeup()
                ↓
  向 wakeupFd_ 写入 8 字节数据（eventfd）
                ↓
    epoll 检测到 wakeupChannel_ 可读
                ↓
         触发 handleRead()
                ↓
       清空 eventfd 的数据（read）
                ↓
    进入 doPendingFunctors() 执行 cb

 */

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
     * 1. 若调用者为非事件循环线程，必须唤醒事件循环线程（可能阻塞在 poll 中），确保新任务被及时处理。
     * 2. 若事件循环线程正在执行 doPendingFunctors()（即处理任务队列），
     *    新任务需立即唤醒以加入当前批处理周期，避免任务延迟一轮循环。
     * 通过wakeup()向事件循环发送通知，确保及时处理新加入的回调
     */
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        // 调用底层唤醒机制（如eventfd写入）
        wakeup();
    }
}

/**
 * @brief 跨线程唤醒事件循环
 * 写入操作会触发 wakeupFd_ 的可读事件，强制中断 poll/epoll_wait 的阻塞状态
 */
void EventLoop::wakeup() const
{
    uint64_t one = 1;
    // 向 wakeupFd_ 写入一个 64 位的整数值（1）
    ssize_t n = write(wakeupFd_, &one, sizeof(one));

    // 检查写入的字节数是否正确，如果不正确则记录错误日志
    if (n != sizeof(one))
    {
        LOG_ERROR("%s write %zd bytes instead of 8", __FUNCTION__, n);
    }
}

/**
 * @brief 清空eventfd状态避免重复触发
 * 1. 消费 wakeupFd_ 的计数器值，重置 eventfd 状态，避免持续触发可读事件
 * 2. 作为唤醒事件的实际处理入口，触发后续异步任务队列的执行（doPendingFunctors）
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
 * @brief 批量执行异步任务队列的核心调度逻辑
 *
 * - 使用swap原子化获取任务队列，最小化临界区提升并发性能
 * - 分离任务获取与执行阶段，避免执行时持有锁引发死锁风险
 * - 标志位管理确保任务执行期间新任务进入下一轮处理周期
 * - 允许任务嵌套提交，维持事件循环的级联响应能力
 */
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;// 状态屏障：阻塞期间新任务入队不处理

    {// 原子交换操作：毫秒级锁定实现无锁化执行
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    // 批量执行所有已转移的任务函数
    // 注意：任务可能产生新的pendingFunctors_，这些新任务将在下次循环处理
    for (const Functor &functor: functors)
    {
        functor();
    }

    // 必须最后更新状态标志，保证可见性（假设存在内存屏障机制）
    callingPendingFunctors_ = false;
}
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::isInLoopThread() const
{
    return threadId_ == CurrentThread::tid();
}