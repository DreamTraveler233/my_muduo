//
// Created by shuzeyong on 2025/5/6.
//

#ifndef MY_MUDUO_CURRENTTHREAD_H
#define MY_MUDUO_CURRENTTHREAD_H

#include "SysHeadFile.h"

namespace CurrentThread
{
    extern __thread int t_cachedTid;//!< 线程局部存储的线程 ID 缓存

    /**
     * @brief 缓存当前线程的线程 ID。
     *
     * 该函数会将当前线程的线程 ID 缓存到 `t_cachedTid` 中。
     */
    void cachedTid();

    /**
     * @brief 获取当前线程的线程 ID。
     * @return 当前线程的线程 ID。
     *
     * 如果 [t_cachedTid] 未缓存，则调用 [cachedTid()] 进行缓存并返回。
     */
    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cachedTid();
        }
        return t_cachedTid;
    }
}// namespace CurrentThread

#endif//MY_MUDUO_CURRENTTHREAD_H
