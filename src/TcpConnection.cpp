//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/my_net/TcpConnection.h"

#include <utility>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, errno);
    }
    return loop;
}

/**
 * @brief TcpConnection构造函数 - 创建TCP连接对象并初始化核心组件
 *
 * @param loop 所属EventLoop事件循环对象指针(不可为空)
 * @param nameArg 连接名称标识符
 * @param sockfd 已建立的socket文件描述符
 * @param localAddr 本地网络地址信息
 * @param peerAddr 对端网络地址信息
 *
 * 初始化流程：
 * 1. 验证事件循环有效性
 * 2. 设置初始状态为kConnecting(连接建立中)
 * 3. 创建socket和channel核心组件
 * 4. 初始化64MB高水位标记(用于流量控制)
 */
TcpConnection::TcpConnection(EventLoop *loop,
                             std::string nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),// 强制校验事件循环有效性
      name_(std::move(nameArg)),
      state_(kConnecting),                // 初始连接状态
      reading_(true),                     // 默认启用读事件监听
      socket_(new Socket(sockfd)),        // 封装socket描述符
      channel_(new Channel(loop, sockfd)),// 创建事件通道
      localAddr_(localAddr),              // 存储本地地址
      peerAddr_(peerAddr),                // 存储对端地址
      highWaterMark_(64 * 1024 * 1024)    // 设置64MB高水位缓冲区限制
{
    // 配置channel的四个核心回调：将网络事件转发到TcpConnection的处理方法
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    // 调试日志记录连接创建信息
    LOG_DEBUG("TcpConnection::ctor[%s] at this fd=%d \n", name_.c_str(), sockfd);

    // 启用TCP keepalive机制保持长连接
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d", name_.c_str(), channel_->getFd(), (int) state_);
}

/**
 * @brief 发送数据到TCP连接
 *
 * 该函数用于将指定的数据发送到当前TCP连接。如果连接状态为已连接（kConnected），
 * 则通过事件循环（loop_）在事件循环线程中调用sendInLoop函数来实际发送数据。
 *
 * @param buf 要发送的数据，类型为std::string，包含待发送的数据内容。
 */
void TcpConnection::send(const std::string &buf)
{
    // 检查当前连接状态是否为已连接
    if (state_ == kConnected)
    {
        // 在事件循环线程中异步调用sendInLoop函数来发送数据
        loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()));
    }
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;     // 已写入的字节数
    size_t remaining = len; // 剩余未发送的字节数
    bool faultError = false;// 是否发生严重错误

    // 如果连接已断开，记录错误并返回
    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    /* 如果是首次调用 send 时，输出缓冲区为空，且未监听可写事件，尝试直接写入数据，
     * 此时直接尝试写入数据，无需等待事件触发，减少延迟
     */
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 尝试直接将数据写入tcp的写缓冲区
        nwrote = write(channel_->getFd(), data, len);

        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            // 既然在这里数据全部发送完毕了，就不用再给channel设置epollout事件了
            // 即不再监听tcp读缓冲区什么时候有空间，如果有注册写完成回调则调用
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else// nwrote < 0
        {
            nwrote = 0;
            // 如果错误不是EWOULDBLOCK，记录错误并根据错误类型设置faultError标志
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
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
        // 如果输出缓冲区中的数据量超过高水位标记且存在高水位回调，则调用回调
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }

        // 将剩余数据追加到输出缓冲区
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);
        // 如果当前没有写操作，则启用写事件监听
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

// 关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}

/**
 * @brief 在事件循环中关闭TCP连接的写端。
 *
 * 该函数用于在事件循环中安全地关闭TCP连接的写端。首先检查是否还有数据正在通过outputBuffer发送，
 * 如果没有数据正在发送，则关闭socket的写端，并触发socket的EPOLLHUP事件，调用channel的closeCallback回调函数，
 * 而channel的closeCallback是由TcpConnection注册的，所以最终调用的是用户设置的TcpConnection::handleClose
 */
void TcpConnection::shutdownInLoop()
{
    // 检查是否还有数据正在通过outputBuffer发送
    if(!channel_->isWriting())
    {
        // 如果没有数据正在发送，则关闭socket的写端
        // 触发socket的EPOLLHUP事件，channel调用closeCallback回调函数
        socket_->shutdownWrite();
    }
}

/**
 * @brief 处理TCP连接建立后的逻辑。
 *
 * 该函数在TCP连接成功建立后被调用，主要完成以下任务：
 * 1. 将连接状态设置为已连接（kConnected）。
 * 2. 将当前连接对象与channel_绑定，确保在channel_事件回调时能够访问到当前连接对象。
 * 3. 启用channel_的读事件监听，以便接收来自对端的数据。
 * 4. 调用用户注册的连接回调函数，通知上层应用连接已建立。
 */
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

/**
 * @brief TcpConnection::connectDestroyed
 *
 * 该函数用于处理TCP连接销毁时的逻辑。主要功能包括：
 * 1. 如果当前连接状态为已连接（kConnected），则将其状态设置为断开连接（kDisconnected），
 *    并禁用与该连接相关的所有事件，最后调用连接回调函数。
 * 2. 无论连接状态如何，都会将该连接的channel从poller中移除。
 */
void TcpConnection::connectDestroyed()
{
    // 如果当前连接状态为已连接，则执行以下操作
    if(state_ == kConnected)
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

/**
 * @brief 处理TCP连接上的读事件
 *
 * 当有数据到达时，从socket中读取数据到输入缓冲区，并根据读取结果
 * 进行相应处理：调用消息回调、处理关闭或错误情况
 *
 * @param receiveTime 数据接收的时间戳，用于传递给上层回调
 */
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &savedErrno);

    // 成功读取数据：调用用户注册的消息回调函数
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    // 客户端主动关闭连接：执行关闭处理流程
    else if (n == 0)
    {
        handleClose();
    }
    // 读取发生错误：记录日志并执行错误处理
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

/**
 * @brief 处理TCP连接的可写事件
 *
 * 当内核发送缓冲区有可用空间时触发该函数，主要完成以下工作：
 * 1. 将应用层输出缓冲区数据写入socket发送缓冲区
 * 2. 管理写事件的注册与注销
 * 3. 处理写完成回调及连接关闭逻辑
 */
void TcpConnection::handleWrite()
{
    // 检查通道是否注册了写事件
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        // 将输出缓冲区数据写入socket文件描述符
        ssize_t n = outputBuffer_.writeFd(channel_->getFd(), &savedErrno);

        // 成功写入数据的情况处理
        if (n > 0)
        {
            // 从缓冲区中移除已成功发送的数据
            outputBuffer_.retrieve(n);

            // 当输出缓冲区数据全部发送完毕时的处理
            if (outputBuffer_.readableBytes() == 0)
            {
                // 停止监听写事件（避免busy loop）
                channel_->disableWriting();

                // 执行写完成回调（如果已设置）
                if (writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
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
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else// 连接已断开时的错误处理
    {
        LOG_ERROR("Connection fd = %d is down, no more writing \n", channel_->getFd());
    }
}

/**
 * @brief 处理TCP连接的关闭流程
 *
 * 该函数在TCP连接需要关闭时被调用，负责执行以下操作：
 * 1. 记录连接状态变更日志
 * 2. 更新连接状态为已断开
 * 3. 关闭底层通道的事件监听
 * 4. 维持连接对象生命周期（防止回调期间被提前析构）
 * 5. 触发连接相关回调函数
 */
void TcpConnection::handleClose()
{
    // 记录连接关闭时的关键信息：文件描述符和当前状态
    LOG_INFO("TcpConnection::handleClose fd = %d state = %d \n", channel_->getFd(), (int) state_);

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

/**
 * @brief 处理TCP连接中的错误信息
 *
 * 该函数用于获取当前套接字上的异步错误状态(SO_ERROR)，通过getsockopt系统调用
 * 获取底层套接字实际发生的错误码，并将错误信息记录到日志中。
 */
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
void TcpConnection::setConnectionCallback(const ConnectionCallback &cb)
{
    connectionCallback_ = cb;
}
void TcpConnection::setMessageCallback(const MessageCallback &cb)
{
    messageCallback_ = cb;
}
void TcpConnection::setWriteCompleteCallback(const WriteCompleteCallback &cb)
{
    writeCompleteCallback_ = cb;
}
void TcpConnection::setCloseCallback(const CloseCallback &cb)
{
    closeCallback_ = cb;
}
void TcpConnection::setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
{
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
}