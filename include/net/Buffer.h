//
// Created by shuzeyong on 2025/5/8.
//

#ifndef MY_MUDUO_BUFFER_H
#define MY_MUDUO_BUFFER_H

#include "SysHeadFile.h"

namespace net
{
    // 缓冲区内存布局示意图：
    // +-------------------+------------------+------------------+
    // | prependable bytes |  readable bytes  |  writable bytes  |
    // |                   |     (CONTENT)    |                  |
    // +-------------------+------------------+------------------+
    // |                   |                  |                  |
    // 0      <=      readerIndex   <=   writerIndex    <=     size
    //
    // - prependable：预留空间，适合用于插入协议头部信息
    // - readable：当前可读区域，包含已接收但尚未处理的数据
    // - writable：当前可写区域，可以继续向其中写入新数据

    /**
     * @brief 用户空间缓冲区类，用于网络通信中数据的高效读写管理
     *
     * 采用双指针设计模式（readerIndex 和 writerIndex），支持自动扩容、预分配空间等特性，
     * 特别适用于异步网络编程中的粘包处理、协议解析和数据暂存。
     */
    class Buffer
    {
    public:
        // 预留空间大小，默认为8字节，常用于存储协议头部长度字段等场景
        static const size_t kCheapPrepend = 8;
        // 默认初始缓冲区容量，设置为1KB
        static const size_t kInitialSize = 1024;

        explicit Buffer(size_t initialSize = kInitialSize);

        /*基础操作*/
        // 获取当前可读数据的字节数
        [[nodiscard]] size_t readableBytes() const;
        // 获取当前可写空间的字节数
        [[nodiscard]] size_t writableBytes() const;
        // 获取前置预留空间的剩余字节数
        [[nodiscard]] size_t prependableBytes() const;

        /* 数据消费 */
        // 标记已读取 len 字节，移动 readerIndex
        void retrieve(size_t len);
        // 将所有可读数据转为字符串并清空 Buffer
        std::string retrieveAllAsString();
        // 提取指定长度的数据为字符串，并移动 readerIndex
        std::string retrieveAsString(size_t len);
        // 清空可读区域（readerIndex 移动到 writerIndex）
        void retrieveAll();

        /*数据追加*/
        // 向缓冲区尾部追加数据
        void append(const char *data, size_t len);

        /*I/O 操作*/
        // 从文件描述符读取数据到缓冲区中（通常是 socket）
        ssize_t readFd(int fd, int *savedErrno);
        // 将缓冲区数据写入文件描述符（如 socket）
        ssize_t writeFd(int fd, int *savedErrno);

    private:
        // 获取缓冲区起始地址
        char *begin();
        [[nodiscard]] const char *begin() const;
        // 获取当前可读区域的起始地址
        char *beginRead();
        [[nodiscard]] const char *beginRead() const;
        // 获取当前可写区域的起始地址
        char *beginWrite();
        [[nodiscard]] const char *beginWrite() const;

        // 扩容或移动数据以腾出足够的写空间
        void makeSpace(size_t len);
        // 确保缓冲区中有至少 len 字节的可写空间，必要时扩容或移动数据
        void ensureWritableBytes(size_t len);

        std::vector<char> buffer_;          // 实际存储数据的容器
        size_t readerIndex_ = kCheapPrepend;// 当前可读位置索引
        size_t writerIndex_ = kCheapPrepend;// 当前可写位置索引
    };
}

#endif//MY_MUDUO_BUFFER_H
