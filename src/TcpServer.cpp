//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/my_net/TcpServer.h"

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, errno);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg, TcpServer::Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option = kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallbacl_(),
      messageCallback_(),
      nextConnId_(1),
      started_(0)
{
    // 当有新用户连接时，会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                  std::placeholders::_1, std::placeholders::_2));
}

/**
 * @brief 处理新客户端连接的回调函数
 *
 * 当有新客户端连接时，acceptor会调用此函数。该函数负责为新连接选择一个事件循环（subLoop），
 * 创建TcpConnection对象，并将其添加到连接管理器中。
 *
 * @param sockfd 新连接的套接字文件描述符
 * @param peerAddr 对端（客户端）的地址信息
 */
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    // 生成唯一的连接名称，格式为：服务器名称-IP:端口#连接ID
    char buf[64] = {};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    // 记录新连接的日志信息
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
             name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 获取本地地址信息
    sockaddr_in local = {};
    socklen_t addrlen = sizeof(local);
    if (getsockname(sockfd, (sockaddr *) &local, &addrlen) < 0)
    {
        LOG_ERROR("TcpServer::newConnection");
    }
    InetAddress localAddr(local);

    // 使用轮询算法从线程池中选择一个事件循环（subLoop）来管理新连接的Channel
    EventLoop *ioLoop = threadPool_->getNextLoop();

    // 创建TcpConnection对象，并将其添加到连接管理器中
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,// sockfd => Socket => Channel
                                            localAddr,
                                            peerAddr));
    connections_[connName] = conn;

    // 设置TcpConnection的回调函数，下面的回调函数都是用户设置的
    // 调用流程：TcpServer => TcpConnection => Channel => Poller => notify channel 调用回调函数
    conn->setConnectionCallback(connectionCallbacl_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调函数
    // 用户调用conn->shutdown() => socket->shutdownWrite() => poller给channel上报EPOLLHUP事件
    // => channel::closeCallback => TcpConnection::handleClose（进行一系列关闭连接的操作）
    // => closeCallback_(guardThis) => TcpServer::removeConnection
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用TcpConnection::connectEstablished，执行连接建立的操作
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

/**
 * @brief 在事件循环中移除指定的TCP连接。
 *
 * 该函数负责在事件循环中安全地移除一个TCP连接。它会从连接列表中删除该连接，
 * 并在对应的I/O事件循环中安排一个任务来销毁连接。
 *
 * @param conn 要移除的TCP连接的智能指针。
 * @return 无返回值。
 */
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    // 记录日志，表示正在移除连接
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s", name_.c_str(), conn->getName().c_str());

    // 从连接列表中移除指定名称的连接，并返回移除的数量
    size_t n = connections_.erase(conn->getName());

    // 获取连接所属的I/O事件循环
    EventLoop *ioLoop = conn->getLoop();

    // 在I/O事件循环中安排一个任务，调用连接的connectDestroyed方法
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

/**
 * @brief 启动TCP服务器
 *
 * 该函数负责初始化服务器资源，启动底层线程池并开始监听连接。
 * 通过计数器确保同一TcpServer对象仅执行一次启动流程。
 * 无参数和返回值。
 */
void TcpServer::start()
{
    // 通过原子操作确保启动逻辑只会执行一次
    if (started_++ == 0)// 防止一个TcpServer对象被多次start
    {
        // 启动线程池处理IO事件，threadInitCallback_用于线程初始化配置
        threadPool_->start(threadInitCallback_);

        // 在事件循环线程中启动监听器，保证线程安全性
        // 将Acceptor::listen绑定到EventLoop的执行队列
        // 确保 listen() 在 mainLoop 线程中执行
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

/**
 * @brief TcpServer类的析构函数。
 *
 * 该析构函数负责释放TcpServer对象所占用的资源，并确保所有已建立的TcpConnection对象被正确销毁。
 * 在析构过程中，会遍历所有已建立的连接，并通过事件循环机制调用每个连接的connectDestroyed方法，
 * 以确保连接资源被安全释放。
 */
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
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

std::shared_ptr<EventLoopThreadPool> TcpServer::getThreadPool()
{
    return threadPool_;
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
void TcpServer::setWriteCompleteCallback(const WriteCompleteCallback &cb)
{
    writeCompleteCallback_ = cb;
}
void TcpServer::setMessageCallback(const MessageCallback &cb)
{
    messageCallback_ = cb;
}
void TcpServer::setConnectionCallback(const ConnectionCallback &cb)
{
    connectionCallbacl_ = cb;
}
void TcpServer::setThreadInitCallback(const TcpServer::ThreadInitCallback &cb)
{
    threadInitCallback_ = cb;
}
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setNumThread(numThreads);
}