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
{
    /**
     * @class TcpServer
     * @brief TCP 服务器类，负责管理网络连接和事件循环分发
     *
     * 该类封装了 TCP 服务器核心功能，包括端口监听、连接管理、事件回调分发等。
     * 采用主从 Reactor 模式，主循环处理新连接，子循环处理已建立连接的 IO 事件。
     */
    class TcpServer : NonCopyable
    {
    public:
        using ThreadInitCallback = std::function<void(EventLoop *)>;            //!< 线程初始化回调类型
        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;//!< 连接集合类型

        /**
         * @enum Option
         * @brief 端口复用选项枚举
         */
        enum Option
        {
            kNoReusePort,//!< 禁止端口复用
            kReusePort,  //!< 允许端口复用
        };

        /**
         * @brief 构造函数，初始化 TCP 服务器
         * @param loop 主事件循环
         * @param listenAddr 监听地址
         * @param nameArg 服务器名称
         * @param option 端口复用选项（默认为 kNoReusePort）
         */
        TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                  std::string nameArg, Option option = kNoReusePort);

        /**
         * @brief 析构函数，清理资源
         */
        ~TcpServer();

        /**
         * @brief 设置工作线程数量（subLoop 数量）
         * @param numThreads 线程数量
         */
        void setThreadNum(int numThreads);

        /**
         * @brief 启动服务器，开始监听端口
         */
        void start();

        //------------------------- 事件回调设置接口 -------------------------
        /**
         * @brief 设置线程初始化回调
         * @param cb 线程初始化回调函数
         */
        void setThreadInitCallback(ThreadInitCallback cb);

        /**
         * @brief 设置新连接建立回调
         * @param cb 新连接建立回调函数
         */
        void setConnectionCallback(ConnectionCallback cb);

        /**
         * @brief 设置消息到达回调
         * @param cb 消息到达回调函数
         */
        void setMessageCallback(MessageCallback cb);

        /**
         * @brief 设置数据发送完成回调
         * @param cb 数据发送完成回调函数
         */
        void setWriteCompleteCallback(WriteCompleteCallback cb);

        //------------------------- 服务器信息获取接口 -------------------------
        /**
         * @brief 获取监听地址的 IP:PORT 字符串
         * @return 返回监听地址的 IP:PORT 字符串
         */
        const std::string &getIpPort() const;

        /**
         * @brief 获取服务器名称
         * @return 返回服务器名称
         */
        const std::string &getName() const;

        /**
         * @brief 获取主事件循环
         * @return 返回主事件循环指针
         */
        EventLoop *getLoop() const;

        /**
         * @brief 获取线程池指针
         * @return 返回线程池指针
         */
        std::shared_ptr<EventLoopThreadPool> getThreadPool();

    private:
        /**
         * @brief 处理新连接到达（由 Acceptor 回调）
         * @param sockfd 新连接的套接字文件描述符
         * @param peerAddr 对端地址信息
         */
        void newConnection(int sockfd, const InetAddress &peerAddr);

        /**
         * @brief 移除连接（供 TcpConnection 回调）
         * @param conn 需要移除的连接对象
         */
        void removeConnection(const TcpConnectionPtr &conn);

        /**
         * @brief 在事件循环线程中执行实际的连接移除操作
         * @param conn 需要移除的连接对象
         */
        void removeConnectionInLoop(const TcpConnectionPtr &conn);

        EventLoop *loop_;         //!< 主事件循环（baseloop）
        const std::string ipPort_;//!< 格式化后的监听地址（IP:PORT）
        const std::string name_;  //!< 服务器名称标识

        std::unique_ptr<Acceptor> acceptor_;//!< 连接接收器（运行在 baseloop）

        std::shared_ptr<EventLoopThreadPool> EventLoopThreadPool_;//!< 事件循环线程池

        //------------------------- 事件回调函数对象 -------------------------
        ConnectionCallback connectionCallback_;      //!< 新连接建立/关闭时回调
        MessageCallback messageCallback_;            //!< 消息到达时回调
        WriteCompleteCallback writeCompleteCallback_;//!< 数据完全写入时回调
        ThreadInitCallback threadInitCallback_;      //!< 时间循环线程的初始化回调

        std::atomic_int started_;  //!< 服务器启动状态标记
        int nextConnId_;           //!< 下一个连接的序列号（用于生成连接名称）
        ConnectionMap connections_;//!< 当前维护的所有连接集合（线程安全需保障）
    };
}// namespace net

#endif//MY_MUDUO_TCPSERVER_H
