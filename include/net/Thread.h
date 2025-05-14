//
// Created by shuzeyong on 2025/5/7.
//

#ifndef MY_MUDUO_THREAD_H
#define MY_MUDUO_THREAD_H

#include "CurrentThread.h"
#include "NonCopyable.h"
#include "SysHeadFile.h"

namespace net
{
    /**
 * @brief 线程封装类，实现跨平台线程管理
 *
 * 核心特性：
 * - 封装std::thread，提供更安全的接口
 * - 支持自定义线程名称和线程函数
 * - 禁止拷贝构造/赋值（继承NonCopyable）
 * - 自动管理线程生命周期（析构时自动detach未join线程）
 */
    class Thread : NonCopyable
    {
    public:
        using ThreadFunc = std::function<void()>;// 线程执行函数类型

        explicit Thread(ThreadFunc func, std::string name = std::string());
        ~Thread();

        // 启动线程（线程ID通过内部同步机制安全获取）
        void start();
        // 等待线程结束（阻塞调用）
        void join();

        // 检查线程是否已启动
        [[nodiscard]] bool started() const;
        // 获取系统级线程ID（有效期为线程生命周期）
        [[nodiscard]] pid_t getTid() const;
        // 获取线程名称（可用于调试日志）
        [[nodiscard]] const std::string &getName() const;
        // 获取全局已创建的线程总数（线程安全）
        static int getNumCreated();

    private:
        // 设置默认线程名称（命名规则：Thread+递增序号）
        void setDefaultName();

        bool started_;                       // 线程启动状态标志（true表示已启动）
        bool joined_;                        // 线程加入状态标志（true表示已调用join）
        std::shared_ptr<std::thread> thread_;// 线程对象（使用shared_ptr管理生命周期）
        pid_t tid_;                          // 系统级线程ID（通过CurrentThread::tid()获取）
        ThreadFunc threadFunc_;              // 线程函数
        std::string threadName_;             // 线程名称（调试用）
        static std::atomic_int threadNum_;   // 全局线程计数器（线程安全）
    };
}// namespace net

#endif//MY_MUDUO_THREAD_H
