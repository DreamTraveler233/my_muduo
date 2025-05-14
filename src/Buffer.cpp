//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/net/Buffer.h"

using namespace net;

Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend)
{}

void Buffer::retrieve(size_t len)
{
    // 处理部分数据消费场景：仅移动读指针位置
    if (len < readableBytes())
    {
        readerIndex_ += len;
    }
    else// len == readableBytes()
    {
        // 完全消费场景：调用重置方法清理读写指针
        retrieveAll();
    }
}

std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readerIndex_);
}

std::string Buffer::retrieveAsString(size_t len)
{
    // 构造结果字符串：从当前读取位置复制len字节数据
    std::string result(beginRead(), len);
    // 更新缓冲区状态：将已读取的len字节移出缓冲区
    retrieve(len);
    return result;
}

void Buffer::append(const char *data, size_t len)
{
    // 确保缓冲区至少有len字节的可写空间
    // 可能触发缓冲区自动扩容操作
    ensureWritableBytes(len);

    // 将数据复制到当前写指针位置
    // 使用std::copy保证内存安全拷贝
    std::copy(data, data + len, beginWrite());

    // 更新写索引位置，反映新写入的数据长度
    writerIndex_ += len;
}

ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    // 备用缓冲区(64KB栈空间)，用于主缓冲区不足时的溢出存储
    char extrabuf[65536] = {};

    // 分散读结构体数组：主缓冲区 + 备用缓冲区
    struct iovec vec[2];
    const size_t writable = writableBytes();

    // 主缓冲区存储段
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;

    // 备用缓冲区存储段
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // 当主缓冲区空间小于备用缓冲区时，启用双缓冲机制
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    // 执行分散读操作
    const ssize_t n = readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (n <= writable)
    {
        // 全部数据存入主缓冲区的情况
        writerIndex_ += n;
    }
    else
    {
        // 主缓冲区填满后使用备用缓冲区的情况：
        // 1. 主缓冲区写至末尾
        // 2. 将备用缓冲区数据追加到主缓冲区
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}


ssize_t Buffer::writeFd(int fd, int *savedErrno)
{
    // 调用系统write函数执行写入操作，从读指针位置开始写入可读数据
    ssize_t n = write(fd, beginRead(), readableBytes());

    // 处理写入错误情况：当写入失败时将系统错误码保存到输出参数
    if (n < 0)
    {
        *savedErrno = errno;
    }
    return n;
}

void Buffer::retrieveAll()
{
    // 同时重置读索引和写索引到预分配的起始位置
    // 该操作本质上清空了缓冲区，但保留了预分配的空间
    readerIndex_ = writerIndex_ = kCheapPrepend;
}

void Buffer::makeSpace(size_t len)
{
    /*
     * 扩容前：
     * |  kCheapPrepend  |    reader   |    writer   |
     * 需要的空间大小：
     * |  kCheapPrepend  |             len             |
     * 因为writableBytes() + prependableBytes() < len + kCheapPrepend，
     * 所以必须进行扩容，将写缓冲区的大小设置为len，防止过度扩容
     *
     * 扩容后：
     * |  kCheapPrepend  |    reader   |             len             |
     */

    // 空间不足时执行扩容操作
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
        buffer_.resize(writerIndex_ + len);
    }
    // 空间足够时移动数据到缓冲区前端以腾出连续空间
    else
    {
        size_t readable = readableBytes();
        std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
}

void Buffer::ensureWritableBytes(size_t len)
{
    // 当剩余可写空间不足时，执行空间扩展操作
    if (writableBytes() < len)
    {
        // 调用内部空间分配方法，具体扩容策略由makeSpace实现
        makeSpace(len);
    }
}

size_t Buffer::readableBytes() const
{
    return writerIndex_ - readerIndex_;
}
size_t Buffer::prependableBytes() const
{
    return readerIndex_;
}
size_t Buffer::writableBytes() const
{
    return buffer_.size() - writerIndex_;
}
const char *Buffer::begin() const
{
    return buffer_.data();
}
char *Buffer::begin()
{
    return buffer_.data();
}
const char *Buffer::beginRead() const
{
    return begin() + readerIndex_;
}
char *Buffer::beginRead()
{
    return begin() + readerIndex_;
}
char *Buffer::beginWrite()
{
    return begin() + writerIndex_;
}
const char *Buffer::beginWrite() const
{
    return begin() + writerIndex_;
}