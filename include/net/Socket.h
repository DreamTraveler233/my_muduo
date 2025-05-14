//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_SOCKET_H
#define MY_MUDUO_SOCKET_H

#include "InetAddress.h"
#include "Logger.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"

namespace net
{
    /**
     * @class Socket
     * @brief 封装套接字文件描述符及其核心操作
     *
     * 该类继承自 [NonCopyable]，禁止拷贝语义，管理套接字生命周期。
     * 提供以下网络基础操作：
     * - 绑定地址
     * - 监听连接
     * - 设置套接字选项
     */
    class Socket : NonCopyable
    {
    public:
        /**
         * @brief 构造函数，初始化套接字
         * @param sockfd 套接字文件描述符
         */
        explicit Socket(int sockfd);

        /**
         * @brief 析构函数，关闭套接字
         */
        ~Socket();

        /**
         * @brief 获取当前管理的套接字文件描述符
         * @return 返回套接字文件描述符
         */
        [[nodiscard]] int getFd() const;

        /**
         * @brief 绑定套接字到指定本地地址
         * @param localaddr 本地地址信息
         */
        void bind(const InetAddress &localaddr) const;

        /**
         * @brief 启动套接字监听模式
         */
        void listen() const;

        /**
         * @brief 接受客户端连接请求
         * @param peeraddr 用于存储客户端地址信息
         * @return 返回新连接的套接字文件描述符
         */
        int accept(InetAddress *peeraddr) const;

        /**
         * @brief 半关闭写方向
         */
        void shutdownWrite() const;

        /**
         * @brief 禁用 Nagle 算法
         * @param on 是否禁用（true 表示禁用，false 表示启用）
         */
        void setTcpNoDelay(bool on) const;

        /**
         * @brief 设置地址重用
         * @param on 是否启用地址重用（true 表示启用，false 表示禁用）
         */
        void setReuseAddr(bool on) const;

        /**
         * @brief 设置端口重用
         * @param on 是否启用端口重用（true 表示启用，false 表示禁用）
         */
        void setReusePort(bool on) const;

        /**
         * @brief 设置 TCP 保活机制
         * @param on 是否启用保活机制（true 表示启用，false 表示禁用）
         */
        void setKeepAlive(bool on) const;

    private:
        const int sockfd_; //!< 套接字文件描述符
    };
}// namespace net

#endif//MY_MUDUO_SOCKET_H
