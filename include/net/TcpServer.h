//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_TCPSERVER_H
#define MY_MUDUO_TCPSERVER_H

#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadpool.h"
#include "InetAddress.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "TcpConnection.h"

namespace net
{ /**
 * @brief TCP服务器类，负责管理网络连接和事件循环分发
 *
 * 该类封装了TCP服务器核心功能，包括端口监听、连接管理、事件回调分发等。
 * 采用主从Reactor模式，主循环处理新连接，子循环处理已建立连接的IO事件。
 */
    class TcpServer : NonCopyable
    {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;            // 线程初始化回调类型
        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;// 连接集合类型

        /// 端口复用选项枚举
        enum Option
        {
            kNoReusePort,// 禁止端口复用
            kReusePort,  // 允许端口复用
        };

        TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                  std::string nameArg, Option option = kNoReusePort);
        ~TcpServer();

        /// 设置工作线程数量（subLoop数量）
        void setThreadNum(int numThreads);

        /// 启动服务器，开始监听端口
        void start();

        // 设置各类事件回调函数
        void setThreadInitCallback(ThreadInitCallback cb);      // 线程初始化回调
        void setConnectionCallback(ConnectionCallback cb);      // 新连接建立回调
        void setMessageCallback(MessageCallback cb);            // 消息到达回调
        void setWriteCompleteCallback(WriteCompleteCallback cb);// 数据发送完成回调

        // 获取服务器信息接口
        const std::string &getIpPort() const;                // 获取监听地址的IP:PORT字符串
        const std::string &getName() const;                  // 获取服务器名称
        EventLoop *getLoop() const;                          // 获取主事件循环
        std::shared_ptr<EventLoopThreadPool> getThreadPool();// 获取线程池指针

    private:
        // 处理新连接到达（由Acceptor回调）
        void newConnection(int sockfd, const InetAddress &peerAddr);

        // 移除连接（供TcpConnection回调）
        void removeConnection(const TcpConnectionPtr &conn);

        // 在事件循环线程中执行实际的连接移除操作
        void removeConnectionInLoop(const TcpConnectionPtr &conn);

        EventLoop *loop_;         // 主事件循环（baseloop）
        const std::string ipPort_;// 格式化后的监听地址（IP:PORT）
        const std::string name_;  // 服务器名称标识

        std::unique_ptr<Acceptor> acceptor_;// 连接接收器（运行在baseloop）

        std::shared_ptr<EventLoopThreadPool> EventLoopThreadPool_;// 事件循环线程池

        // 事件回调函数对象
        ConnectionCallback connectionCallback_;      // 新连接建立/关闭时回调
        MessageCallback messageCallback_;            // 消息到达时回调
        WriteCompleteCallback writeCompleteCallback_;// 数据完全写入时回调
        ThreadInitCallback threadInitCallback_;      // 时间循环线程的初始化回调

        std::atomic_int started_;  // 服务器启动状态标记
        int nextConnId_;           // 下一个连接的序列号（用于生成连接名称）
        ConnectionMap connections_;// 当前维护的所有连接集合（线程安全需保障）
    };
}// namespace net

#endif//MY_MUDUO_TCPSERVER_H