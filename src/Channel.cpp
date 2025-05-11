//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/my_net/Channel.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false)
{}

Channel::~Channel()
{
    remove();// 自动清理
}

// 当一个TcpConnection新连接创建的时候，会调用TcpConnection::connectEstablished()，
// 然后通过channel_->tie(shared_from_this())，将weak_ptr绑定该TcpConnection连接
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;  // 绑定 weak_ptr
    tied_ = true;// 标记已绑定
}

/**
 * @brief 更新当前Channel的事件注册状态
 *
 * 该函数将当前 Channel 的事件监听状态同步到 EventLoop 的 Poller 中
 * 当 Channel 监听的事件类型（如可读、可写）发生变化时，
 * 必须调用此函数以更新 Poller 的监听设置
 */
void Channel::update()
{
    // 通过 EventLoop 调用 Poller 的更新接口
    loop_->updateChannel(this);
}

/**
 * @brief 从所属EventLoop中移除当前Channel对象
 *
 * 该函数用于从 EventLoop 的 Poller 中注销当前 Channel 的事件监听。
 * 通常在 Channel 销毁或不再需要监听事件时调用，以释放资源
 *
 * @note 本函数应在Channel所属的EventLoop线程中调用，保证线程安全。
 * 函数执行后该Channel对象将不再接收任何事件通知，但不会销毁对象本身。
 */
void Channel::remove()
{
    // 在Channel所属的EventLoop中，删除当前Channel
    loop_->removeChannel(this);
}

/**
 * @brief 处理事件（带生命周期保护）
 * @param receiveTime 事件触发的时间戳（通常由 Poller 提供）
 * @details 若绑定了共享资源（tie_），则先尝试提升为强引用，确保处理期间资源有效。
 *
 * 当连接建立时，会调用TcpConnection::connectEstablished()，
 * 会将TcpConnection通过channel_->tie()方法，
 * 使TcpConnection被weak_ptr管理。
 */
void Channel::handleEvent(Timestamp receiveTime)
{
    if (tied_)// 检查是否绑定了共享资源（TcpConnection）
    {
        // 尝试将 weak_ptr 提升为 shared_ptr（强引用），判断该资源是否还有效（没有被释放）
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)// 提升成功，资源有效
        {
            handleEventWithGuard(receiveTime);// 安全处理事件
        }
        // 提升失败则跳过（资源已销毁）
    }
    else// 未绑定共享资源
    {
        handleEventWithGuard(receiveTime);// 直接处理事件
    }
}

/**
 * @brief 实际处理事件的内部函数
 * @param receiveTime 事件触发的时间戳
 * @details 按优先级处理事件：错误 > 挂起 > 读 > 写。
 * 每个事件分支独立处理，确保所有触发的事件都能被响应。
 */
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent returnEvent:%d\n", revents_);

    // 1. 处理错误事件（EPOLLERR 优先级最高）
    // EPOLLERR 表示文件描述符发生了错误
    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
        else
        {
            LOG_DEBUG("No error callback set for fd %d", fd_);
        }
    }

    // 2. 处理挂起事件（当对端关闭连接，且接收缓冲区没有没有可读数据时才调用）
    // EPOLLHUP 表示连接完全挂起（双方均不可读写）
    // 条件 !(revents_ & EPOLLIN) 确保没有剩余数据可读
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
        else
        {
            LOG_DEBUG("No close callback set for fd %d", fd_);
        }
    }

    // 3. 处理读事件（EPOLLIN | EPOLLPRI | EPOLLRDHUP）
    // EPOLLIN：有普通数据可读
    // EPOLLPRI：有紧急数据（带外数据，如 TCP 的 MSG_OOB）
    // EPOLLRDHUP：对端关闭了写方向（半关闭），本地仍可发送数据
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
        else
        {
            LOG_DEBUG("No read callback set for fd %d", fd_);
        }
    }

    // 4. 处理写事件（EPOLLOUT）
    // EPOLLOUT 表示文件描述符可写
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
        else
        {
            LOG_DEBUG("No write callback set for fd %d", fd_);
        }
    }
}
void Channel::setReadCallback(Channel::ReadEventCallback cb) { readCallback_ = std::move(cb); }
void Channel::setWriteCallback(Channel::EventCallback cb) { writeCallback_ = std::move(cb); }
void Channel::setCloseCallback(Channel::EventCallback cb) { closeCallback_ = std::move(cb); }
void Channel::setErrorCallback(Channel::EventCallback cb) { errorCallback_ = std::move(cb); }
int Channel::getFd() const { return fd_; }
int Channel::getEvents() const { return events_; }
void Channel::setRevents(uint32_t revt) { revents_ = revt; }
bool Channel::isNoneEvent() const { return events_ == kNoneEvent; }
bool Channel::isReading() const { return events_ & kReadEvent; }
bool Channel::isWriting() const { return events_ & kWriteEvent; }
int Channel::getIndex() const { return index_; }
void Channel::setIndex(int index) { index_ = index; }
EventLoop *Channel::ownerLoop() const { return loop_; }
void Channel::enableReading()
{
    events_ |= kReadEvent;
    update();
}
void Channel::disableReading()
{
    events_ &= ~kReadEvent;
    update();
}
void Channel::enableWriting()
{
    events_ |= kWriteEvent;
    update();
}
void Channel::disableWriting()
{
    events_ &= ~kWriteEvent;
    update();
}
void Channel::disableAll()
{
    events_ &= kNoneEvent;
    update();
}