//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_EPOLLPOLLER_H
#define MY_MUDUO_EPOLLPOLLER_H

#include "Logger.h"
#include "Poller.h"
#include "SysHeadFile.h"

namespace net
{ /**
 * @class EPollPoller
 * @brief 基于 epoll 实现的多路事件分发器，提供高效的可扩展 I/O 事件通知机制。
 *
 * 本类封装 Linux epoll 系统调用，主要职责包括：
 * 1. 通过 epoll_create 创建 epoll 实例，管理 epoll 文件描述符生命周期
 * 2. 使用 epoll_ctl 动态管理注册的事件类型（EPOLL_CTL_ADD/MOD/DEL）
 * 3. 通过 epoll_wait 高效监听事件，将就绪事件分发给 EventLoop 处理
 * 4. 维护文件描述符到 Channel 的映射关系，确保事件回调正确派发
 *
 * 设计特点：
 * - 采用水平触发模式（LT），兼容常规文件描述符和 socket
 * - 事件列表动态扩容机制，避免频繁内存分配
 * - 线程不安全，需确保在所属 EventLoop 线程调用
 */
    class EPollPoller : public Poller
    {
    public:
        // 创建 epoll 实例
        explicit EPollPoller(EventLoop *loop);
        // 关闭 epoll 文件描述符
        ~EPollPoller() override;
        // 核心事件监听接口，封装 epoll_wait
        Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;

        // 更新 Channel 关注的事件类型
        void updateChannel(Channel *channel) override;
        // 从 epoll 实例移除 Channel
        void removeChannel(Channel *channel) override;

    private:
        // 初始事件列表大小（可动态扩容）
        static const int kInitEventListSize = 16;

        using ReventList = std::vector<epoll_event>;

        // 将 epoll_wait 结果转换为活跃 Channel 列表
        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
        // 执行 epoll_ctl 操作
        void update(int operation, Channel *channel) const;

        int epollFd_;      // epoll 实例的文件描述符
        ReventList events_;// epoll_wait 返回的事件列表（可动态扩容）
    };
}// namespace net

#endif//MY_MUDUO_EPOLLPOLLER_H
