//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_INETADDRESS_H
#define MY_MUDUO_INETADDRESS_H

#include "SysHeadFile.h"

namespace net
{
    /**
     * @class InetAddress
     * @brief 封装网络地址信息，提供 IP 地址和端口的操作接口
     *
     * 本类主要用于封装 `sockaddr_in` 结构体，提供以下功能：
     * - 支持从 IP 地址和端口构造网络地址
     * - 支持从 `sockaddr_in` 结构体构造网络地址
     * - 提供 IP 地址、端口和完整地址字符串的获取方法
     * - 提供对底层 `sockaddr_in` 结构体的访问和修改接口
     */
    class InetAddress
    {
    public:
        /**
         * @brief 构造函数，通过 IP 地址和端口初始化网络地址
         * @param port 端口号（默认为 0）
         * @param ip IP 地址（默认为 "127.0.0.1"）
         */
        explicit InetAddress(uint16_t port = 0, const std::string &ip = "127.0.0.1");

        /**
         * @brief 构造函数，通过 `sockaddr_in` 结构体初始化网络地址
         * @param addr `sockaddr_in` 结构体
         */
        explicit InetAddress(const sockaddr_in &addr);

        /**
         * @brief 获取 IP 地址字符串
         * @return 返回 IP 地址字符串
         */
        [[nodiscard]] std::string toIp() const;

        /**
         * @brief 获取完整的 IP 地址和端口字符串
         * @return 返回格式为 "IP:Port" 的字符串
         */
        [[nodiscard]] std::string toIpPort() const;

        /**
         * @brief 获取端口号
         * @return 返回端口号
         */
        [[nodiscard]] uint16_t toPort() const;

        /**
         * @brief 获取底层的 `sockaddr_in` 结构体
         * @return 返回 `sockaddr_in` 结构体的常量引用
         */
        [[nodiscard]] const sockaddr_in &getSockAddr() const;

        /**
         * @brief 设置底层的 `sockaddr_in` 结构体
         * @param addr 新的 `sockaddr_in` 结构体
         */
        void setSockAddr(const sockaddr_in &addr);

    private:
        sockaddr_in addr_{};//!< 底层的 `sockaddr_in` 结构体，存储网络地址信息
    };
}// namespace net

#endif//MY_MUDUO_INETADDRESS_H
