//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/net/InetAddress.h"

using namespace net;

InetAddress::InetAddress(uint16_t port, const std::string &ip)
{
    addr_.sin_family = AF_INET;                            // ipv4
    addr_.sin_port = htons(port);                          // 将本地字节序转换为网络字节序
    inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr.s_addr);// 将字符串 -> 二进制（IPv4）
}

InetAddress::InetAddress(const sockaddr_in &addr)
    : addr_(addr)
{}

std::string InetAddress::toIp() const
{
    char buf[64] = {};
    inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const
{
    // ip : port
    std::ostringstream oss;
    oss << toIp() << ":" << toPort();
    return oss.str();
}

uint16_t InetAddress::toPort() const { return ntohs(addr_.sin_port); }
const sockaddr_in &InetAddress::getSockAddr() const { return addr_; }
void InetAddress::setSockAddr(const sockaddr_in &addr) { addr_ = addr; }