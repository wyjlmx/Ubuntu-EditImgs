#include "mlog/Log.hpp"

#include <iostream>

int main()
{
    std::cout << "hello, world" << std::endl;

    auto &logger = ei::mlog::Log::getInstance();
    logger.write(
        ei::mlog::LogLevel::INFO,
        "This is an Info log message.",
        std::source_location::current() // <-- 抓取此处的行号、文件名
    );

    logger.write(
        ei::mlog::LogLevel::ERROR,
        "This is an Info log message.",
        std::source_location::current() // <-- 抓取此处的行号、文件名
    );

    return 0;
}