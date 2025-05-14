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
     * @class Buffer
     * @brief 用户空间缓冲区类，用于网络通信中数据的高效读写管理
     *
     * 采用双指针设计模式（readerIndex 和 writerIndex），支持自动扩容、预分配空间等特性，
     * 特别适用于异步网络编程中的粘包处理、协议解析和数据暂存。
     */
    class Buffer
    {
    public:
        /**
         * @brief 预留空间大小，默认为8字节，常用于存储协议头部长度字段等场景
         */
        static const size_t kCheapPrepend = 8;

        /**
         * @brief 默认初始缓冲区容量，设置为1KB
         */
        static const size_t kInitialSize = 1024;

        /**
         * @brief 构造函数
         * @param initialSize 初始缓冲区大小，默认为 kInitialSize
         */
        explicit Buffer(size_t initialSize = kInitialSize);

        /**
         * @brief 获取当前可读数据的字节数
         * @return 可读数据的字节数
         */
        [[nodiscard]] size_t readableBytes() const;

        /**
         * @brief 获取当前可写空间的字节数
         * @return 可写空间的字节数
         */
        [[nodiscard]] size_t writableBytes() const;

        /**
         * @brief 获取前置预留空间的剩余字节数
         * @return 前置预留空间的剩余字节数
         */
        [[nodiscard]] size_t prependableBytes() const;

        /**
         * @brief 标记已读取 len 字节，移动 readerIndex
         * @param len 已读取的字节数
         */
        void retrieve(size_t len);

        /**
         * @brief 将所有可读数据转为字符串并清空 Buffer
         * @return 转换后的字符串
         */
        std::string retrieveAllAsString();

        /**
         * @brief 提取指定长度的数据为字符串，并移动 readerIndex
         * @param len 要提取的字节数
         * @return 提取的字符串
         */
        std::string retrieveAsString(size_t len);

        /**
         * @brief 清空可读区域（readerIndex 移动到 writerIndex）
         */
        void retrieveAll();

        /**
         * @brief 向缓冲区尾部追加数据
         * @param data 要追加的数据
         * @param len 数据的长度
         */
        void append(const char *data, size_t len);

        /**
         * @brief 从文件描述符读取数据到缓冲区中（通常是 socket）
         * @param fd 文件描述符
         * @param savedErrno 保存错误码的指针
         * @return 读取的字节数
         */
        ssize_t readFd(int fd, int *savedErrno);

        /**
         * @brief 将缓冲区数据写入文件描述符（如 socket）
         * @param fd 文件描述符
         * @param savedErrno 保存错误码的指针
         * @return 写入的字节数
         */
        ssize_t writeFd(int fd, int *savedErrno);

    private:
        /**
         * @brief 获取缓冲区起始地址
         * @return 缓冲区的起始地址
         */
        char *begin();

        /**
         * @brief 获取缓冲区起始地址（const 版本）
         * @return 缓冲区的起始地址
         */
        [[nodiscard]] const char *begin() const;

        /**
         * @brief 获取当前可读区域的起始地址
         * @return 可读区域的起始地址
         */
        char *beginRead();

        /**
         * @brief 获取当前可读区域的起始地址（const 版本）
         * @return 可读区域的起始地址
         */
        [[nodiscard]] const char *beginRead() const;

        /**
         * @brief 获取当前可写区域的起始地址
         * @return 可写区域的起始地址
         */
        char *beginWrite();

        /**
         * @brief 获取当前可写区域的起始地址（const 版本）
         * @return 可写区域的起始地址
         */
        [[nodiscard]] const char *beginWrite() const;

        /**
         * @brief 扩容或移动数据以腾出足够的写空间
         * @param len 需要的空间大小
         */
        void makeSpace(size_t len);

        /**
         * @brief 确保缓冲区中有至少 len 字节的可写空间，必要时扩容或移动数据
         * @param len 需要的空间大小
         */
        void ensureWritableBytes(size_t len);

    private:
        std::vector<char> buffer_;          //!< 实际存储数据的容器
        size_t readerIndex_ = kCheapPrepend;//!< 当前可读位置索引
        size_t writerIndex_ = kCheapPrepend;//!< 当前可写位置索引
    };
}// namespace net

#endif//MY_MUDUO_BUFFER_H
