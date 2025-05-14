//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/net/Socket.h"

using namespace net;

Socket::Socket(int sockfd)
    : sockfd_(sockfd)
{}

void Socket::setKeepAlive(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReusePort(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReuseAddr(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setTcpNoDelay(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::shutdownWrite() const
{
    if (shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("Socket::shutdownWrite");
    }
}

int Socket::accept(InetAddress *peeraddr) const
{
    // 初始化客户端地址结构体
    sockaddr_in client_addr = {};
    socklen_t client_addrlen = sizeof(client_addr);

    // 调用系统函数 accept 接受客户端连接
    int connfd = ::accept4(sockfd_, (sockaddr *) &client_addr, &client_addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);

    // 如果连接成功，将客户端地址信息存储在 peeraddr 中
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(client_addr);
    }

    // 返回连接文件描述符
    return connfd;
}

void Socket::listen() const
{
    if (::listen(sockfd_, 1024) < 0)
    {
        LOG_FATAL("listen sockfd %d fail \n", sockfd_);
    }
}

void Socket::bind(const InetAddress &localaddr) const
{
    if (::bind(sockfd_, (sockaddr *) &localaddr.getSockAddr(), sizeof(sockaddr_in)) < 0)
    {
        LOG_FATAL("bind sockfd %d fail \n", sockfd_);
    }
}

int Socket::getFd() const
{
    return sockfd_;
}
Socket::~Socket()
{
    close(sockfd_);
}