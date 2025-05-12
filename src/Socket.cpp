//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/my_net/Socket.h"

Socket::Socket(int sockfd)
    : sockfd_(sockfd)
{}

/**
 * @brief 设置套接字的SO_KEEPALIVE选项
 * @param on 启用标志，true表示开启TCP保活机制，false则关闭
 *
 * 通过设置SO_KEEPALIVE选项，操作系统会定时检测连接是否存活。
 * 当启用时，若长时间无数据交换，系统会自动发送探测包检测对端状态
 */
void Socket::setKeepAlive(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

/**
 * @brief 设置套接字的SO_REUSEPORT选项
 * @param on 启用标志，true表示允许多个套接字绑定相同端口
 *
 * 启用SO_REUSEPORT选项后，允许多个进程/线程绑定相同IP和端口组合，
 * 常用于实现负载均衡或高并发服务。需注意操作系统版本兼容性
 */
void Socket::setReusePort(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
}

/**
 * @brief 设置套接字的SO_REUSEADDR选项
 * @param on 启用标志，true表示允许地址重用
 *
 * 启用SO_REUSEADDR选项后，允许套接字绑定处于TIME_WAIT状态的本地地址，
 * 可避免服务器重启时出现"地址已在使用"的错误
 */
void Socket::setReuseAddr(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

/**
 * @brief 设置套接字的TCP_NODELAY选项
 * @param on 启用标志，true表示禁用Nagle算法
 *
 * 通过设置TCP_NODELAY选项禁用Nagle算法，可减少小数据包的传输延迟，
 * 适用于需要低延迟的实时通信场景。启用后数据将立即发送不缓冲
 */
void Socket::setTcpNoDelay(bool on) const
{
    int optval = on ? 1 : 0;
    setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

/**
 * @brief 关闭套接字的写方向（半关闭）
 *
 * 调用系统shutdown函数禁用本套接字的发送功能，允许：
 * 1. 确保发送缓冲区数据被发送并收到ACK
 * 2. 对端会收到EOF（read返回0）
 * 3. 仍可接收数据
 */
void Socket::shutdownWrite() const
{
    if (shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("Socket::shutdownWrite");
    }
}

/**
 * @brief 接受一个客户端连接请求，并获取客户端的地址信息。
 *
 * 该函数通过调用系统函数 `::accept` 来接受一个客户端连接请求。如果连接成功，
 * 会将客户端的地址信息存储在 `peeraddr` 中，并返回新创建的连接文件描述符。
 *
 * @param peeraddr 指向 `InetAddress` 对象的指针，用于存储客户端的地址信息。
 *                如果连接成功，该对象将被更新为客户端地址。
 *
 * @return 返回新创建的连接文件描述符。如果连接失败，返回值为 -1。
 */
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

int Socket::getFd() const { return sockfd_; }
Socket::~Socket() { close(sockfd_); }