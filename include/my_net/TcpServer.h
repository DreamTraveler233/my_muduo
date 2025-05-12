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

class TcpServer : NonCopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    enum Option {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr,
              std::string nameArg, Option option = kNoReusePort);
    ~TcpServer();

    // 设置底层subLoop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

    // 设置回调函数
    void setThreadInitCallback(const ThreadInitCallback &cb);
    void setConnectionCallback(const ConnectionCallback &cb);
    void setMessageCallback(const MessageCallback &cb);
    void setWriteCompleteCallback(const WriteCompleteCallback &cb);

    // 获取成员变量
    const std::string &getIpPort() const;
    const std::string &getName() const;
    EventLoop *getLoop() const;
    std::shared_ptr<EventLoopThreadPool> getThreadPool();

private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *loop_;//baseLoop 用户定义的loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;// 运行在mianLoop，任务是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;// one loop per thread

    ConnectionCallback connectionCallbacl_;      // 有新连接时的回调
    MessageCallback messageCallback_;            // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;// 消息发送完后的回调

    ThreadInitCallback threadInitCallback_;// loop线程初始化的回调

    std::atomic_int started_;
    int nextConnId_;

    ConnectionMap connections_;// 保存所有的连接
};

#endif//MY_MUDUO_TCPSERVER_H
