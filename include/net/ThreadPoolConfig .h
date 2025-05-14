//
// Created by shuzeyong on 2025/5/13.
//

#ifndef MY_MUDUO_THREADPOOLCONFIG_H
#define MY_MUDUO_THREADPOOLCONFIG_H

#include "SysHeadFile.h"
#include "ThreadPool.h"

namespace thp
{
    struct ThreadPoolConfig
    {
        PoolMode mode = PoolMode::MODE_CACHED;                      // 默认动态模式
        size_t threadMaxSize = 200;                                 // CACHED模式下，线程池线程最大数量（1024）
        size_t taskQueMaxSize = 2048;                               // 任务队列最大任务数量（1024）
        size_t initThreadSize = std::thread::hardware_concurrency();// 线程初始化数量（硬件支持的并发线程数）
        size_t threaMaxIdleTime = 60;                               // 线程最大空闲时间（60s）

        // 参数校验方法
        [[nodiscard]] bool validate() const
        {
            return threadMaxSize >= initThreadSize && taskQueMaxSize > 0 && threaMaxIdleTime > 0;
        }
    };
}

#endif//MY_MUDUO_THREADPOOLCONFIG_H
