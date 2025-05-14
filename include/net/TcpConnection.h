//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_TCPCONNECTION_H
#define MY_MUDUO_TCPCONNECTION_H

#include "Buffer.h"
#include "Callbacks.h"
#include "Channel.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "Socket.h"
#include "SysHeadFile.h"

namespace net
{
    class Channel;
    class EventLoop;
    class Socket;

    /**
     * @class TcpConnection
     * @brief TCP 连接管理类，封装连接状态、IO 事件处理及回调机制
     *
     * 职责说明：
     * - 连接状态管理：维护 TCP 连接的生命周期（建立、数据传输、关闭）。
     * - 事件驱动：通过 [Channel] 监听 socket 的可读、可写、关闭和错误事件，并触发对应回调。
     * - 数据缓冲：使用双缓冲区（[inputBuffer_] 和 [outputBuffer_]）实现高效的非阻塞 IO 操作。
     * - 流量控制：通过高水位标记（[highWaterMark_]）防止发送缓冲区过度膨胀。
     * - 线程安全：通过事件循环（[EventLoop]）确保跨线程操作的安全性。
     */
    class TcpConnection : NonCopyable, public std::enable_shared_from_this<TcpConnection>
    {
    public:
        /**
         * @brief 构造函数，初始化 TCP 连接
         * @param loop 所属的事件循环
         * @param name 连接名称标识
         * @param sockfd 套接字文件描述符
         * @param localAddr 本地地址信息
         * @param peerAddr 对端地址信息
         */
        TcpConnection(EventLoop *loop,
                      std::string name,
                      int sockfd,
                      const InetAddress &localAddr,
                      const InetAddress &peerAddr);

        /**
         * @brief 析构函数，清理资源
         */
        ~TcpConnection();

        //------------------------- 连接属性访问接口 -------------------------
        /**
         * @brief 获取所属事件循环
         * @return 返回所属的 [EventLoop] 对象
         */
        EventLoop *getLoop() const;

        /**
         * @brief 获取连接名称标识
         * @return 返回连接名称
         */
        const std::string &getName() const;

        /**
         * @brief 获取本地地址信息
         * @return 返回本地地址信息
         */
        const InetAddress &getLocalAddress() const;

        /**
         * @brief 获取对端地址信息
         * @return 返回对端地址信息
         */
        const InetAddress &getPeerAddress() const;

        /**
         * @brief 获取输入缓冲区指针
         * @return 返回输入缓冲区指针
         */
        Buffer *getInputBuffer();

        /**
         * @brief 获取输出缓冲区指针
         * @return 返回输出缓冲区指针
         */
        Buffer *getOutputBuffer();

        //------------------------- 连接状态判断接口 -------------------------
        /**
         * @brief 判断是否处于已连接状态
         * @return 如果已连接，返回 true；否则返回 false
         */
        bool isConnected() const;

        /**
         * @brief 判断是否已完全断开连接
         * @return 如果已完全断开连接，返回 true；否则返回 false
         */
        bool isDisconnected() const;

        //------------------------- 数据操作接口 -------------------------
        /**
         * @brief 发送数据（线程安全）
         * @param buf 待发送的数据
         */
        void send(const std::string &buf);

        /**
         * @brief 主动关闭连接（半关闭模式）
         */
        void shutdown();

        //------------------------- 回调设置接口 -------------------------
        /**
         * @brief 设置连接状态变化回调
         * @param cb 连接状态变化回调函数
         */
        void setConnectionCallback(ConnectionCallback cb);

        /**
         * @brief 设置数据到达回调
         * @param cb 数据到达回调函数
         */
        void setMessageCallback(MessageCallback cb);

        /**
         * @brief 设置写完成回调
         * @param cb 写完成回调函数
         */
        void setWriteCompleteCallback(WriteCompleteCallback cb);

        /**
         * @brief 设置连接关闭回调
         * @param cb 连接关闭回调函数
         */
        void setCloseCallback(CloseCallback cb);

        /**
         * @brief 设置高水位回调
         * @param cb 高水位回调函数
         * @param highWaterMark 高水位阈值
         */
        void setHighWaterMarkCallback(HighWaterMarkCallback cb,
                                      size_t highWaterMark);

        //------------------------- 生命周期管理接口 -------------------------
        /**
         * @brief 由 TcpServer 调用，通知连接已建立
         */
        void connectEstablished();

        /**
         * @brief 由 TcpServer 调用，彻底销毁连接资源
         */
        void connectDestroyed();

    private:
        /**
         * @enum StateE
         * @brief TCP 连接状态枚举
         */
        enum StateE
        {
            kDisconnected, //!< 已断开（初始/最终状态）
            kConnecting,   //!< 正在建立连接（构造时初始状态）
            kConnected,    //!< 已连接（可正常通信）
            kDisconnecting //!< 正在断开连接（调用 shutdown 后状态）
        };

        //------------------------- 事件处理核心逻辑 -------------------------
        /**
         * @brief 处理可读事件（数据到达）
         * @param receiveTime 事件发生的时间戳
         */
        void handleRead(Timestamp receiveTime);

        /**
         * @brief 处理可写事件（发送缓冲区可写入）
         */
        void handleWrite();

        /**
         * @brief 处理连接关闭事件（FIN/RST）
         */
        void handleClose();

        /**
         * @brief 处理套接字错误事件
         */
        void handleError();

        //------------------------- 线程安全操作封装 -------------------------
        /**
         * @brief 在事件循环线程中实际发送数据
         * @param data 待发送的数据
         * @param len 数据长度
         */
        void sendInLoop(const void *data, size_t len);

        /**
         * @brief 在事件循环线程中实际关闭连接
         */
        void shutdownInLoop();

        /**
         * @brief 原子操作更新连接状态
         * @param state 新的连接状态
         */
        void setState(StateE state);

        //------------------------- 成员变量 -------------------------
        EventLoop *loop_;       //!< 所属事件循环（SubLoop）
        const std::string name_;//!< 连接名称（用于日志追踪）
        std::atomic_int state_; //!< 原子连接状态（StateE 枚举值）
        bool reading_;          //!< 读事件监听标志位

        std::unique_ptr<Socket> socket_;  //!< 套接字资源管理（RAII）
        std::unique_ptr<Channel> channel_;//!< 事件通道管理（绑定 socket 和事件回调）

        const InetAddress localAddr_;//!< 本地地址信息（IP+Port）
        const InetAddress peerAddr_; //!< 对端地址信息（IP+Port）

        //------------------------- 回调函数对象 -------------------------
        ConnectionCallback connectionCallback_;      //!< 连接建立/断开时触发
        MessageCallback messageCallback_;            //!< 数据到达时触发
        WriteCompleteCallback writeCompleteCallback_;//!< 数据完全写入内核缓冲区时触发
        HighWaterMarkCallback highWaterMarkCallback_;//!< 输出缓冲区超过阈值时触发
        CloseCallback closeCallback_;                //!< 连接关闭时通知上层清理资源

        size_t highWaterMark_;//!< 高水位阈值（默认 64MB，用于流量控制）

        Buffer inputBuffer_; //!< 输入缓冲区（存储接收数据）
        Buffer outputBuffer_;//!< 输出缓冲区（存储待发送数据）
    };
}// namespace net

#endif//MY_MUDUO_TCPCONNECTION_H
