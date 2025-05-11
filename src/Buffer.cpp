//
// Created by shuzeyong on 2025/5/8.
//

#include "../include/my_net/Buffer.h"

Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend)
{}

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
    return &*buffer_.begin();
}
char *Buffer::begin()
{
    return &*buffer_.begin();
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


/**
 * @brief 移动读指针向前推进指定长度，处理缓冲区数据消费操作
 *
 * 当消费部分数据时更新读指针位置，消费全部可读数据时直接重置缓冲区索引
 *
 * @param len 需要消费的数据长度，若等于可读数据长度则触发完全重置，
 *            必须满足 len <= readableBytes()
 * @note 调用后原数据仍保留在缓冲区，但后续读取会从新位置开始
 */
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

/**
 * @brief 重置缓冲区读写位置到初始状态
 *
 * 该函数用于清空缓冲区内容，将读写指针重置到预留空间的起始位置。
 * 常用于复用缓冲区内存空间，或准备接收新的数据包。
 */
void Buffer::retrieveAll()
{
    // 同时重置读索引和写索引到预分配的起始位置
    // 该操作本质上清空了缓冲区，但保留了预分配的空间
    readerIndex_ = writerIndex_ = kCheapPrepend;
}

/**
 * @brief 获取缓冲区中所有可读数据并以字符串形式返回
 *
 * 该函数通过调用retrieveAsString方法，从当前读索引位置开始
 * 获取整个缓冲区的有效数据并转换为字符串。适用于需要一次性
 * 读取全部缓冲内容的场景。
 *
 * @return std::string 包含缓冲区所有有效数据的字符串
 */
std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readerIndex_);
}

/**
 * @brief 从缓冲区中提取指定长度的数据并转换为字符串
 *
 * 1. 从缓冲区当前可读位置开始，构造指定长度的字符串
 * 2. 向前移动缓冲区的读位置指针
 *
 * @param len 需要提取的字节长度。必须满足 len <= readableBytes()，
 *            否则可能引发未定义行为（具体约束取决于Buffer类的实现）
 * @return std::string 包含已提取数据的字符串对象
 *
 * @note 实际使用前应确保缓冲区中有足够可读数据，通常应先调用readableBytes()
 *       验证可用数据量是否满足len要求
 */
std::string Buffer::retrieveAsString(size_t len)
{
    // 构造结果字符串：从当前读取位置复制len字节数据
    std::string result(beginRead(), len);
    // 更新缓冲区状态：将已读取的len字节移出缓冲区
    retrieve(len);
    return result;
}

/**
 * @brief 确保缓冲区有足够可写空间
 *
 * 检查当前可写字节数是否满足需求，若剩余空间不足则通过makeSpace()方法
 * 扩展缓冲区空间。该方法是缓冲区自动扩容机制的核心接口。
 *
 * @param len 需要的最小可写字节数，类型为无符号整型size_t
 * @return void 无返回值
 */
void Buffer::ensureWritableBytes(size_t len)
{
    // 当剩余可写空间不足时，执行空间扩展操作
    if (writableBytes() < len)
    {
        // 调用内部空间分配方法，具体扩容策略由makeSpace实现
        makeSpace(len);
    }
}

/**
 * @brief 调整缓冲区空间以满足指定长度的写入需求
 *
 * 该函数根据当前空间情况选择扩容缓冲区或移动数据，确保至少有len字节的可写空间。
 * 当可写空间和前置空闲空间总和不足时进行扩容，否则通过移动数据优化空间利用率。
 *
 * @param len 需要保证的最小连续可写空间长度
 */
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

/**
 * @brief 将指定数据追加到缓冲区末尾
 *
 * 该函数首先确保缓冲区有足够的可写空间，然后将输入数据
 * 复制到缓冲区的写入位置，最后更新写入位置索引。
 *
 * @param data 要追加的数据的指针，数据来源的内存应由调用方管理有效性
 * @param len  要追加的数据长度（单位：字节），必须小于等于data实际可用长度
 */
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

/**
 * @brief 从文件描述符读取数据到缓冲区(LT模式)
 *
 * 本函数使用readv系统调用实现分散读操作，当主缓冲区空间不足时，
 * 会将溢出数据暂存到临时栈空间，再追加到缓冲区。适用于水平触发模式下
 * 的高效数据读取。
 *
 * @param fd 要读取的文件描述符
 * @param savedErrno 用于保存读取失败时的错误码
 * @return ssize_t 成功时返回读取的字节数，失败返回-1并设置savedErrno
 */
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

/**
 * @brief 将缓冲区中的数据写入指定的文件描述符
 *
 * @param fd 目标文件描述符，数据将被写入到此文件描述符
 * @param savedErrno 用于保存错误码的输出参数指针，当发生错误时存储errno值
 * @return ssize_t 成功时返回实际写入的字节数，
 *                 失败时返回-1并通过savedErrno返回错误码
 */
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