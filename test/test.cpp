//
// Created by shuzeyong on 2025/5/10.
//

#include "../include/net/TcpServer.h"

class EchoServer
{
public:
    EchoServer(net::EventLoop *loop, const net::InetAddress &addr, const std::string &name)
        : server_(loop, addr, name),
          loop_(loop)
    {
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
    // 可读写事件回调函数
    void onMessage(const net::TcpConnectionPtr &conn, net::Buffer *buf, net::Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();
    }

    net::EventLoop *loop_;
    net::TcpServer server_;
};

int main()
{
    net::EventLoop loop;
    net::InetAddress addr(8080,"192.168.126.100");
    EchoServer server(&loop, addr, "EchoServer-01");// Acceptor => non-blacking listenfd => create bind
    server.start();                                 // listen EventLoopThread listenfd => mainLoop
    loop.loop();                                    // 启动mainLoop底层的Poller，开始监听事件

    return 0;
}