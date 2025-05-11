//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_EVENTLOOP_H
#define MY_MUDUO_EVENTLOOP_H

#include "Channel.h"
#include "CurrentThread.h"
#include "NonCopyable.h"
#include "Poller.h"
#include "SysHeadFile.h"
#include "Timestamp.h"

class Poller;
class Channel;

/**
 * @class EventLoop
 * @brief 事件循环核心类，负责事件驱动的核心调度机制
 *
 * 本类实现Reactor模式的核心事件循环，负责以下功能：
 * 1. 通过Poller进行I/O事件监听
 * 2. 调度定时器事件
 * 3. 处理跨线程异步任务队列
 * 4. 管理Channel的生命周期和事件监听状态
 * 采用one loop per thread设计，每个线程最多拥有一个EventLoop实例
 * 通过wakeup通道实现跨线程唤醒机制，确保任务处理的及时性
 */
class EventLoop : NonCopyable
{
public:
    using Functor = std::function<void()>;// 异步任务函数类型定义

    EventLoop();
    ~EventLoop();
    // 启动事件循环，进入无限事件处理流程
    void loop();
    // 请求退出事件循环
    void quit();

    // 在当前事件循环线程立即执行任务
    void runInLoop(const Functor &cb);
    // 将任务放入异步队列等待执行
    void queueInLoop(const Functor &cb);

    // 唤醒事件循环（跨线程安全）
    void wakeup() const;

    // 更新通道监听状态
    void updateChannel(Channel *channel);
    // 移除通道监听
    void removeChannel(Channel *channel);

    // 检查通道是否已被注册
    bool hasChannel(Channel *channel);
    // 获取最近一次poll操作的返回时间戳
    [[nodiscard]] Timestamp pollReturnTime() const;
    // 验证当前线程是否属于本事件循环
    [[nodiscard]] bool isInLoopThread() const;

private:
    // 处理wakeupFd_的可读事件（唤醒事件）
    void handleRead() const;
    // 执行异步任务队列
    void doPendingFunctors();

    using ChannelList = std::vector<Channel *>;// 定义事件通道列表类型

    std::atomic_bool looping_;// 事件循环运行状态标志
    std::atomic_bool quit_;   // 退出请求标志

    const pid_t threadId_;// 所属线程ID（用于线程安全检查）

    Timestamp pollReturnTime_;     // 最近一次poll调用的时间戳
    std::unique_ptr<Poller> poller_;// 多路复用器（Epoll/Poll的抽象）

    int wakeupFd_;                          // 唤醒文件描述符，用于跨线程唤醒事件循环
    std::unique_ptr<Channel> wakeupChannel_;// 唤醒事件通道，用于监听唤醒事件

    ChannelList activeChannels_;// 当前活跃的事件通道列表

    std::atomic_bool callingPendingFunctors_;// 标识是否正在执行待处理的任务函数
    std::vector<Functor> pendingFunctors_;   // 待处理的任务函数队列
    std::mutex mutex_;                       // 互斥锁，用于保护待处理任务队列的线程安全
};

#endif//MY_MUDUO_EVENTLOOP_H
