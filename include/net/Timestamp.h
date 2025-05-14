//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_TIMESTAMP_H
#define MY_MUDUO_TIMESTAMP_H

#include "SysHeadFile.h"

namespace net
{
    class Timestamp
    {
    public:
        Timestamp();
        explicit Timestamp(int64_t microSecondsSinceEpoch);
        static Timestamp now();
        std::string toString();

    private:
        int64_t microSecondsSinceEpoch_;
    };
}// namespace net

#endif//MY_MUDUO_TIMESTAMP_H
