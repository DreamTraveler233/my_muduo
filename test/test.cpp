#include "../include/net/TcpServer.h"
#include <iostream>

// 消息到达回调函数
void onMessage(const net::TcpConnectionPtr& conn, net::Buffer* buf, net::Timestamp time) {
    std::string msg = buf->retrieveAllAsString();
    std::cout << "Received message: " << msg << std::endl;
    // 将消息原样返回给客户端
    if (conn->isConnected()) {
        conn->send(msg);
    }
}

// 连接建立/关闭回调函数
void onConnection(const net::TcpConnectionPtr& conn) {
    if (conn->isConnected()) {
        std::cout << "New connection from " << conn->getPeerAddress().toIpPort() << std::endl;
    } else {
        std::cout << "Connection closed: " << conn->getPeerAddress().toIpPort() << std::endl;
    }
}

int main() {
    net::EventLoop loop;
    net::InetAddress listenAddr(8080, "192.168.126.100"); // 监听 8888 端口
    net::TcpServer server(&loop, listenAddr, "TestServer");

    // 设置回调函数
    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);

    // 设置工作线程数量
    server.setThreadNum(2);

    // 启动服务器
    server.start();

    // 启动事件循环
    loop.loop();

    return 0;
}