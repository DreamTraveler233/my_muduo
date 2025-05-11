//
// Created by shuzeyong on 2025/5/7.
//

#ifndef MY_MUDUO_EVENTLOOPTHREAD_H
#define MY_MUDUO_EVENTLOOPTHREAD_H

#include "EventLoop.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "Thread.h"

class EventLoopThread : NonCopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    explicit EventLoopThread(ThreadInitCallback cb = ThreadInitCallback(),
                             std::string name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    void threadFunc();

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};

#endif//MY_MUDUO_EVENTLOOPTHREAD_H
