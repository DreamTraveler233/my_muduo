//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_INETADDRESS_H
#define MY_MUDUO_INETADDRESS_H

#include "SysHeadFile.h"

class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, const std::string &ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr);
    [[nodiscard]] std::string toIp() const;
    [[nodiscard]] std::string toIpPort() const;
    [[nodiscard]] uint16_t toPort() const;
    [[nodiscard]] const sockaddr_in &getSockAddr() const;
    void setSockAddr(const sockaddr_in &addr);

private:
    sockaddr_in addr_{};
};

#endif//MY_MUDUO_INETADDRESS_H
