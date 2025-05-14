//
// Created by shuzeyong on 2025/5/13.
//

#ifndef MY_MUDUO_THREADPOOLCONFIG_H
#define MY_MUDUO_THREADPOOLCONFIG_H

#include "SysHeadFile.h"
#include "ThreadPool.h"

namespace thp
{
    /**
     * @struct ThreadPoolConfig
     * @brief 线程池配置结构体，用于配置线程池的运行参数
     */
    struct ThreadPoolConfig
    {
        PoolMode mode = PoolMode::MODE_CACHED;                      //!< 线程池模式，默认为动态模式
        size_t threadMaxSize = 200;                                 //!< CACHED 模式下，线程池线程最大数量
        size_t taskQueMaxSize = 2048;                               //!< 任务队列最大任务数量
        size_t initThreadSize = std::thread::hardware_concurrency();//!< 线程初始化数量，默认为硬件支持的并发线程数
        size_t threaMaxIdleTime = 60;                               //!< 线程最大空闲时间（单位：秒）

        /**
         * @brief 参数校验方法，用于验证配置参数是否合法
         * @return 如果配置参数合法，返回 true；否则返回 false
         */
        [[nodiscard]] bool validate() const
        {
            return threadMaxSize >= initThreadSize && taskQueMaxSize > 0 && threaMaxIdleTime > 0;
        }
    };
}// namespace thp

#endif//MY_MUDUO_THREADPOOLCONFIG_H
