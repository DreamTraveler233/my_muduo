#ifndef MY_MUDUO_CALLBACKS_H  
#define MY_MUDUO_CALLBACKS_H  

#include "SysHeadFile.h"  

namespace net  
{  
    class Buffer;  
    class TcpConnection;  
    class Timestamp;  

    /**  
     * @class TcpConnection  
     * @brief TCP 连接对象的智能指针类型定义  
     */  
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;  

    /**  
     * @brief 连接建立或关闭时的回调函数类型  
     * @param conn TCP 连接对象的共享指针  
     */  
    using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;  

    /**  
     * @brief 连接关闭时的专用回调函数类型  
     * @param conn TCP 连接对象的共享指针  
     */  
    using CloseCallback = std::function<void(const TcpConnectionPtr &)>;  

    /**  
     * @brief 数据发送完成时的回调函数类型  
     * @param conn TCP 连接对象的共享指针  
     */  
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;  

    /**  
     * @brief 接收到新消息时的回调函数类型  
     * @param conn TCP 连接对象的共享指针  
     * @param buffer 接收数据的缓冲区指针  
     * @param receiveTime 数据到达的时间戳  
     */  
    using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer *, Timestamp)>;  

    /**  
     * @brief 高水位标记触发的回调函数类型  
     * @param conn TCP 连接对象的共享指针  
     * @param mark 高水位标记的字节数阈值  
     */  
    using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;  
}// namespace net  

#endif//MY_MUDUO_CALLBACKS_H  