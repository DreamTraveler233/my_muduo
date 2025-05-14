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

/**
 * @brief TcpConnection构造函数 - 创建TCP连接对象并初始化核心组件
 *
 * @param loop 所属EventLoop事件循环对象指针(不可为空)
 * @param name 连接名称标识符
 * @param sockfd 已建立的socket文件描述符
 * @param localAddr 本地网络地址信息
 * @param peerAddr 对端网络地址信息
 *
 * 初始化流程：
 * 1. 绑定事件循环并验证有效性
 * 2. 初始化socket和channel
 * 3. 配置默认高水位阈值（64MB）
 * 4. 注册Channel的四大事件回调
 */
TcpConnection::TcpConnection(EventLoop *loop,
                             std::string name,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr,
                             std::shared_ptr<thp::ThreadPool> threadPool)
    : loop_(CheckLoopNotNull(loop)),// 强制校验事件循环有效性
      name_(std::move(name)),
      state_(kConnecting),                // 初始连接状态（正在连接）
      reading_(true),                     // 默认启用读事件监听
      socket_(new Socket(sockfd)),        // 封装socket描述符
      channel_(new Channel(loop, sockfd)),// 创建事件通道
      localAddr_(localAddr),              // 存储本地地址
      peerAddr_(peerAddr),                // 存储对端地址
      threadPool_(std::move(threadPool)), // 线程池
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
        loop_->runInLoop(
                [This = shared_from_this(), capture0 = buf.c_str(), capture1 = buf.size()] {
                    This->sendInLoop(capture0, capture1);
                });
    }
}

/**
 * @brief 在事件循环线程中执行实际数据发送（核心发送逻辑）
 *
 * 该函数执行实际的发送操作，处理以下情况：
 * - 直接写入socket的可能性
 * - 部分写入时的缓冲区管理
 * - 错误处理与连接状态管理
 * - 高水位回调触发
 *
 * @param data 待发送数据的起始地址
 * @param len 待发送数据的长度
 *
 * 执行流程：
 * 1. 连接状态验证：若已断开则中止发送
 * 2. 直接发送尝试：当满足条件时尝试直接写入socket
 * 3. 错误处理：处理EWOULDBLOCK及其他致命错误
 * 4. 缓冲区管理：将未发送数据存入outputBuffer_
 * 5. 事件注册：当需要继续发送时启用EPOLLOUT事件监听
 * 6. 高水位控制：当缓冲区超过阈值时触发流量控制回调
 */
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

/**
 * @brief 关闭TCP连接
 *
 * 功能说明：
 * - 当连接处于已连接状态时，启动优雅关闭流程：
 *   1. 将连接状态置为断开中(kDisconnecting)
 *   2. 通过事件循环异步执行底层关闭操作
 *
 * 注意：
 * - 必须通过事件循环线程执行以保证线程安全
 * - 实际关闭操作由shutdownInLoop()在事件循环线程完成
 */
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

/**
 * @brief 在事件循环中安全关闭TCP连接的写端
 *
 * 该函数处理TCP连接写端关闭逻辑，确保在事件循环中无数据发送时才执行关闭。
 * 主要流程：
 * 1. 检查输出缓冲区是否正在发送数据（通过channel_的写状态判断）；
 * 2. 若没有数据在发送，立即关闭socket的SHUT_WR半关闭，停止写入操作；
 * 3. 关闭写端会触发EPOLLHUP事件，进而通过channel_的closeCallback回调
 *    通知上层连接关闭事件，该回调最终指向TcpConnection::handleClose，
 *    使得用户自定义的连接关闭逻辑能被正确执行。
 *
 * @note 必须在IO事件循环线程中调用，避免多线程竞争
 */
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

/**
 * @brief 处理TCP发送缓冲区的数据冲刷与状态机协调
 *
 * 核心设计目标：在非阻塞IO模型下实现可靠的数据传输，平衡吞吐量与资源消耗
 *
 * 关键设计考量：
 * 1. 流量自适应 - 通过动态注册/注销EPOLLOUT事件避免无意义的忙等待（ET模式下持续触发）
 * 2. 优雅关闭保障 - 确保在关闭前完成所有待发送数据的传输
 * 3. 线程模型安全 - 所有IO操作限制在单一IO线程，避免多线程竞态
 */
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

/**
 * @brief 处理TCP连接的关闭流程
 */
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

EventLoop *TcpConnection::getLoop() const { return loop_; }
const std::string &TcpConnection::getName() const { return name_; }
const InetAddress &TcpConnection::getLocalAddress() const { return localAddr_; }
const InetAddress &TcpConnection::getPeerAddress() const { return peerAddr_; }
Buffer *TcpConnection::getInputBuffer() { return &inputBuffer_; }
Buffer *TcpConnection::getOutputBuffer() { return &outputBuffer_; }
bool TcpConnection::isConnected() const { return state_ == kConnected; }
bool TcpConnection::isDisconnected() const { return state_ == kDisconnected; }
std::shared_ptr<thp::ThreadPool> TcpConnection::getThreadPool() { return threadPool_; }
void TcpConnection::setState(TcpConnection::StateE state) { state_ = state; }
void TcpConnection::setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
void TcpConnection::setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
void TcpConnection::setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
void TcpConnection::setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
void TcpConnection::setHighWaterMarkCallback(HighWaterMarkCallback cb, size_t highWaterMark)
{
    highWaterMarkCallback_ = std::move(cb);
    highWaterMark_ = highWaterMark;
}