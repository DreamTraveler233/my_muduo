//
// Created by shuzeyong on 2025/4/30.
//

#ifndef MY_MUDUO_LOGGER_H
#define MY_MUDUO_LOGGER_H

#include "NonCopyable.h"
#include "SysHeadFile.h"
#include "Timestamp.h"

// LOG_INFO("%s, %d", arg1, arg2)
class Logger;
#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(INFO);                         \
        char buf[1024] = {};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(ERROR);                        \
        char buf[1024] = {};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(FATAL);                        \
        char buf[1024] = {};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
        exit(-1);                                         \
    } while (0)

#ifdef DEBUG
#define LOG_DEBUG(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &logger = Logger::getInstance();           \
        logger.setLogLevel(DEBUG);                        \
        char buf[1024] = {};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf);                                  \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

// 定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel {
    INFO, // 普通信息
    ERROR,// 错误信息
    FATAL,// core信息
    DEBUG // 调试信息
};

/*日志类*/
class Logger : NonCopyable
{
public:
    // 获取日志的唯一单例
    static Logger &getInstance();
    // 设置日志级别
    void setLogLevel(int level);
    // 写日志
    void log(const std::string &msg) const;

private:
    int level_;
};

#endif//MY_MUDUO_LOGGER_H
