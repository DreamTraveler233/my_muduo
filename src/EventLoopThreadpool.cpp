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
 *
 * 初始化成员变量：
 * - started_   : 线程池启动状态标记（默认false）
 * - numThreads_: 工作线程数量（默认0，表示无额外IO线程）
 * - next_      : 轮询索引，用于分配任务到不同线程时的轮询策略
 *
 * 构造时仅进行基础参数设置，实际线程创建和启动由start()方法执行
 */
EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, std::string nameArg)
    : baseLoop_(baseLoop),
      name_(std::move(nameArg)),
      started_(false),
      numThreads_(0),
      next_(0)
{}

const std::string &EventLoopThreadPool::getName() const
{
    return name_;
}

bool EventLoopThreadPool::started() const
{
    return started_;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop *>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}

/**
 * @brief 以轮询方式获取下一个事件循环对象（EventLoop）
 *
 * 该函数用于在多线程环境下，从线程池中按轮询策略分配事件循环对象。
 * 当线程池中存在子事件循环时，采用轮询方式依次返回；若不存在子事件循环，
 * 则始终返回基事件循环(baseLoop_)
 *
 * @return EventLoop* 返回的事件循环对象指针，可能是基事件循环或子事件循环
 */
EventLoop *EventLoopThreadPool::getNextLoop()
{
    // 默认使用基事件循环
    EventLoop *loop = baseLoop_;

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
 * 根据配置的线程数量创建多个事件循环线程，并执行线程初始化回调函数。
 * 如果是单线程模式（numThreads_=0），则直接在基础事件循环上执行初始化回调。
 *
 * @param cb 线程初始化回调函数，参数为EventLoop*类型，在事件循环启动前执行
 */
void EventLoopThreadPool::start(const EventLoopThreadPool::ThreadInitCallback &cb)
{
    started_ = true;

    // 创建并启动多个事件循环线程
    for (int i = 0; i < numThreads_; ++i)
    {
        // 生成线程名称：基础名称+序号
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);

        // 创建线程对象并转移所有权到vector
        auto *t = new EventLoopThread(cb, buf);
        thread_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());// 底层创建一个新线程，绑定一个新的EventLoop 并返回该loop的地址
    }

    // 处理单线程模式下的初始化回调
    if (numThreads_ == 0 && cb)
    {
        // 直接在主事件循环上执行初始化
        cb(baseLoop_);
    }
}

void EventLoopThreadPool::setNumThread(int numThreads)
{
    numThreads_ = numThreads;
}

EventLoopThreadPool::~EventLoopThreadPool() = default;