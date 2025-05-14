//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_POLLER_H
#define MY_MUDUO_POLLER_H

#include "Channel.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "Timestamp.h"

namespace net
{
    class Channel;
    class EventLoop;

    /**
 * @class Poller
 * @brief I/O 复用机制的抽象基类，提供统一的事件监听接口。
 *
 * Poller 负责封装底层 I/O 复用（如 epoll、poll 等）的实现细节，提供以下核心功能：
 * 1. 通过 poll() 阻塞监听所有注册的文件描述符事件。
 * 2. 管理 Channel 的注册、更新和注销。
 * 3. 将就绪事件分发给对应的 Channel 处理。
 *
 * @note 此类为抽象类，具体实现需由派生类（如 EPollPoller）完成。
 */
    class Poller : NonCopyable
    {
    public:
        using ChannelList = std::vector<Channel *>;
        Poller(EventLoop *loop);
        virtual ~Poller() = default;

        // 开始监听所有注册的 Channel 事件，阻塞等待直到事件就绪或超时
        virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
        // 更新或添加 Channel 的事件监听
        virtual void updateChannel(Channel *channel) = 0;
        // 移除 Channel 的事件监听
        virtual void removeChannel(Channel *channel) = 0;

        // 检查 Channel 是否已注册到当前 Poller
        virtual bool hasChannel(Channel *channel);

        // 创建默认的 Poller 实例（平台相关）
        static Poller *newDefaultPoller(EventLoop *loop);

    protected:
        using ChannelMap = std::unordered_map<int, Channel *>;
        ChannelMap channels_;// 所有注册的 Channel，键为文件描述符（fd），值为对应的 Channel 指针

    private:
        EventLoop *ownerLoop_;// 所属的 EventLoop，用于确保线程安全性
    };
}// namespace net

#endif//MY_MUDUO_POLLER_H
