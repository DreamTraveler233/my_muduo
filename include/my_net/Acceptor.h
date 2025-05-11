//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_ACCEPTOR_H
#define MY_MUDUO_ACCEPTOR_H

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "Socket.h"
#include "SysHeadFile.h"

class Acceptor : NonCopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;
    Acceptor(EventLoop *loop, InetAddress listenAddr, bool reuseport);
    ~Acceptor();
    void setNewConnectionCallback(NewConnectionCallback cb);
    bool getListenning();
    void listen();

private:
    void handleRead();

    EventLoop *loop_;// baseLoop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};

#endif//MY_MUDUO_ACCEPTOR_H
