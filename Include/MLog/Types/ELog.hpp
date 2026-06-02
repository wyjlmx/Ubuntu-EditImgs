#pragma once

#include <cstdint>
#include <string>

namespace Log
{
    enum class LogLevel : uint8_t
    {
        INFO = 0,
        ERROR
    };

    struct LogBuffer
    {
        std::string buffer;

        LogBuffer() { buffer.reserve(1025); }

        void reset() { buffer.clear(); }
    };
}