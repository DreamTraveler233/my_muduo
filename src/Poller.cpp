//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/net/Poller.h"

using namespace net;

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}
/**
 * @brief 检查指定的 Channel 是否存在于 Poller 中。
 * 该函数通过查找与 Channel 关联的文件描述符（fd）来检查该 Channel 是否已经注册到 Poller 中。
 * 如果找到对应的文件描述符，并且该文件描述符对应的 Channel 与传入的 Channel 相同，则返回 true，否则返回 false。
 * @param channel 指向要检查的 Channel 对象的指针。
 * @return bool 如果 Channel 存在于 Poller 中，则返回 true；否则返回 false。
 */
bool Poller::hasChannel(Channel *channel)
{
    // 查找与 Channel 关联的文件描述符
    auto it = channels_.find(channel->getFd());

    // 检查是否找到对应的文件描述符，并且该文件描述符对应的 Channel 与传入的 Channel 相同
    return it != channels_.end() && it->second == channel;
}
