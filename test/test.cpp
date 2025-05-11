//
// Created by shuzeyong on 2025/5/10.
//

#include "../include/my_net/TcpServer.h"

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop, addr, name),
          loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
                std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
                std::bind(&EchoServer::onMessage, this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置loop线程数量
        //server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    // 连接建立和连接断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->isConnected())
        {
            LOG_INFO("Connection UP : %s", conn->getPeerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->getPeerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件回调函数
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();// 关闭服务器写端
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8080,"192.168.126.100");
    EchoServer server(&loop, addr, "EchoServer-01");// Acceptor => non-blacking listenfd => create bind
    server.start();                                 // listen EventLoopThread listenfd => mainLoop
    loop.loop();                                    // 启动mainLoop底层的Poller，开始监听事件

    return 0;
}