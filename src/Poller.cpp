//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/net/Poller.h"

using namespace net;

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{}

bool Poller::hasChannel(Channel *channel)
{
    // 查找与 Channel 关联的文件描述符
    auto it = channels_.find(channel->getFd());

    // 检查是否找到对应的文件描述符，并且该文件描述符对应的 Channel 与传入的 Channel 相同
    return it != channels_.end() && it->second == channel;
}
