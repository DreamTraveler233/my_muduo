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
     * @brief I/O 复用机制的抽象基类，提供统一的事件监听接口
     *
     * Poller 负责封装底层 I/O 复用（如 epoll、poll 等）的实现细节，提供以下核心功能：
     * - 通过 [poll()] 阻塞监听所有注册的文件描述符事件
     * - 管理 [Channel] 的注册、更新和注销
     * - 将就绪事件分发给对应的 [Channel] 处理
     *
     * @note 此类为抽象类，具体实现需由派生类（如 [EPollPoller]）完成
     */
    class Poller : NonCopyable
    {
    public:
        using ChannelList = std::vector<Channel *>;///< 定义事件通道列表类型

        /**
         * @brief 构造函数，初始化 Poller
         * @param loop 所属的 [EventLoop] 对象
         */
        Poller(EventLoop *loop);

        /**
         * @brief 虚析构函数，确保派生类正确析构
         */
        virtual ~Poller() = default;

        /**
         * @brief 开始监听所有注册的 [Channel] 事件，阻塞等待直到事件就绪或超时
         * @param timeoutMs 超时时间（毫秒）
         * @param activeChannels 用于存储活跃的 [Channel] 列表
         * @return 返回事件发生的时间戳
         */
        virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

        /**
         * @brief 更新或添加 [Channel] 的事件监听
         * @param channel 需要更新的 [Channel] 对象
         */
        virtual void updateChannel(Channel *channel) = 0;

        /**
         * @brief 移除 [Channel] 的事件监听
         * @param channel 需要移除的 [Channel] 对象
         */
        virtual void removeChannel(Channel *channel) = 0;

        /**
         * @brief 检查 [Channel] 是否已注册到当前 Poller
         * @param channel 需要检查的 [Channel] 对象
         * @return 如果 [Channel] 已注册，返回 true；否则返回 false
         */
        virtual bool hasChannel(Channel *channel);

        /**
         * @brief 创建默认的 Poller 实例（平台相关）
         * @param loop 所属的 [EventLoop] 对象
         * @return 返回 Poller 实例指针
         */
        static Poller *newDefaultPoller(EventLoop *loop);

    protected:
        using ChannelMap = std::unordered_map<int, Channel *>;
        ChannelMap channels_;//!< 所有注册的 [Channel]，键为文件描述符（fd），值为对应的 [Channel] 指针

    private:
        EventLoop *ownerLoop_;//!< 所属的 [EventLoop]，用于确保线程安全性
    };
}// namespace net

#endif//MY_MUDUO_POLLER_H
