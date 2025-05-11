//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/my_net/Acceptor.h"

/**
 * @brief 创建非阻塞TCP套接字
 *
 * 创建并返回一个非阻塞的TCP套接字描述符。该套接字具有以下特性：
 * 1. 使用IPv4地址族(AF_INET)
 * 2. 采用TCP协议->流式传输(SOCK_STREAM)
 * 3. 设置为非阻塞模式(SOCK_NONBLOCK)
 * 4. 在执行exec时自动关闭(SOCK_CLOEXEC)
 *
 * @return int 套接字文件描述符。当返回值<0时，程序会记录致命错误并终止，
 *             成功时返回非负整型描述符
 */
static int createNonblocking()
{
    // 创建TCP套接字并设置非阻塞、exec关闭标志
    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    // 错误处理：socket创建失败时终止程序
    if (sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

/**
 * @brief Acceptor构造函数 - 创建监听套接字并初始化相关配置
 * @param loop 事件循环对象指针，用于处理套接字事件
 * @param listenAddr 需要绑定的网络地址信息（IP+端口）
 * @param reuseport 是否启用SO_REUSEPORT选项（允许多个进程绑定相同端口）
 */
Acceptor::Acceptor(EventLoop *loop, InetAddress listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),// 创建套接字
      acceptChannel_(loop, acceptSocket_.getFd()),
      listenning_(false)
{
    // 设置套接字选项：地址重用+端口重用策略
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);

    // 将套接字绑定到指定网络地址
    acceptSocket_.bind(std::move(listenAddr));

    // 设置读事件回调，当监听套接字可读时（有新连接）触发handleRead
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

/**
 * @brief Acceptor类的析构函数，负责资源清理工作
 *
 * 在对象生命周期结束时，停止所有网络事件监听并移除底层channel资源。
 * 主要流程：
 * 1. 禁用acceptChannel_上的所有事件监听（如读/写/错误事件）
 * 2. 从事件循环中完全移除该channel，防止后续事件触发
 */
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::setNewConnectionCallback(Acceptor::NewConnectionCallback cb)
{
    newConnectionCallback_ = std::move(cb);
}

bool Acceptor::getListenning()
{
    return listenning_;
}

/**
 * @brief 启动服务器监听功能
 *
 * 该函数完成以下核心操作：
 * 1. 设置监听状态标志位
 * 2. 激活底层套接字的监听模式
 * 3. 启用事件通道的读事件监听功能
 */
void Acceptor::listen()
{
    // 设置服务端监听状态标志
    listenning_ = true;

    // 激活底层socket的监听模式（TCP backlog使用默认值）
    acceptSocket_.listen();

    // 在事件循环中注册读事件监听，开始接收新连接
    acceptChannel_.enableReading();
}

/**
 * @brief 处理新连接请求的回调函数
 *
 * 当监听套接字有可读事件时触发，表示有新客户端连接到达。
 * 主要流程：
 * 1. 接受新连接，获取客户端地址信息
 * 2. 若连接成功，通过回调函数分发新连接
 * 3. 若没有设置回调则立即关闭连接
 * 4. 处理accept可能发生的错误（含EMFILE特殊错误处理）
 */
void Acceptor::handleRead()
{
    // 接受新连接并获取客户端地址信息
    InetAddress peerAddr;
    int coonfd = acceptSocket_.accept(&peerAddr);

    if (coonfd >= 0)
    {
        /* 成功建立连接时的处理逻辑 */
        if (newConnectionCallback_)
        {
            // 将新连接的文件描述符和地址通过回调函数转发
            // 回调函数负责将连接分发到子事件循环
            newConnectionCallback_(coonfd, peerAddr);
        }
        else
        {
            // 未设置回调时直接关闭连接
            close(coonfd);
        }
    }
    else
    {
        /* accept系统调用失败处理 */
        LOG_ERROR("%s:%s:%d accept error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE)
        {
            // 处理文件描述符耗尽的情况
            LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
