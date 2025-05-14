//
// Created by shuzeyong on 2025/5/1.
//

#include "../include/net/Timestamp.h"

using namespace net;

Timestamp::Timestamp()
    : microSecondsSinceEpoch_(0)
{}
Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    : microSecondsSinceEpoch_(microSecondsSinceEpoch)
{}

Timestamp Timestamp::now()
{
    // 使用time函数获取当前时间，并将其传递给Timestamp构造函数
    return Timestamp(time(nullptr));
}

std::string Timestamp::toString()
{
    char buf[128] = {};// 用于存储格式化后的时间字符串的缓冲区

    // 将时间戳转换为本地时间结构体 tm
    tm *tm_time = localtime(&microSecondsSinceEpoch_);

    // 使用 snprintf 将时间格式化为 "YYYY/MM/DD HH:MM:SS" 的字符串
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,// 年份，tm_year 是从 1900 年开始的偏移量
             tm_time->tm_mon + 1,    // 月份，tm_mon 是从 0 开始的，所以需要加 1
             tm_time->tm_mday,       // 日
             tm_time->tm_hour,       // 小时
             tm_time->tm_min,        // 分钟
             tm_time->tm_sec);       // 秒

    return buf;// 返回格式化后的时间字符串
}