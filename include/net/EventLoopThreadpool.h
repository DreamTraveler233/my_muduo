//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_EVENTLOOPTHREADPOOL_H
#define MY_MUDUO_EVENTLOOPTHREADPOOL_H

#include "EventLoopThread.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"

namespace net
{ /**
 * @brief 事件循环线程池类，管理一组 EventLoopThread
 *
 * 核心功能：
 * - 基于 "one loop per thread" 模型，创建多个事件循环线程
 * - 提供轮询策略分配任务到不同线程的事件循环
 * - 支持线程池初始化回调，统一配置子线程事件循环
 * - 允许动态设置线程数量（需在调用 start() 前配置）
 *
 * @note 线程池生命周期由 mainLoop_（主事件循环）管理，析构时自动回收所有子线程资源
 */
    class EventLoopThreadPool : NonCopyable
    {
    public:
        // 线程初始化回调类型
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        EventLoopThreadPool(EventLoop *baseLoop, std::string nameArg);

        // 设置线程池中子线程数量
        void setNumThread(int numThreads);
        // 启动线程池并初始化子线程
        void start(const ThreadInitCallback &cb = ThreadInitCallback());
        // 通过轮询策略获取下一个事件循环对象
        EventLoop *getNextLoop();
        // 获取所有子线程的事件循环对象
        std::vector<EventLoop *> getAllLoops();
        // 检查线程池是否已启动
        [[nodiscard]] bool started() const;
        // 获取线程池名称（用于日志标识）
        [[nodiscard]] const std::string &getName() const;

    private:
        EventLoop *mainLoop_;                                 // 主事件循环（由外部创建和管理生命周期）
        std::string name_;                                    // 线程池名称标识
        bool started_;                                        // 启动状态标记
        int numThreads_;                                      // 子线程数量（0表示仅用baseLoop_）
        int next_;                                            // 轮询索引（无锁访问，依赖外部调用同步）
        std::vector<std::unique_ptr<EventLoopThread>> thread_;// 子线程对象集合
        std::vector<EventLoop *> loops_;                      // 子线程事件循环指针集合（指向线程栈对象）
    };
}

#endif//MY_MUDUO_EVENTLOOPTHREADPOOL_H
