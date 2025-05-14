//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_SOCKET_H
#define MY_MUDUO_SOCKET_H

#include "InetAddress.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"

namespace net
{ /**
 * @class Socket
 * @brief 封装套接字文件描述符及其核心操作
 *
 * 该类继承自 NonCopyable，禁止拷贝语义，管理套接字生命周期。
 * 提供绑定地址、监听连接、设置套接字选项等网络基础操作。
 */
    class Socket : NonCopyable
    {
    public:
        explicit Socket(int sockfd);
        ~Socket();

        // 获取当前管理的套接字文件描述符
        [[nodiscard]] int getFd() const;
        // 绑定套接字到指定本地地址
        void bind(const InetAddress &localaddr) const;
        // 启动套接字监听模式
        void listen() const;
        // 接受客户端连接请求
        int accept(InetAddress *peeraddr) const;
        // 半关闭写方向
        void shutdownWrite() const;

        // 禁用Nagle算法
        void setTcpNoDelay(bool on) const;
        // 地址重用
        void setReuseAddr(bool on) const;
        // 端口重用
        void setReusePort(bool on) const;
        // TCP保活机制
        void setKeepAlive(bool on) const;

    private:
        const int sockfd_;
    };
}// namespace net

#endif//MY_MUDUO_SOCKET_H
