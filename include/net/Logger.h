//
// Created by shuzeyong on 2025/4/30.
//

#ifndef MY_MUDUO_LOGGER_H
#define MY_MUDUO_LOGGER_H

#include "NonCopyable.h"
#include "SysHeadFile.h"

namespace net
{
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

/**
     * @enum LogLevel
     * @brief 定义日志级别
     */
enum LogLevel
{
    INFO,  ///< 普通信息
    ERROR, ///< 错误信息
    FATAL, ///< 严重错误信息（程序终止）
    DEBUG  ///< 调试信息
};

/**
     * @class Logger
     * @brief 日志类，提供日志记录功能
     *
     * 本类实现单例模式，提供以下功能：
     * - 支持不同级别的日志记录（INFO、ERROR、FATAL、DEBUG）
     * - 提供日志级别设置接口
     * - 提供日志写入接口
     *
     * @note 该类不可拷贝（继承 [NonCopyable]）
     */
class Logger : NonCopyable
{
public:
    /**
         * @brief 获取日志类的唯一单例
         * @return 返回 Logger 实例的引用
         */
    static Logger &getInstance();

    /**
         * @brief 设置日志级别
         * @param level 日志级别（参见 [LogLevel]）
         */
    void setLogLevel(int level);

    /**
         * @brief 写入日志
         * @param msg 日志消息内容
         */
    void log(const std::string &msg) const;

private:
    int level_; //!< 当前日志级别
};
}// namespace net

#endif//MY_MUDUO_LOGGER_H
