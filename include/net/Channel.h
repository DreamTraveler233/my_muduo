#ifndef MY_MUDUO_CHANNEL_H
#define MY_MUDUO_CHANNEL_H

#include "EventLoop.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "Timestamp.h"

namespace net
{
    class EventLoop;

    /**
     * 封装单个 fd 的事件管理，负责：
     * 1、注册/更新/移除事件监听（通过 EventLoop）
     * 2、定义事件回调（如读、写、关闭）
     * 3、处理事件并调用用户设置的回调函数
     */
    class Channel : NonCopyable
    {
    public:
        using EventCallback = std::function<void()>;
        using ReadEventCallback = std::function<void(Timestamp)>;

        Channel(EventLoop *loop, int fd);
        ~Channel();

        // 基础信息获取
        [[nodiscard]] int getFd() const;    // 返回绑定的文件描述符
        [[nodiscard]] int getEvents() const;// 返回当前监听的事件类型
        void setRevents(uint32_t revt);     // 设置 Poller 返回的事件类型

        // 事件监听控制
        void enableReading(); // 启用读事件监听（EPOLLIN | EPOLLPRI）
        void disableReading();// 禁用读事件监听
        void enableWriting(); // 启用写事件监听（EPOLLOUT）
        void disableWriting();// 禁用写事件监听
        void disableAll();    // 禁用所有事件监听

        // 事件状态判断
        [[nodiscard]] bool isNoneEvent() const;// 是否未监听任何事件
        [[nodiscard]] bool isReading() const;  // 是否正在监听读事件
        [[nodiscard]] bool isWriting() const;  // 是否正在监听写事件

        // Poller 状态管理
        [[nodiscard]] int getIndex() const;// 返回在 Poller 中的状态索引（-1 表示未注册）
        void setIndex(int index);          // 设置 Poller 中的状态索引

        [[nodiscard]] EventLoop *ownerLoop() const;// 返回所属的 EventLoop
        void remove();                             // 从 EventLoop 中移除当前 Channel

        // 事件处理入口
        void handleEvent(Timestamp receiveTime);

        // 回调函数设置
        void setReadCallback(ReadEventCallback cb);// 设置读事件回调（带时间戳）
        void setWriteCallback(EventCallback cb);   // 设置写事件回调
        void setCloseCallback(EventCallback cb);   // 设置连接关闭回调
        void setErrorCallback(EventCallback cb);   // 设置错误处理回调

        // 资源绑定：防止 Channel 被销毁时回调仍在执行
        void tie(const std::shared_ptr<void> &obj);

    private:
        void update();// 通知 EventLoop 更新当前 Channel 的事件监听

        // 内部事件处理（带生命周期保护）
        void handleEventWithGuard(Timestamp receiveTime);

        // 静态常量：事件类型定义
        static const int kNoneEvent; // 无事件（0）
        static const int kReadEvent; // 读事件（EPOLLIN | EPOLLPRI）
        static const int kWriteEvent;// 写事件（EPOLLOUT）

        EventLoop *loop_; // 所属的 EventLoop，用于事件循环操作
        const int fd_;    // 绑定的文件描述符（如 socket）
        int events_;      // 当前监听的事件掩码（由用户设置）
        uint32_t revents_;// Poller 返回的就绪事件掩码
        int index_;       // 在 Poller 中的状态索引（-1: 未注册，1: 已注册，2: 已删除）

        // 生命周期管理：通过弱引用绑定防止回调时资源失效
        std::weak_ptr<void> tie_;// 绑定的共享资源（如 TcpConnection）
        bool tied_;              // 是否已绑定资源

        // 回调函数
        ReadEventCallback readCallback_;// 读事件回调（参数为事件触发时间）
        EventCallback writeCallback_;   // 写事件回调
        EventCallback closeCallback_;   // 连接关闭回调
        EventCallback errorCallback_;   // 错误处理回调
    };
}// namespace net

#endif//MY_MUDUO_CHANNEL_H
