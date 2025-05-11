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

/*
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * TcpConnection 设置回调函数 => Channel => Poller => Channel的回调操作
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

    EventLoop *getLoop() const;
    const std::string &getName() const;
    const InetAddress &getLocalAddress() const;
    const InetAddress &getPeerAddress() const;
    Buffer *getInputBuffer();
    Buffer *getOutputBuffer();

    bool isConnected() const;
    bool isDisconnected() const;
    void send(const std::string &buf);
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb);
    void setMessageCallback(const MessageCallback &cb);
    void setWriteCompleteCallback(const WriteCompleteCallback &cb);
    void setCloseCallback(const CloseCallback &cb);
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark);

    void connectEstablished();
    void connectDestroyed();

private:
    // Tcp连接状态机枚举定义
    enum StateE
    {
        kDisconnected,// 已经断开连接
        kConnecting,  // 正在连接
        kConnected,   // 已经连接
        kDisconnecting// 正在断开连接
    };

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *data, size_t len);
    void shutdownInLoop();

    void setState(StateE state);

    EventLoop *loop_;       // TcpConnection由subLoop管理，此处指针指向具体的subLoop实例
    const std::string name_;//连接名称标识，用于日志记录和调试时区分不同连接
    std::atomic_int state_; //原子状态标识
    bool reading_;          // 读状态标志位

    // 这里和Accept类似，需要将
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;// 本地端地址
    const InetAddress peerAddr_; // 对端地址

    ConnectionCallback connectionCallback_;      // 连接建立/断开回调函数，在连接状态变化时触发
    MessageCallback messageCallback_;            // 消息到达回调函数，当接收到完整数据报文时触发
    WriteCompleteCallback writeCompleteCallback_;// 写完成回调函数，当输出缓冲区数据全部写入socket后触发
    HighWaterMarkCallback highWaterMarkCallback_;// 高水位标记回调，当输出缓冲区数据量超过highWaterMark_时触发
    CloseCallback closeCallback_;                // 连接关闭回调，用于通知上层清理连接资源

    size_t highWaterMark_;// 流量控制阈值，单位：字节，用于防止输出缓冲区过度膨胀

    Buffer inputBuffer_; // 接收缓冲区
    Buffer outputBuffer_;// 发送缓冲区
};

#endif//MY_MUDUO_TCPCONNECTION_H
