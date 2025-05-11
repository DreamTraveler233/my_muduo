//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_EVENTLOOPTHREADPOOL_H
#define MY_MUDUO_EVENTLOOPTHREADPOOL_H

#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "EventLoopThread.h"


class EventLoopThreadPool : NonCopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    EventLoopThreadPool(EventLoop *baseLoop, std::string nameArg);
    ~EventLoopThreadPool();
    void setNumThread(int numThreads);
    void start(const ThreadInitCallback &cb = ThreadInitCallback());
    EventLoop *getNextLoop();
    std::vector<EventLoop *> getAllLoops();
    [[nodiscard]] bool started() const;
    [[nodiscard]] const std::string &getName() const;

private:
    EventLoop *baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> thread_;
    std::vector<EventLoop *> loops_;
};

#endif//MY_MUDUO_EVENTLOOPTHREADPOOL_H
