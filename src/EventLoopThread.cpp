//
// Created by shuzeyong on 2025/5/7.
//

#include "../include/my_net/EventLoopThread.h"

/**
 * @brief EventLoopThread 构造函数 - 创建事件循环线程对象
 *
 * @param cb 线程初始化回调函数，当事件循环线程启动时会执行该回调（可为空）
 * @param name 线程名称，用于标识和调试目的
 *
 * @note 成员初始化说明：
 * - loop_ 初始化为空，将在线程函数中创建实际的事件循环对象
 * - exiting_ 初始化为 false 表示线程处于运行状态
 * - thread_ 使用 lambda 包装的 threadFunc 作为线程主函数，并设置线程名称
 * - mutex_ 和 cond_ 用于线程同步的条件变量机制
 * - callback_ 通过移动语义保存线程初始化回调函数
 */
EventLoopThread::EventLoopThread(ThreadInitCallback cb, std::string name)
    : loop_(nullptr),      // 延迟初始化，将在所属线程创建
      exiting_(false),     // 线程运行状态标识
      thread_([this] { threadFunc(); },std::move(name)), // 绑定成员函数作为线程入口
      mutex_(),            // 默认构造互斥锁
      cond_(),             // 默认构造条件变量
      callback_(std::move(cb)) // 移动捕获线程初始化回调
{}

/**
 * @brief EventLoopThread 类析构函数
 *
 * 负责安全停止事件循环线程并回收资源。主要工作流程：
 * 1. 设置退出标志位通知关联线程结束
 * 2. 停止事件循环（如果存在）
 * 3. 等待工作线程结束
 */
EventLoopThread::~EventLoopThread()
{
    // 设置线程退出标志，通知关联线程需要结束运行
    exiting_ = true;

    // 安全停止事件循环并回收线程资源
    if (loop_ != nullptr)
    {
        // 请求事件循环退出（非立即停止）
        loop_->quit();

        // 等待工作线程完全退出，避免资源泄漏
        thread_.join();
    }
}

/**
 * @brief 启动事件循环线程并返回创建的事件循环对象
 *
 * 本函数完成以下主要工作：
 * 1. 启动成员线程执行事件循环
 * 2. 通过条件变量等待新线程完成EventLoop对象初始化
 * 3. 返回新线程创建的事件循环对象指针
 *
 * @return EventLoop* 新线程创建的事件循环对象指针，
 *        调用者通过该指针可以与事件循环线程进行交互
 */
EventLoop *EventLoopThread::startLoop()
{
    // 启动事件循环线程，将执行EventLoopThread::threadFunc函数
    thread_.start();

    EventLoop *loop = nullptr;

    // 同步块：等待新线程完成EventLoop对象初始化
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 使用条件变量等待直到loop_被新线程设置
        while (loop_ == nullptr)
        {
            cond_.wait(lock);// 自动释放锁并等待通知
        }
        loop = loop_;// 获取已初始化的EventLoop对象
    }

    return loop;
}

/**
 * @brief 新线程入口函数，执行事件循环的创建与运行
 *
 * 1. 在新线程中创建独立的EventLoop对象，实现"one loop per thread"模型
 * 2. 通过条件变量通知主线程子线程已初始化完成
 * 3. 运行事件循环直到退出
 *
 * 线程安全说明：
 * - 使用互斥锁保护loop_成员的读写操作
 * - 通过条件变量实现线程间同步
 */
void EventLoopThread::threadFunc()
{
    // 创建线程专属的事件循环对象（每个IO线程有独立的事件循环）one loop per thread
    EventLoop loop;

    // 执行用户自定义的初始化回调（常用于设置线程特定参数）
    if (callback_)
    {
        // 用户回调函数，允许在事件循环启动前进行自定义配置
        callback_(&loop);
    }

    // 同步机制：将创建好的事件循环对象通过条件变量通知主线程
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();// 唤醒等待的主线程
    }

    // 启动事件循环（阻塞调用，直到事件循环退出）
    loop.loop();// EventLoop loop => Poller.poll

    // 清理阶段：事件循环结束后置空指针
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;// 防止产生悬挂指针
    }
}
