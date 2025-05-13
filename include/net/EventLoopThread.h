//
// Created by shuzeyong on 2025/5/7.
//

#ifndef MY_MUDUO_EVENTLOOPTHREAD_H
#define MY_MUDUO_EVENTLOOPTHREAD_H

#include "EventLoop.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "Thread.h"

namespace net
{ /**
   * @brief 事件循环线程类，实现 "one loop per thread" 模型
   *
   * 核心功能：
   * - 将 EventLoop 与 Thread 绑定，确保每个线程拥有独立的事件循环
   * - 提供线程安全的事件循环对象初始化机制
   * - 支持线程初始化回调，允许用户自定义事件循环配置
   *
   * @note 该类不可拷贝（继承 NonCopyable），需通过指针或移动语义管理实例
   */
    class EventLoopThread : NonCopyable
    {
    public:
        // 线程初始化回调类型
        using ThreadInitCallback = std::function<void(EventLoop *)>;

        explicit EventLoopThread(ThreadInitCallback cb = ThreadInitCallback(),
                                 std::string name = std::string());
        ~EventLoopThread();

        // 启动事件循环线程
        EventLoop *startLoop();

    private:
        // 线程入口函数，执行事件循环的初始化和运行
        void threadFunc();

        EventLoop *loop_;             // 事件循环对象指针（由所属线程创建和销毁）
        bool exiting_;                // 线程退出标志（true 表示请求停止事件循环）
        Thread thread_;               // 线程管理对象（实际执行 threadFunc()）
        std::mutex mutex_;            // 互斥锁（保护 loop_ 的并发访问）
        std::condition_variable cond_;// 条件变量（同步 loop_ 初始化完成事件）
        ThreadInitCallback callback_; // 线程初始化回调（配置事件循环参数等）
    };
}

#endif//MY_MUDUO_EVENTLOOPTHREAD_H
