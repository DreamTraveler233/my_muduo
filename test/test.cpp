//
// Created by shuzeyong on 2025/5/10.
//

#include "../include/net/TcpServer.h"
#include "../include/thp/ThreadPool.h"

class EchoServer
{
public:
    EchoServer(net::EventLoop *loop, const net::InetAddress &addr, const std::string &name)
        : server_(loop, addr, name),
          loop_(loop),
          globalThreadPool_(std::make_unique<thp::ThreadPool>())
    {
        // 启动线程池
        globalThreadPool_->start();

        // 设置消息回调函数
        server_.setMessageCallback(
                [this](const net::TcpConnectionPtr &conn, net::Buffer *buf, net::Timestamp timestamp) {
                    onMessage(conn, buf, timestamp);
                });

        // 设置loop线程数量
        // server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    // 可读写事件回调函数
    void onMessage(const net::TcpConnectionPtr &conn, net::Buffer *buf, net::Timestamp time)
    {
        std::string data = buf->retrieveAllAsString();

        globalThreadPool_->submitTask([conn, data]() {
            // 模拟耗时计算（如数据库查询、图像处理）
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string result = "Processed: " + data;

            // 返回到subLoop线程发送
            conn->getLoop()->runInLoop([conn, result] {
                if (conn->isConnected()) {
                    conn->send(result);
                    // conn->shutdown();  // 根据业务需求决定是否关闭
                }
            });
        });
    }

    net::EventLoop *loop_;
    net::TcpServer server_;
    std::unique_ptr<thp::ThreadPool> globalThreadPool_;
};

int main()
{
    net::EventLoop loop;
    net::InetAddress addr(8080, "192.168.126.100");
    EchoServer server(&loop, addr, "EchoServer-01");// Acceptor => non-blacking listenfd => create bind
    server.start();                                 // listen EventLoopThread listenfd => mainLoop
    loop.loop();                                    // 启动mainLoop底层的Poller，开始监听事件

    return 0;
}
