//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/net/TcpServer.h"

using namespace net;

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, errno);
    }
    return loop;
}

/**
 * @brief 默认的TCP连接回调函数
 *        当TCP连接的状态发生变化时，此函数会被调用它主要用于记录连接的建立或断开
 */
void defaultConnectionCallback(const TcpConnectionPtr &conn)
{
    LOG_INFO("%s -> %s is %s",
             conn->getLocalAddress().toIpPort().c_str(),
             conn->getPeerAddress().toIpPort().c_str(),
             conn->isConnected() ? "UP" : "DOWN");
}

/**
 * @brief 默认的TCP消息回调函数，用于处理接收到的TCP消息。
 *        该函数会清空接收缓冲区中的所有数据，通常用于忽略接收到的消息或进行简单的数据清理。
 */
void defaultMessageCallback(const TcpConnectionPtr &, Buffer *buf, Timestamp)
{
    buf->retrieveAll();
}

TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     std::string name,
                     TcpServer::Option option)
    : loop_(CheckLoopNotNull(loop)),                                  // 检查事件循环是否为空，并初始化
      ipPort_(listenAddr.toIpPort()),                                 // 获取监听地址的 IP 和端口字符串
      name_(std::move(name)),                                         // 移动服务器名称到成员变量
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),// 创建 Acceptor 对象，用于接受新连接
      EventLoopThreadPool_(new EventLoopThreadPool(loop, name_)),     // 创建事件循环线程池，用于处理连接
      connectionCallback_(defaultConnectionCallback),                 // 初始化默认的连接回调函数
      messageCallback_(defaultMessageCallback),                       // 初始化默认的消息回调函数
      nextConnId_(1),                                                 // 初始化下一个连接 ID 为 1
      started_(0)                                                     // 初始化服务器启动状态为未启动
{
    // 设置 Acceptor 的新连接回调函数，当有新连接时，调用 TcpServer::newConnection 方法
    acceptor_->setNewConnectionCallback(
            [this](auto &&PH1, auto &&PH2) {
                newConnection(
                        std::forward<decltype(PH1)>(PH1),
                        std::forward<decltype(PH2)>(PH2));
            });
}

TcpServer::~TcpServer()
{
    // 记录日志，表示TcpServer对象正在析构
    LOG_INFO("TcpServer::~TcpServer %s destructing", name_.c_str());

    // 遍历所有已建立的连接
    for (auto &item: connections_)
    {
        // 创建一个局部的shared_ptr智能指针对象，用于管理TcpConnection对象
        // 当该局部对象超出作用域时，会自动释放TcpConnection对象资源
        TcpConnectionPtr conn(item.second);

        // 重置原始指针，避免重复释放
        item.second.reset();

        // 通过事件循环机制调用TcpConnection的connectDestroyed方法，确保连接资源被安全释放
        conn->getLoop()->runInLoop([conn] { conn->connectDestroyed(); });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 生成唯一的连接名称，格式为：服务器名称@连接ID（例如：Server@1）
    char buffer[32] = {};
    snprintf(buffer, sizeof(buffer), "@%d", nextConnId_);
    std::string connName = name_ + buffer;

    // 记录新连接的日志信息，包括客户端地址和连接ID
    LOG_INFO("[%s] NEW CONNECTION | Client:%s | ConnID:%d",
             name_.c_str(), peerAddr.toIpPort().c_str(), nextConnId_);

    ++nextConnId_;

    // 获取本地地址信息
    sockaddr_in local = {};
    socklen_t addrlen = sizeof(local);
    if (getsockname(sockfd, (sockaddr *) &local, &addrlen) < 0)
    {
        LOG_ERROR("TcpServer::newConnection");
    }
    InetAddress localAddr(local);// 将sockaddr_in转换为InetAddress对象

    // 从线程池中选择一个事件循环（subLoop）来处理新连接
    // 使用轮询（Round-Robin）算法分配，确保负载均衡
    EventLoop *ioLoop = EventLoopThreadPool_->getNextLoop();

    // 创建TcpConnection对象，管理新连接的通信
    TcpConnectionPtr conn(new TcpConnection(ioLoop,   // 连接所在的EventLoop
                                            connName, // 连接名称
                                            sockfd,   // accept返回的connfd
                                            localAddr,// 服务端地址
                                            peerAddr  // accept返回的客户端地址
                                            ));
    connections_[connName] = conn;// 将连接管理到ConnectionMap中

    // 设置用户回调函数：这些回调由TcpServer的用户定义（如业务逻辑处理）
    conn->setConnectionCallback(connectionCallback_);      // 1. 连接建立/关闭回调
    conn->setMessageCallback(messageCallback_);            // 2. 消息到达回调，当Channel有数据可读时触发
    conn->setWriteCompleteCallback(writeCompleteCallback_);// 3. 数据发送完成回调，当数据完全写入内核缓冲区时触发

    // 设置关闭连接的回调函数，当TcpConnection需要关闭时（如收到FIN包），调用此Lambda回调
    // 流程：TcpConnection::handleClose -> closeCallback_ -> TcpServer::removeConnection
    conn->setCloseCallback(
            [this](auto &&PH1) {
                removeConnection(std::forward<decltype(PH1)>(PH1));// 从连接管理器中移除并销毁连接
            });

    // 在选定的subLoop中执行连接建立操作
    // 通过runInLoop确保connectEstablished在ioLoop线程中调用，避免竞态条件
    ioLoop->runInLoop([conn] { conn->connectEstablished(); });
    // connectEstablished()会将Channel注册到Poller，开始监听可读事件
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop([this, conn] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    // 日志记录：包含服务器名、连接名、客户端地址、本地地址等关键信息
    LOG_INFO("[%s] REMOVE CONNECTION | ConnName:%s | Client:%s | Local:%s",
             name_.c_str(),
             conn->getName().c_str(),
             conn->getPeerAddress().toIpPort().c_str(),  // 获取客户端地址（IP:Port）
             conn->getLocalAddress().toIpPort().c_str());// 获取本地地址（IP:Port）

    // 从连接池中删除（RAII方式自动管理连接生命周期）
    size_t n = connections_.erase(conn->getName());
    (void) n;

    // 在IO线程中安全销毁连接（确保在所属事件循环线程执行）
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop([conn] {
        conn->connectDestroyed();// 触发连接销毁前的清理操作
    });
}

void TcpServer::start()
{
    // 通过原子操作确保启动逻辑只会执行一次
    if (started_++ == 0)// 防止一个TcpServer对象被多次start
    {
        // 启动线程池处理IO事件，threadInitCallback_用于线程初始化配置
        EventLoopThreadPool_->start(threadInitCallback_);

        // 在mainLoop中启动监听器Acceptpr，开始监听新连接
        loop_->runInLoop([acceptor = acceptor_.get()] { acceptor->listen(); });
    }
}

std::shared_ptr<EventLoopThreadPool> TcpServer::getThreadPool()
{
    return EventLoopThreadPool_;
}

EventLoop *TcpServer::getLoop() const
{
    return loop_;
}

const std::string &TcpServer::getName() const
{
    return name_;
}

const std::string &TcpServer::getIpPort() const
{
    return ipPort_;
}

void TcpServer::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    writeCompleteCallback_ = std::move(cb);
}

void TcpServer::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}

void TcpServer::setConnectionCallback(ConnectionCallback cb)
{
    connectionCallback_ = std::move(cb);
}

void TcpServer::setThreadInitCallback(TcpServer::ThreadInitCallback cb)
{
    threadInitCallback_ = std::move(cb);
}

void TcpServer::setThreadNum(int numThreads)
{
    EventLoopThreadPool_->setNumThread(numThreads);
}
