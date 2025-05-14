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
     * @class Thread
     * @brief 线程封装类，实现跨平台线程管理
     *
     * 核心特性：
     * - 封装 `std::thread`，提供更安全的接口
     * - 支持自定义线程名称和线程函数
     * - 禁止拷贝构造/赋值（继承 [NonCopyable]）
     * - 自动管理线程生命周期（析构时自动 detach 未 join 线程）
     */
    class Thread : NonCopyable
    {
    public:
        using ThreadFunc = std::function<void()>; //!< 线程执行函数类型

        /**
         * @brief 构造函数，初始化线程
         * @param func 线程执行函数
         * @param name 线程名称（可选）
         */
        explicit Thread(ThreadFunc func, std::string name = std::string());

        /**
         * @brief 析构函数，自动管理线程生命周期
         */
        ~Thread();

        /**
         * @brief 启动线程（线程 ID 通过内部同步机制安全获取）
         */
        void start();

        /**
         * @brief 等待线程结束（阻塞调用）
         */
        void join();

        /**
         * @brief 检查线程是否已启动
         * @return 如果线程已启动，返回 true；否则返回 false
         */
        [[nodiscard]] bool started() const;

        /**
         * @brief 获取系统级线程 ID（有效期为线程生命周期）
         * @return 返回系统级线程 ID
         */
        [[nodiscard]] pid_t getTid() const;

        /**
         * @brief 获取线程名称（可用于调试日志）
         * @return 返回线程名称
         */
        [[nodiscard]] const std::string &getName() const;

        /**
         * @brief 获取全局已创建的线程总数（线程安全）
         * @return 返回全局已创建的线程总数
         */
        static int getNumCreated();

    private:
        /**
         * @brief 设置默认线程名称（命名规则：Thread+递增序号）
         */
        void setDefaultName();

        bool started_;                       //!< 线程启动状态标志（true 表示已启动）
        bool joined_;                        //!< 线程加入状态标志（true 表示已调用 join）
        std::shared_ptr<std::thread> thread_;//!< 线程对象（使用 shared_ptr 管理生命周期）
        pid_t tid_;                          //!< 系统级线程 ID（通过 [CurrentThread::tid()] 获取）
        ThreadFunc threadFunc_;              //!< 线程函数
        std::string threadName_;             //!< 线程名称（调试用）
        static std::atomic_int threadNum_;   //!< 全局线程计数器（线程安全）
    };
}// namespace net

#endif//MY_MUDUO_THREAD_H
