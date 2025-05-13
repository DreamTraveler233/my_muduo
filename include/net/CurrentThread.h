//
// Created by shuzeyong on 2025/5/6.
//

#ifndef MY_MUDUO_CURRENTTHREAD_H
#define MY_MUDUO_CURRENTTHREAD_H

#include "SysHeadFile.h"

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    void cachedTid();

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
