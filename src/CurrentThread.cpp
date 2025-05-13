//
// Created by shuzeyong on 2025/5/6.
//

#include "../include/net/CurrentThread.h"

namespace CurrentThread
{
    __thread int t_cachedTid = 0;
    void cachedTid()
    {
        if (t_cachedTid == 0)
        {
            t_cachedTid = static_cast<pid_t>(syscall(SYS_gettid));
        }
    }
}// namespace CurrentThread