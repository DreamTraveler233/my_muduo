//
// Created by shuzeyong on 2025/5/7.
//

#ifndef MY_MUDUO_THREAD_H
#define MY_MUDUO_THREAD_H

#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "CurrentThread.h"

class Thread : NonCopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, std::string name = std::string());
    ~Thread();
    void start();
    void join();
    [[nodiscard]] bool started() const;
    [[nodiscard]] pid_t getTid() const;
    [[nodiscard]] const std::string &getName() const;
    static int getNumCreated();

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_;
};

#endif//MY_MUDUO_THREAD_H
