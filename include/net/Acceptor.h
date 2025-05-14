//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_ACCEPTOR_H
#define MY_MUDUO_ACCEPTOR_H

#include "Channel.h"
#include "EventLoop.h"
#include "NonCopyable.h"
#include "Socket.h"
#include "SysHeadFile.h"

namespace net
{
    /**
     * @class Acceptor
     * @brief 监听套接字管理器，负责接受新 TCP 连接
     *
     * 该类继承自 NonCopyable，管理监听套接字的生命周期和事件处理。
     * 通过事件循环监听新连接请求，并通过回调机制将新连接分发给上层模块。
     */
    class Acceptor : NonCopyable
    {
    public:
        /**
         * @brief 新连接到达时的回调函数类型
         *
         * @param sockfd 新连接的文件描述符
         * @param addr 客户端的网络地址信息
         */
        using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &addr)>;

        /**
         * @brief 构造函数
         *
         * 初始化 Acceptor 对象，创建监听套接字并绑定到指定地址。
         *
         * @param loop 事件循环对象指针，用于处理套接字事件
         * @param listenAddr 需要绑定的网络地址信息（IP + 端口）
         * @param reuseport 是否启用 SO_REUSEPORT 选项（允许多个进程绑定相同端口）
         */
        Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);

        /**
         * @brief 析构函数
         *
         * 负责资源清理工作，禁用并移除事件监听。
         */
        ~Acceptor();

        /**
         * @brief 设置新连接到达时的回调函数
         *
         * @param cb 新连接回调函数，用于处理新连接
         */
        void setNewConnectionCallback(NewConnectionCallback cb);

        /**
         * @brief 获取当前监听状态
         *
         * @return bool 返回当前是否处于监听状态
         */
        [[nodiscard]] bool getListenning() const;

        /**
         * @brief 启动监听功能
         *
         * 激活底层 socket 的监听模式，并注册读事件监听以接收新连接。
         */
        void listen();

    private:
        /**
         * @brief 处理监听套接字的读事件（新连接到达）
         *
         * 该函数在网络监听套接字可读时被触发，执行 accept 操作接收新连接，
         * 并根据是否设置回调函数进行后续处理。
         */
        void handleRead();

        EventLoop *loop_;                            //!< 所属事件循环（通常为主事件循环 mainLoop），由用户在主线程创建
        Socket acceptSocket_;                        //!< 监听套接字对象
        Channel acceptChannel_;                      //!< 监听套接字对应的事件通道
        NewConnectionCallback newConnectionCallback_;//!< 新连接回调函数
        bool listenning_;                            //!< 监听状态标志（true=正在监听）
    };
}// namespace net

#endif// MY_MUDUO_ACCEPTOR_H
