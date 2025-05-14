//
// Created by shuzeyong on 2025/4/30.
//

#include "../include/net/Logger.h"
#include "../include/net/Timestamp.h"

using namespace net;

Logger &Logger::getInstance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level)
{
    level_ = level;
}

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
