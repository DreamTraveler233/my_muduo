//
// Created by shuzeyong on 2025/5/1.
//

#ifndef MY_MUDUO_TIMESTAMP_H
#define MY_MUDUO_TIMESTAMP_H

#include "SysHeadFile.h"

namespace net
{
    /**
     * @class Timestamp
     * @brief 时间戳类，用于表示从 Epoch（1970-01-01 00:00:00 UTC）开始的微秒数
     */
    class Timestamp
    {
    public:
        /**
         * @brief 默认构造函数，初始化时间为 0
         */
        Timestamp();

        /**
         * @brief 构造函数，通过微秒数初始化时间戳
         * @param microSecondsSinceEpoch 从 Epoch 开始的微秒数
         */
        explicit Timestamp(int64_t microSecondsSinceEpoch);

        /**
         * @brief 获取当前时间的时间戳
         * @return 返回当前时间的时间戳
         */
        static Timestamp now();

        /**
         * @brief 将时间戳转换为字符串
         * @return 返回时间戳的字符串表示
         */
        std::string toString();

    private:
        int64_t microSecondsSinceEpoch_; //!< 从 Epoch 开始的微秒数
    };
}// namespace net

#endif//MY_MUDUO_TIMESTAMP_H
