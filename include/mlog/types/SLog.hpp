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
}

#endif // SLOG_HPP
