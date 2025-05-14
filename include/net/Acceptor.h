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

namespace net
{
    /**
     * @class Acceptor
     * @brief 监听套接字管理器，负责接受新TCP连接
     *
     * 该类继承自 NonCopyable，管理监听套接字的生命周期和事件处理。
     * 通过事件循环监听新连接请求，并通过回调机制将新连接分发给上层模块。
     */
    class Acceptor : NonCopyable
    {
    public:
        // 新连接到达时的回调函数类型（参数：新连接的文件描述符和客户端地址）
        using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

        Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
        ~Acceptor();

        // 设置新连接到达时的回调函数
        void setNewConnectionCallback(NewConnectionCallback cb);
        // 获取当前监听状态
        [[nodiscard]] bool getListenning() const;
        // 启动监听功能
        void listen();

    private:
        // 处理监听套接字的读事件（新连接到达）
        void handleRead();

        EventLoop *loop_;                            // 所属事件循环（通常为主事件循环mainLoop），由用户在主线程创建
        Socket acceptSocket_;                        // 监听套接字对象
        Channel acceptChannel_;                      // 监听套接字对应的事件通道
        NewConnectionCallback newConnectionCallback_;// 新连接回调函数
        bool listenning_;                            // 监听状态标志（true=正在监听）
    };
}// namespace net

#endif//MY_MUDUO_ACCEPTOR_H
