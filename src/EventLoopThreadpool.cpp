//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/my_net/EventLoopThreadpool.h"

/**
 * @brief EventLoop线程池构造函数
 *
 * @param baseLoop 主事件循环指针，作为线程池的基准事件循环。当线程池中无其他线程时，
 *                 所有IO事件将由该事件循环处理
 * @param nameArg 线程池名称标识，用于日志记录和调试时区分不同的线程池
 * @note 构造时仅进行基础参数设置，实际线程创建和启动由start()方法执行
 */
EventLoopThreadPool::EventLoopThreadPool(EventLoop *mainLoop_, std::string nameArg)
    : mainLoop_(mainLoop_),// 由用户创建的mainLoop_
      name_(std::move(nameArg)),
      started_(false),
      numThreads_(0),
      next_(0)
{}

/**
 * @brief 获取所有事件循环对象
 *
 * 该函数返回当前线程池中的所有事件循环对象。如果线程池中没有额外的事件循环对象，
 * 则返回一个仅包含主事件循环对象的向量。
 *
 * @return std::vector<EventLoop *> 返回一个包含所有事件循环对象的向量。
 *         如果线程池中没有额外的事件循环对象，则返回一个仅包含主事件循环对象的向量。
 */
std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    // 检查线程池中是否有额外的事件循环对象
    if (loops_.empty())
    {
        // 如果没有额外的事件循环对象，返回一个仅包含主事件循环对象的向量
        return std::vector<EventLoop *>(1, mainLoop_);
    }
    else
    {
        // 如果有额外的事件循环对象，直接返回这些事件循环对象的向量
        return loops_;
    }
}

/**
 * @brief 以轮询方式获取下一个事件循环对象（EventLoop）
 *
 * 该函数用于在多线程环境下，从线程池中按轮询策略分配事件循环对象。
 * 当线程池中存在子事件循环时，采用轮询方式依次返回；若不存在子事件循环，
 * 则始终返回基事件循环(mainLoop_)
 *
 * @return EventLoop* 返回的事件循环对象指针，可能是基事件循环或子事件循环
 */
EventLoop *EventLoopThreadPool::getNextLoop()
{
    // 默认使用基事件循环
    EventLoop *loop = mainLoop_;

    // 当存在子事件循环时执行轮询逻辑
    if (!loops_.empty())
    {
        // 采用轮询算法获取下一个事件循环
        loop = loops_[next_];
        ++next_;

        // 轮询索引回滚机制：当超过容器大小时重置为0
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}

/**
 * @brief 启动事件循环线程池
 *
 * 功能流程：
 * 1. 检查线程池是否已启动，防止重复初始化
 * 2. 根据配置的线程数量创建子线程
 * 3. 收集所有子线程的事件循环对象
 * 4. 处理单线程模式下的初始化回调
 *
 * @param cb 线程初始化回调函数，在子线程事件循环启动前执行
 *
 * @note 关键实现细节：
 * - 线程名称生成：使用基础名称+序号格式（需注意缓冲区溢出风险）
 * - 资源管理：通过unique_ptr管理线程对象，自动释放资源
 * - 单线程模式：当numThreads_=0时，直接在mainLoop_上执行回调
 */
void EventLoopThreadPool::start(const EventLoopThreadPool::ThreadInitCallback &cb)
{
    if (started_)
    {
        return;// 防止重复启动（线程安全屏障）
    }

    // 创建并启动多个事件循环线程
    for (int i = 0; i < numThreads_; ++i)
    {
        // 生成线程名称：基础名称+序号
        char buf[name_.size() + 32];// 固定大小缓冲区，需确保name_长度合理
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);

        // 创建线程对象并转移所有权到vector（自动内存管理）
        auto *t = new EventLoopThread(cb, buf);
        thread_.push_back(std::unique_ptr<EventLoopThread>(t));

        // 启动线程并获取事件循环指针（阻塞直到线程初始化完成）
        /* startLoop()底层实现细节：
         * -创建一个新线程
         * -调用线程函数
         * -线程函数创建一个EventLoop，并启动EventLoop
         * -返回EventLoop对象
         */
        loops_.push_back(t->startLoop());
    }

    // 处理单线程模式（numThreads_=0）的特殊情况
    if (numThreads_ == 0 && cb)
    {
        // 直接在主事件循环执行初始化（确保回调在IO线程执行）
        cb(mainLoop_);
    }

    started_ = true;// 必须在所有资源初始化完成后设置
}

void EventLoopThreadPool::setNumThread(int numThreads) { numThreads_ = numThreads; }
const std::string &EventLoopThreadPool::getName() const { return name_; }
bool EventLoopThreadPool::started() const { return started_; }