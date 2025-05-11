//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_BUFFER_H
#define MY_MUDUO_BUFFER_H

#include "SysHeadFile.h"

// +-------------------+------------------+------------------+
// | prependable bytes |  readable bytes  |  writable bytes  |
// |                   |     (CONTENT)    |                  |
// +-------------------+------------------+------------------+
// |                   |                  |                  |
// 0      <=      readerIndex   <=   writerIndex    <=     size

/**
 * @brief 缓冲区类，用于高效管理数据的读写和预置空间
 *
 * 采用双指针设计，支持自动扩容和内存预分配，适用于网络编程等场景
 */
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;  // 预置空间默认大小，用于协议头等场景
    static const size_t kInitialSize = 1024;// 缓冲区默认初始容量

    explicit Buffer(size_t initialSize = kInitialSize);
    // 获取可读数据字节数
    [[nodiscard]] size_t readableBytes() const;
    // 获取可写空间字节数
    [[nodiscard]] size_t writableBytes() const;
    // 获取预置空间剩余字节数
    [[nodiscard]] size_t prependableBytes() const;

    void retrieve(size_t len);
    void retrieveAll();
    std::string retrieveAllAsString();
    std::string retrieveAsString(size_t len);
    void ensureWritableBytes(size_t len);
    void append(const char *data, size_t len);

    // 从fd上读取数据
    ssize_t readFd(int fd, int *savedErrno);
    // 往fd上发送数据
    ssize_t writeFd(int fd, int *savedErrno);

private:
    // 获取缓冲区起始地址
    char *begin();
    [[nodiscard]] const char *begin() const;
    // 获取读缓冲区起始地址
    char *beginRead();
    [[nodiscard]] const char *beginRead() const;
    // 获取写缓冲区起始地址
    char *beginWrite();
    [[nodiscard]] const char *beginWrite() const;

    void makeSpace(size_t len);

    std::vector<char> buffer_;          // 实际数据存储容器
    size_t readerIndex_ = kCheapPrepend;// 当前读指针位置
    size_t writerIndex_ = kCheapPrepend;// 当前写指针位置
};

#endif//MY_MUDUO_BUFFER_H
