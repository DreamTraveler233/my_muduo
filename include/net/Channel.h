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
     * @brief 封装单个文件描述符（fd）的事件管理。
     *
     * 该类负责：
     * 1. 注册/更新/移除事件监听（通过 `EventLoop`）。
     * 2. 定义事件回调（如读、写、关闭）。
     * 3. 处理事件并调用用户设置的回调函数。
     */
    class Channel : NonCopyable
    {
    public:
        using EventCallback = std::function<void()>;             //!< 事件回调函数类型
        using ReadEventCallback = std::function<void(Timestamp)>;//!< 读事件回调函数类型（带时间戳）

        /**
         * @brief 构造函数。
         * @param loop 所属的 [EventLoop]。
         * @param fd 绑定的文件描述符。
         */
        Channel(EventLoop *loop, int fd);

        /// 析构函数。
        ~Channel();

        /**
         * @brief 获取绑定的文件描述符。
         * @return 绑定的文件描述符。
         */
        [[nodiscard]] int getFd() const;

        /**
         * @brief 获取当前监听的事件类型。
         * @return 当前监听的事件类型。
         */
        [[nodiscard]] int getEvents() const;

        /**
         * @brief 设置 Poller 返回的事件类型。
         * @param revt Poller 返回的事件类型。
         */
        void setRevents(uint32_t revt);

        /**
         * @brief 启用读事件监听（`EPOLLIN | EPOLLPRI`）。
         */
        void enableReading();

        /**
         * @brief 禁用读事件监听。
         */
        void disableReading();

        /**
         * @brief 启用写事件监听（`EPOLLOUT`）。
         */
        void enableWriting();

        /**
         * @brief 禁用写事件监听。
         */
        void disableWriting();

        /**
         * @brief 禁用所有事件监听。
         */
        void disableAll();

        /**
         * @brief 判断是否未监听任何事件。
         * @return 如果未监听任何事件，返回 `true`；否则返回 `false`。
         */
        [[nodiscard]] bool isNoneEvent() const;

        /**
         * @brief 判断是否正在监听读事件。
         * @return 如果正在监听读事件，返回 `true`；否则返回 `false`。
         */
        [[nodiscard]] bool isReading() const;

        /**
         * @brief 判断是否正在监听写事件。
         * @return 如果正在监听写事件，返回 `true`；否则返回 `false`。
         */
        [[nodiscard]] bool isWriting() const;

        /**
         * @brief 获取在 Poller 中的状态索引。
         * @return 在 Poller 中的状态索引（-1 表示未注册）。
         */
        [[nodiscard]] int getIndex() const;

        /**
         * @brief 设置 Poller 中的状态索引。
         * @param index Poller 中的状态索引。
         */
        void setIndex(int index);

        /**
         * @brief 获取所属的 [EventLoop]。
         * @return 所属的 [EventLoop]。
         */
        [[nodiscard]] EventLoop *ownerLoop() const;

        /**
         * @brief 从 [EventLoop] 中移除当前 [Channel]。
         */
        void remove();

        /**
         * @brief 事件处理入口。
         * @param receiveTime 事件触发的时间戳。
         */
        void handleEvent(Timestamp receiveTime);

        /**
         * @brief 设置读事件回调（带时间戳）。
         * @param cb 读事件回调函数。
         */
        void setReadCallback(ReadEventCallback cb);

        /**
         * @brief 设置写事件回调。
         * @param cb 写事件回调函数。
         */
        void setWriteCallback(EventCallback cb);

        /**
         * @brief 设置连接关闭回调。
         * @param cb 连接关闭回调函数。
         */
        void setCloseCallback(EventCallback cb);

        /**
         * @brief 设置错误处理回调。
         * @param cb 错误处理回调函数。
         */
        void setErrorCallback(EventCallback cb);

        /**
         * @brief 绑定资源，防止 [Channel] 被销毁时回调仍在执行。
         * @param obj 绑定的共享资源（如 [TcpConnection]）。
         */
        void tie(const std::shared_ptr<void> &obj);

    private:
        /**
         * @brief 通知 [EventLoop] 更新当前 [Channel] 的事件监听。
         */
        void update();

        /**
         * @brief 内部事件处理（带生命周期保护）。
         * @param receiveTime 事件触发的时间戳。
         */
        void handleEventWithGuard(Timestamp receiveTime);

        static const int kNoneEvent; //!< 无事件（0）
        static const int kReadEvent; //!< 读事件（`EPOLLIN | EPOLLPRI`）
        static const int kWriteEvent;//!< 写事件（`EPOLLOUT`）

        EventLoop *loop_; //!< 所属的 [EventLoop]，用于事件循环操作
        const int fd_;    //!< 绑定的文件描述符（如 socket）
        int events_;      //!< 当前监听的事件掩码（由用户设置）
        uint32_t revents_;//!< Poller 返回的就绪事件掩码
        int index_;       //!< 在 Poller 中的状态索引（-1: 未注册，1: 已注册，2: 已删除）

        std::weak_ptr<void> tie_;//!< 绑定的共享资源（如 [TcpConnection]）
        bool tied_;              //!< 是否已绑定资源

        ReadEventCallback readCallback_;//!< 读事件回调（参数为事件触发时间）
        EventCallback writeCallback_;   //!< 写事件回调
        EventCallback closeCallback_;   //!< 连接关闭回调
        EventCallback errorCallback_;   //!< 错误处理回调
    };
}// namespace net

#endif//MY_MUDUO_CHANNEL_H
