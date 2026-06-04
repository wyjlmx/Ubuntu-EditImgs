#ifndef SLOG_HPP
#define SLOG_HPP

#include "ELog.hpp"

#include <cstdint>
#include <string>
#include <source_location>

namespace ei::mlog
{
    struct LogBuffer
    {
        std::string buffer;

        LogBuffer() { buffer.reserve(1025); }

        void reset() { buffer.clear(); }
    };

    struct LogPattern
    {
        LogLevel level;
        std::string threadId;
        std::string timestamp;
        std::string message;
        std::source_location location;
    };

    struct LogOptions
    {
        std::string logDirPath = "./Logs";
        std::string patternFmt = "[%t]^%f:%n^:> %m";     // %t:时间、%l:级别、%m:消息、%f:文件名、%n:行数、%F:函数名
    };
}

#endif // SLOG_HPP
