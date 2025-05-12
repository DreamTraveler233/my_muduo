//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_TCPCONNECTION_H
#define MY_MUDUO_TCPCONNECTION_H

#include "Buffer.h"
#include "Callbacks.h"
#include "Channel.h"
#include "InetAddress.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "Socket.h"
#include "SysHeadFile.h"
#include "Timestamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * @class TcpConnection
 * @brief TCP连接管理类，封装连接状态、IO事件处理及回调机制
 *
 * 职责说明：
 * - 连接状态管理：维护TCP连接的生命周期（建立、数据传输、关闭）。
 * - 事件驱动：通过 Channel 监听socket的可读、可写、关闭和错误事件，并触发对应回调。
 * - 数据缓冲：使用双缓冲区（inputBuffer_ 和 outputBuffer_）实现高效的非阻塞IO操作。
 * - 流量控制：通过高水位标记（highWaterMark_）防止发送缓冲区过度膨胀。
 * - 线程安全：通过事件循环（EventLoop）确保跨线程操作的安全性。
 */
class TcpConnection : NonCopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                  std::string nameArg,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);

    ~TcpConnection();

    //------------------------- 连接属性访问接口 -------------------------
    EventLoop *getLoop() const;                // 获取所属事件循环
    const std::string &getName() const;        // 获取连接名称标识
    const InetAddress &getLocalAddress() const;// 获取本地地址信息
    const InetAddress &getPeerAddress() const; // 获取对端地址信息
    Buffer *getInputBuffer();                  // 获取输入缓冲区指针
    Buffer *getOutputBuffer();                 // 获取输出缓冲区指针

    //------------------------- 连接状态判断接口 -------------------------
    bool isConnected() const;   // 判断是否处于已连接状态
    bool isDisconnected() const;// 判断是否已完全断开连接

    //------------------------- 数据操作接口 -------------------------
    void send(const std::string &buf);// 发送数据（线程安全）
    void shutdown();                  // 主动关闭连接（半关闭模式）

    //------------------------- 回调设置接口 -------------------------
    void setConnectionCallback(const ConnectionCallback &cb);      // 设置连接状态变化回调
    void setMessageCallback(const MessageCallback &cb);            // 设置数据到达回调
    void setWriteCompleteCallback(const WriteCompleteCallback &cb);// 设置写完成回调
    void setCloseCallback(const CloseCallback &cb);                // 设置连接关闭回调
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, // 设置高水位回调
                                  size_t highWaterMark);

    //------------------------- 生命周期管理接口 -------------------------
    void connectEstablished();// 由TcpServer调用，通知连接已建立
    void connectDestroyed();  // 由TcpServer调用，彻底销毁连接资源

private:
    /// @brief TCP连接状态枚举
    enum StateE
    {
        kDisconnected,// 已断开（初始/最终状态）
        kConnecting,  // 正在建立连接（构造时初始状态）
        kConnected,   // 已连接（可正常通信）
        kDisconnecting// 正在断开连接（调用shutdown后状态）
    };

    //------------------------- 事件处理核心逻辑 -------------------------
    void handleRead(Timestamp receiveTime);// 处理可读事件（数据到达）
    void handleWrite();                    // 处理可写事件（发送缓冲区可写入）
    void handleClose();                    // 处理连接关闭事件（FIN/RST）
    void handleError();                    // 处理套接字错误事件

    //------------------------- 线程安全操作封装 -------------------------
    void sendInLoop(const void *data, size_t len);// 在事件循环线程中实际发送数据
    void shutdownInLoop();                        // 在事件循环线程中实际关闭连接

    void setState(StateE state);// 原子操作更新连接状态

    //------------------------- 成员变量 -------------------------
    EventLoop *loop_;       // 所属事件循环（SubLoop）
    const std::string name_;// 连接名称（用于日志追踪）
    std::atomic_int state_; // 原子连接状态（StateE枚举值）
    bool reading_;          // 读事件监听标志位

    std::unique_ptr<Socket> socket_;  // 套接字资源管理（RAII）
    std::unique_ptr<Channel> channel_;// 事件通道管理（绑定socket和事件回调）

    const InetAddress localAddr_;// 本地地址信息（IP+Port）
    const InetAddress peerAddr_; // 对端地址信息（IP+Port）

    //------------------------- 回调函数对象 -------------------------
    ConnectionCallback connectionCallback_;      // 连接建立/断开时触发
    MessageCallback messageCallback_;            // 数据到达时触发
    WriteCompleteCallback writeCompleteCallback_;// 数据完全写入内核缓冲区时触发
    HighWaterMarkCallback highWaterMarkCallback_;// 输出缓冲区超过阈值时触发
    CloseCallback closeCallback_;                // 连接关闭时通知上层清理资源

    size_t highWaterMark_;// 高水位阈值（默认64MB，用于流量控制）

    Buffer inputBuffer_; // 输入缓冲区（存储接收数据）
    Buffer outputBuffer_;// 输出缓冲区（存储待发送数据）
};

#endif//MY_MUDUO_TCPCONNECTION_H
