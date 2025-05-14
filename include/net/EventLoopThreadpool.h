//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_EVENTLOOPTHREADPOOL_H
#define MY_MUDUO_EVENTLOOPTHREADPOOL_H

#include "EventLoopThread.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"

namespace net
{
    /**
     * @class EventLoopThreadPool
     * @brief 事件循环线程池类，管理一组 [EventLoopThread]
     *
     * 核心功能：
     * - 基于 "one loop per thread" 模型，创建多个事件循环线程
     * - 提供轮询策略分配任务到不同线程的事件循环
     * - 支持线程池初始化回调，统一配置子线程事件循环
     * - 允许动态设置线程数量（需在调用 [start()] 前配置）
     *
     * @note 线程池生命周期由 [mainLoop_]（主事件循环）管理，析构时自动回收所有子线程资源
     */
    class EventLoopThreadPool : NonCopyable
    {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;//!< 线程初始化回调类型

        /**
         * @brief 构造函数，初始化事件循环线程池
         * @param baseLoop 主事件循环对象
         * @param nameArg 线程池名称标识
         */
        EventLoopThreadPool(EventLoop *baseLoop, std::string nameArg);

        /**
         * @brief 设置线程池中子线程数量
         * @param numThreads 子线程数量
         */
        void setNumThread(int numThreads);

        /**
         * @brief 启动线程池并初始化子线程
         * @param cb 线程初始化回调函数，用于配置子线程事件循环
         */
        void start(const ThreadInitCallback &cb = ThreadInitCallback());

        /**
         * @brief 通过轮询策略获取下一个事件循环对象
         * @return 返回下一个事件循环对象指针
         */
        EventLoop *getNextLoop();

        /**
         * @brief 获取所有子线程的事件循环对象
         * @return 返回所有子线程的事件循环对象列表
         */
        std::vector<EventLoop *> getAllLoops();

        /**
         * @brief 检查线程池是否已启动
         * @return 如果线程池已启动，返回 true；否则返回 false
         */
        [[nodiscard]] bool started() const;

        /**
         * @brief 获取线程池名称（用于日志标识）
         * @return 返回线程池名称
         */
        [[nodiscard]] const std::string &getName() const;

    private:
        EventLoop *mainLoop_;                                 //!< 主事件循环（由外部创建和管理生命周期）
        std::string name_;                                    //!< 线程池名称标识
        bool started_;                                        //!< 启动状态标记
        int numThreads_;                                      //!< 子线程数量（0 表示仅用 [mainLoop_]）
        int next_;                                            //!< 轮询索引（无锁访问，依赖外部调用同步）
        std::vector<std::unique_ptr<EventLoopThread>> thread_;//!< 子线程对象集合
        std::vector<EventLoop *> loops_;                      //!< 子线程事件循环指针集合（指向线程栈对象）
    };
}// namespace net

#endif//MY_MUDUO_EVENTLOOPTHREADPOOL_H
