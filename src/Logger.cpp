//
// Created by shuzeyong on 2025/4/30.
//

#include "../include/my_net/Logger.h"

// 获取日志的唯一单例
/*利用局部静态变量的线程安全特性（C++11 保证）*/
Logger &Logger::getInstance()
{
    static Logger logger;
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int level)
{
    level_ = level;
}

// 写日志
/*日志输出格式：[级别信息] time : msg*/
void Logger::log(const std::string &msg) const
{
    switch (level_)
    {
        case INFO:
            std::cout << "[INFO]";
            break;
        case ERROR:
            std::cout << "[ERROR]";
            break;
        case FATAL:
            std::cout << "[FATAL]";
            break;
        case DEBUG:
            std::cout << "[DEBUG]";
            break;
        default:
            break;
    }

    // 打印时间和msg
    std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}
