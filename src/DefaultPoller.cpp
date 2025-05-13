//
// Created by shuzeyong on 2025/5/1.
//
#include "../include/net/EPollPoller.h"
#include "../include/net/Poller.h"

using namespace net;

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (getenv("MUDUO_USE_POLL"))
    {
        // 生成poll实例
        return nullptr;
    }
    else
    {
        // 生成epoll实例
        return new EPollPoller(loop);
    }
}