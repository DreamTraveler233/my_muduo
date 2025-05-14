//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/net/TcpConnection.h"

using namespace net;

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, errno);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             std::string name,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),// 强制校验事件循环有效性
      name_(std::move(name)),
      state_(kConnecting),                // 初始连接状态（正在连接）
      reading_(true),                     // 默认启用读事件监听
      socket_(new Socket(sockfd)),        // 封装socket描述符
      channel_(new Channel(loop, sockfd)),// 创建事件通道
      localAddr_(localAddr),              // 存储本地地址
      peerAddr_(peerAddr),                // 存储对端地址
      highWaterMark_(64 * 1024 * 1024)    // 设置64MB高水位缓冲区限制
{
    // 配置channel的四个核心回调：将网络事件转发到TcpConnection的处理方法
    channel_->setReadCallback([this](auto &&PH1) { handleRead(std::forward<decltype(PH1)>(PH1)); });
    channel_->setWriteCallback([this] { handleWrite(); });
    channel_->setCloseCallback([this] { handleClose(); });
    channel_->setErrorCallback([this] { handleError(); });

    // 调试日志记录连接创建信息
    LOG_DEBUG("TcpConnection::ctor[%s] at this fd=%d \n", name_.c_str(), sockfd);

    // 启用TCP keepalive机制保持长连接
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d", name_.c_str(), channel_->getFd(), static_cast<int>(state_));
}

void TcpConnection::send(const std::string &buf)
{
    // 检查当前连接状态是否为已连接
    if (state_ == kConnected)
    {
        // 在事件循环线程中异步调用sendInLoop函数来发送数据
        loop_->runInLoop(
                [This = shared_from_this(), capture0 = buf.c_str(), capture1 = buf.size()] {
                    This->sendInLoop(capture0, capture1);
                });
    }
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;     // 实际写入socket的字节数
    size_t remaining = len; // 剩余待发送字节数
    bool faultError = false;// 致命错误标志（EPIPE/ECONNRESET）

    // 前置状态检查：确保连接尚未断开
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    /* 直接写入优化路径：当满足以下条件时尝试直接写入socket
     * 1. 输出缓冲区为空（没有待发送的遗留数据）
     * 2. 未注册写事件监听（说明之前没有发送阻塞的情况）
     */
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 尝试非阻塞写入（可能部分成功）
        nwrote = write(channel_->getFd(), data, len);

        if (nwrote >= 0)// 成功写入部分或全部数据
        {
            remaining = len - nwrote;
            // 既然在这里数据全部发送完毕了，就不用再给channel设置handleWrite()事件了
            // 即不再监听tcp读缓冲区什么时候有空间，如果有注册写完成回调则调用
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 延迟触发写完成回调（确保回调在IO线程执行）
                loop_->queueInLoop([This = shared_from_this()] {
                    This->writeCompleteCallback_(This);
                });
            }
        }
        else// 写入出错处理
        {
            nwrote = 0;
            // 如果错误不是EWOULDBLOCK，记录错误并根据错误类型设置faultError标志
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("%s : %d : %s", __FILE__, __LINE__, __FUNCTION__);
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    // 如果没有发生严重错误且仍有剩余数据未发送，将数据追加到输出缓冲区并启用写事件监听
    // 如果poller发现tcp的发送缓冲区有空间了，会通知相应的channel，然后调用handleWrite方法
    // 最终也就是调用TcpConnection::handleWrite()方法，即将输出缓冲区数据写入tcp写缓冲区，直至输出缓冲区中的数据全部发送完成
    if (!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();// 目前发送缓冲区中剩余的待发送数据的长度
        // 高水位检测：当前缓冲区大小+新数据是否超过阈值
        if (oldLen + remaining >= highWaterMark_ &&
            oldLen < highWaterMark_ &&
            highWaterMarkCallback_)
        {
            // 触发高水位回调（流量控制通知），该高水位回调函数由用户设置
            // 当输出缓冲区超过阈值时，通知应用层暂停发送数据，避免缓冲区无限膨胀（如内存耗尽）
            loop_->queueInLoop([This = shared_from_this(), val = oldLen + remaining] {
                This->highWaterMarkCallback_(This, val);
            });
        }

        // 数据追加到输出缓冲区
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);

        // 注册写事件监听（当内核发送缓冲区可用时触发handleWrite）
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);

        loop_->runInLoop([This = shared_from_this()] {
            This->shutdownInLoop();
        });
    }
}

void TcpConnection::shutdownInLoop()
{
    // 关键条件判断：仅当输出通道无待发送数据时才能立即关闭写端
    if (!channel_->isWriting())
    {
        // 执行半关闭操作，触发后续连接关闭事件链
        // shutdownWrite()将发送FIN包，通知对端不再发送数据
        // 同时使epoll监听到EPOLLHUP事件，激活closeCallback
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished()
{
    // 将连接状态设置为已连接
    setState(kConnected);

    // 将当前连接对象与channel_绑定，确保在channel_事件回调时能够访问到当前连接对象
    // 因为TcpConnection对象是暴露给用户的，所以得保障TcpConnection对象的生命周期
    channel_->tie(shared_from_this());

    // 启用channel_的读事件监听，以便接收来自对端的数据
    channel_->enableReading();

    // 调用用户注册的连接回调函数，通知上层应用连接已建立
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    // 如果当前连接状态为已连接，则执行以下操作
    if (state_ == kConnected)
    {
        // 将连接状态设置为断开连接
        setState(kDisconnected);

        // 禁用与该连接相关的所有事件
        channel_->disableAll();

        // 调用连接回调函数，通知上层连接已销毁
        connectionCallback_(shared_from_this());
    }

    // 将该连接的channel从poller中移除
    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    // 从fd的读缓冲区中读取数据到用户的读缓冲区中
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &savedErrno);

    if (n > 0)// 成功读取数据：调用用户注册的消息回调函数
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)// 客户端主动关闭连接：执行关闭处理流程
    {
        handleClose();
    }
    else// 读取发生错误：记录日志并执行用户设置的错误处理
    {
        errno = savedErrno;
        LOG_ERROR("%s : %d : %s", __FILE__, __LINE__, __FUNCTION__);
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    // 检查通道是否注册了写事件
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        // 非阻塞写入：将输出缓冲区数据尽可能多地写入socket
        ssize_t n = outputBuffer_.writeFd(channel_->getFd(), &savedErrno);

        // 成功写入数据的处理流程
        if (n > 0)
        {
            // 更新缓冲区状态，移动读指针以释放已发送数据占用的空间
            outputBuffer_.retrieve(n);

            // 当输出缓冲区数据全部发送完毕时的处理
            if (outputBuffer_.readableBytes() == 0)
            {
                // 停止监听写事件（避免busy loop）
                channel_->disableWriting();

                // 执行写完成回调（如果已设置），表示用户写缓冲区有空间了
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop([This = shared_from_this()] {
                        This->writeCompleteCallback_(This);
                    });
                }

                // 当outputBuffer中的数据全部发送完毕时，如果连接状态为kDisconnecting则表示在某处已经调用了shutdown，
                // 但是因为outputBuffer中还有数据未发送，未调用shutdownInLoop()，这里重新调用
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else// 写入失败处理
        {
            LOG_ERROR("%s : %d : %s", __FILE__, __LINE__, __FUNCTION__);
        }
    }
    else// 连接已断开时的错误处理
    {
        LOG_ERROR("Connection fd = %d is down, no more writing \n", channel_->getFd());
    }
}

void TcpConnection::handleClose()
{
    // 记录连接关闭时的关键信息：文件描述符和当前状态
    LOG_DEBUG("%s fd = %d state = %d \n", __FUNCTION__ channel_->getFd(), (int) state_);

    // 将连接状态标记为已断开
    setState(kDisconnected);

    // 禁用通道上的所有事件监听（读写事件等）
    channel_->disableAll();

    /* 创建智能指针保持对象生命周期：
     * 1. 使用shared_from_this()保证在回调执行期间对象不会被销毁
     * 2. 避免在回调链执行过程中出现悬空指针
     */
    TcpConnectionPtr guardThis(shared_from_this());

    // 触发用户设置的上层连接回调（通知连接状态变化）
    connectionCallback_(guardThis);

    // 关闭连接的回调函数，执行的是TcpServer给的TcpServer::removeConnection
    closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;

    // 通过getsockopt获取套接字错误状态，优先获取SO_ERROR选项值
    // 如果系统调用失败则取errno作为错误码
    if (getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }

    // 记录包含连接名称和具体错误码的日志信息
    LOG_ERROR("TcpConnection::handleError name [%s] - SO_ERROR = %d \n", name_.c_str(), err);
}

EventLoop *TcpConnection::getLoop() const
{
    return loop_;
}

const std::string &TcpConnection::getName() const
{
    return name_;
}

const InetAddress &TcpConnection::getLocalAddress() const
{
    return localAddr_;
}

const InetAddress &TcpConnection::getPeerAddress() const
{
    return peerAddr_;
}

Buffer *TcpConnection::getInputBuffer()
{
    return &inputBuffer_;
}

Buffer *TcpConnection::getOutputBuffer()
{
    return &outputBuffer_;
}

bool TcpConnection::isConnected() const
{
    return state_ == kConnected;
}

bool TcpConnection::isDisconnected() const
{
    return state_ == kDisconnected;
}

void TcpConnection::setState(TcpConnection::StateE state)
{
    state_ = state;
}

void TcpConnection::setConnectionCallback(ConnectionCallback cb)
{
    connectionCallback_ = std::move(cb);
}
void TcpConnection::setMessageCallback(MessageCallback cb)
{
    messageCallback_ = std::move(cb);
}
void TcpConnection::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    writeCompleteCallback_ = std::move(cb);
}
void TcpConnection::setCloseCallback(CloseCallback cb)
{
    closeCallback_ = std::move(cb);
}
void TcpConnection::setHighWaterMarkCallback(HighWaterMarkCallback cb, size_t highWaterMark)
{
    highWaterMarkCallback_ = std::move(cb);
    highWaterMark_ = highWaterMark;
}