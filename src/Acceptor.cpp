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
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),// 创建套接字
      acceptChannel_(loop, acceptSocket_.getFd()),
      listenning_(false)
{
    // 设置套接字选项：地址重用+端口重用策略
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);

    // 将套接字绑定到指定网络地址
    acceptSocket_.bind(listenAddr);

    // 设置读事件回调，当监听套接字可读时（有新连接）触发handleRead
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

/**
 * @brief Acceptor类的析构函数，负责资源清理工作
 */
Acceptor::~Acceptor()
{
    // 禁用acceptChannel_上的所有事件监听（如读/写/错误事件）
    acceptChannel_.disableAll();
    // 从事件循环（mainLoop）中完全移除该channel，防止后续事件触发
    acceptChannel_.remove();
}

/**
 * @brief 启动服务器监听功能
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
 * @brief 处理新连接到达的读事件回调函数
 *
 * 该函数在网络监听套接字可读时被触发，执行accept操作接收新连接，
 * 并根据是否设置回调函数进行后续处理。包含完整的错误处理逻辑，
 * 特别注意处理EMFILE(进程文件描述符耗尽)等关键错误情况。
 *
 * @note 本函数没有参数和返回值，属于类成员函数
 */
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);

    // 成功接收新连接时的处理流程
    if (connfd >= 0)
    {
        /* 检查是否设置了新连接回调函数
         * - 若已设置：将新连接交给上层处理
         * - 未设置：立即关闭连接并记录错误日志
         */
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            close(connfd);
            LOG_ERROR("No connection callback set, closing fd: %d", connfd);
        }
    }
    // 处理accept失败的各种错误情况
    else
    {
        /* 错误处理优先级：
         * 1. 非阻塞模式下的正常返回（EAGAIN/EWOULDBLOCK）
         * 2. 进程文件描述符耗尽（EMFILE）
         * 3. 可恢复的系统中断（EINTR）或客户端中止（ECONNABORTED）
         * 4. 其他未知错误
         */
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return;
        }
        else if (errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d Too many open files", __FILE__, __FUNCTION__, __LINE__);
        }
        else if (errno == EINTR || errno == ECONNABORTED)
        {
            LOG_DEBUG("Accept error: %s (errno=%d)", strerror(errno), errno);
        }
        else
        {
            LOG_ERROR("%s:%s:%d accept error:%d", __FILE__, __FUNCTION__, __LINE__, errno);
        }
    }
}

void Acceptor::setNewConnectionCallback(Acceptor::NewConnectionCallback cb) { newConnectionCallback_ = std::move(cb); }
bool Acceptor::getListenning() const { return listenning_; }