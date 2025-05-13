//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_CALLBACKS_H
#define MY_MUDUO_CALLBACKS_H

#include "SysHeadFile.h"

namespace net
{
    class Buffer;
    class TcpConnection;
    class Timestamp;

    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

    using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;
    using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;
}

#endif//MY_MUDUO_CALLBACKS_H
