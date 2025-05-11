//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_SOCKET_H
#define MY_MUDUO_SOCKET_H

#include "InetAddress.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"

class Socket : NonCopyable
{
public:
    explicit Socket(int sockfd);
    ~Socket();
    [[nodiscard]] int getFd() const;
    void bind(const InetAddress &localaddr) const;
    void listen() const;
    int accept(InetAddress *peeraddr) const;
    void shutdownWrite() const;
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};

#endif//MY_MUDUO_SOCKET_H
