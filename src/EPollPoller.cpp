//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/my_net/EPollPoller.h"

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

EPollPoller::~EPollPoller()
{
    close(epollFd_);// 确保关闭 epoll 描述符
}

/**
 * @brief 执行一次 epoll_wait 调用，获取就绪事件并填充活跃通道列表
 *
 * @param timeoutMs epoll_wait 超时时间（毫秒）
 * @param activeChannels 输出参数，用于存储就绪事件对应的 Channel 列表
 * @return Timestamp 返回调用完成时的时间戳（通常用于记录事件触发时间）
 *
 * 功能说明:
 * - 通过 epoll_wait 监听所有注册的文件描述符事件
 * - 处理三种结果情况：有事件就绪/超时/错误
 * - 动态调整事件存储数组大小以提升性能
 * - 记录关键节点日志用于调试和状态监控
 */
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

/**
 * @brief 将epoll监听到的就绪事件转换为活跃Channel列表
 *
 * @param numEvents  从poll()中epoll_wait获取的就绪事件总数
 * @param activeChannels 输出参数，存储待处理的活跃Channel（由EventLoop消费）
 *
 * @note 与poll()的协作流程：
 * 1. 事件收集阶段 - poll()通过epoll_wait获取内核事件到events_数组
 * 2. 事件转换阶段 - 本函数遍历events_数组前numEvents项（关键数据流动）
 * 3. 事件消费阶段 - activeChannels传递给EventLoop执行回调
 *
 * 流程可视化：
 * poll()收集事件 → fillActiveChannels转换 → EventLoop消费
 *
 * 设计要点：
 * - 直接通过epoll_event.data.ptr获取Channel（零拷贝优化，避免哈希查找）
 * - 必须紧随poll()调用后执行，保证events_数组数据有效性
 * - 时间复杂度严格为O(numEvents)，与注册的fd总数无关
 */
void EPollPoller::fillActiveChannels(int numEvents, Poller::ChannelList *activeChannels) const
{
    // 遍历epoll_wait返回的所有就绪事件（事件数据由poll()阶段收集）
    for (int i = 0; i < numEvents; ++i)
    {
        // 从事件结构体直接提取关联的Channel对象（关键性能优化点）
        // 注：该指针由update()操作时通过event.data.ptr设置
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);

        // 将内核检测到的事件类型回写至Channel（如EPOLLIN/EPOLLOUT）
        // 后续EventLoop将根据revents执行对应回调
        channel->setRevents(events_[i].events);

        // 将就绪Channel加入待处理列表（注意：这里不处理事件，仅收集状态）
        // 调用方需保证在EventLoop线程中消费activeChannels
        activeChannels->push_back(channel);
    }
}

/**
 * @brief 更新 Channel 的事件监控状态
 *
 * @param channel 目标 Channel 对象，需包含有效 fd 和事件信息
 *
 * 状态转换逻辑：
 * [New/Deleted] -> 添加事件监控 (EPOLL_CTL_ADD)
 * [Added]       -> 无事件时移除监控 (EPOLL_CTL_DEL)
 *                -> 有事件时更新监控 (EPOLL_CTL_MOD)
 *
 * 关键保证：
 * - 维护 fd 与 Channel 的映射关系
 * - 自动处理 epoll 事件集的增/删/改
 * - 防御重复添加和无效操作
 */
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

/**
 * @brief 从 epoll 实例和 Poller 中移除指定 Channel
 *
 * @param channel 要移除的 Channel 对象指针，必须非空且有效
 *
 * 功能说明:
 * - 将 Channel 从 channels_ 映射表中移除，解除 fd 与 Channel 的关联
 * - 根据 Channel 当前状态决定是否需要执行 epoll_ctl(DEL) 操作
 * - 重置 Channel 的状态为 kNew，使其可被重新添加到其他 Poller 或复用
 *
 * 关键流程:
 * 1. 清理映射关系 -> 2. 执行系统调用 -> 3. 重置状态
 */
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

/**
 * @brief 执行 epoll_ctl 系统调用的核心封装函数
 *
 * @param operation 操作类型: EPOLL_CTL_ADD/EPOLL_CTL_MOD/EPOLL_CTL_DEL
 * @param channel 关联的 Channel 对象，包含要操作的 fd 和事件
 *
 * 功能说明:
 * - 构造 epoll_event 结构体并执行指定的 epoll_ctl 操作
 * - 统一处理系统调用错误，区分 DEL 和其他操作的错误级别
 * - 维护 epoll 实例与文件描述符事件监听的同步
 *
 * 关键流程:
 * 1. 构造事件结构 -> 2. 执行系统调用 -> 3. 错误处理
 */
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