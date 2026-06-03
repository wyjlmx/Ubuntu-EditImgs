#ifndef ELOG_HPP
#define ELOG_HPP

#include <cstdint>
#include <string>

namespace ei::mlog
{
    enum class LogLevel : uint8_t
    {
        INFO = 0,
        ERROR
    };
    
}

#endif // ELOG_HPP