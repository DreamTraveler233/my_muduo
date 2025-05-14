//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/net/EPollPoller.h"

using namespace net;

// Channel 状态常量定义
const int kNew = -1;   // 初始状态，未加入 epoll 监控
const int kAdded = 1;  // 已加入 epoll 监控
const int kDeleted = 2;// 已从 epoll 监控移除但保留在 Poller 中

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),                          // 初始化基类 Poller
      epollFd_(epoll_create1(EPOLL_CLOEXEC)),// 创建 epoll 实例并设置 close-on-exec，防止未关闭的 epoll 描述符可能被继承到新程序
      events_(kInitEventListSize)            // 预分配事件数组空间
{
    // 创建失败
    if (epollFd_ < 0)
    {
        LOG_FATAL("%s:%s:%d epoll_create error:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
}

EPollPoller::~EPollPoller() { close(epollFd_); }

Timestamp EPollPoller::poll(int timeoutMs, Poller::ChannelList *activeChannels)
{
    // 打印当前管理的文件描述符总数（监控负载情况）
    LOG_DEBUG("%s => fd total count:%zu \n", __FUNCTION__, channels_.size());

    // 执行 epoll_wait 系统调用（核心阻塞点）
    int numEvents = epoll_wait(
            epollFd_,                        // epoll 实例描述符
            events_.data(),                  // 事件数组起始地址（C++11写法）
            static_cast<int>(events_.size()),// 最大可接收事件数（注意类型转换）
            timeoutMs                        // 超时时间（-1 阻塞，0 非阻塞，>0 毫秒超时）
    );

    // 立即保存 errno（后续操作可能修改该值）
    int saveErrno = errno;

    // 处理有事件就绪的情况
    if (numEvents > 0)
    {
        LOG_DEBUG("revents num is %d \n", numEvents);// 记录事件触发数量

        // 将就绪事件转换为活跃 Channel 列表
        fillActiveChannels(numEvents, activeChannels);

        // 动态扩容优化：如果本次事件填满数组，则双倍扩容避免下次溢出
        if (static_cast<size_t>(numEvents) == events_.size())
        {
            events_.resize(events_.size() * 2);// 典型 vector 扩容策略
            LOG_DEBUG("Epoll event list expanded to %zd", events_.size());
        }
    }
    // 处理超时情况（无事件就绪）
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout \n", __FUNCTION__);// 记录正常超时
    }
    // 处理错误情况
    else
    {
        // 仅处理非中断引起的错误（EINTR 为正常中断无需报警）
        if (saveErrno != EINTR)
        {
            // 还原 errno 并记录错误（保证线程安全）
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll error: %s", strerror(saveErrno));
        }
    }

    // 返回当前时间戳
    return Timestamp::now();
}

void EPollPoller::fillActiveChannels(int numEvents, Poller::ChannelList *activeChannels) const
{
    // 遍历epoll_wait返回的所有就绪事件（事件数据由poll()阶段收集）
    for (int i = 0; i < numEvents; ++i)
    {
        // 从事件结构体直接提取关联的Channel对象（关键性能优化点）
        // 注：该指针由update()操作时通过event.data.ptr设置
        auto *channel = static_cast<Channel *>(events_[i].data.ptr);

        // 将内核检测到的事件类型回写至Channel（如EPOLLIN/EPOLLOUT）
        // 后续EventLoop将根据revents执行对应回调
        channel->setRevents(events_[i].events);

        // 将就绪Channel加入待处理列表（注意：这里不处理事件，仅收集状态）
        // 调用方需保证在EventLoop线程中消费activeChannels
        activeChannels->push_back(channel);
    }
}

void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->getIndex();
    const int fd = channel->getFd();

    // 打印调试信息：文件描述符、关注事件、当前状态
    LOG_INFO("Updating channel fd=%d events=%d status=%s",
             fd, channel->getEvents(),
             (index == kNew) ? "New" : (index == kAdded) ? "Added"
                                                         : "Deleted");

    // 处理需要 ADD 操作的两种情况：
    // 1. 新 Channel (从未添加过)
    // 2. 之前被删除但保留在channel_中的 Channel (需要重新添加)
    if (index == kNew || index == kDeleted)
    {
        // 如果是全新 Channel
        if (index == kNew)
        {
            // 防御重复注册
            if (channels_.find(fd) != channels_.end())
            {
                LOG_ERROR("Duplicate channel fd=%d", fd);
                return;
            }

            channels_[fd] = channel;// 注册到映射表
        }
        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);// 加入 epoll 监控
    }
    else// 处理已存在的 Channel (kAdded 状态)
    {
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);// 无关注事件则移除监控
            channel->setIndex(kDeleted);   // 更新状态为已删除（但仍保留在 channels_ 中）
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);// 更新事件监听类型
        }
    }
}

void EPollPoller::removeChannel(Channel *channel)
{
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, channel->getFd());

    // 从映射表中移除（无论当前状态如何都立即执行）
    const int fd = channel->getFd();
    channels_.erase(fd);// 如果 fd 不存在则 erase 无副作用

    // 获取当前状态以决定是否需要执行 epoll_ctl(DEL)
    int index = channel->getIndex();

    // 仅当 Channel 处于已添加状态时才需要执行 DEL 操作
    if (index == kAdded)
    {
        // 核心操作：调用 epoll_ctl(EPOLL_CTL_DEL)
        update(EPOLL_CTL_DEL, channel);
    }

    // 强制重置状态为初始值（重要！使 Channel 可复用）
    channel->setIndex(kNew);// 现在该 Channel 可被重新添加到其他 Poller
}

void EPollPoller::update(int operation, Channel *channel) const
{
    epoll_event event{};
    const int fd = channel->getFd();    // 需确保Channel生命周期有效
    event.events = channel->getEvents();// 从Channel提取当前关注的事件掩码
    event.data.ptr = channel;           // 关键设计：通过指针实现事件到对象的反向关联

    // 执行 epoll_ctl 系统调用
    if (epoll_ctl(epollFd_, operation, fd, &event) < 0)
    {
        // 错误处理：根据操作类型区分日志级别
        if (operation == EPOLL_CTL_DEL)
        {
            // DEL 操作错误通常可容忍（例如描述符已关闭）
            LOG_ERROR("epoll_ctl del error:%d \n", errno);// 记录错误但继续运行
        }
        else
        {
            // ADD/MOD 操作失败是严重错误（可能导致事件丢失）
            LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);// 终止程序
        }
    }
}