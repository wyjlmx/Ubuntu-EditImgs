#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "mlog/types/SLog.hpp"

#include <memory>

namespace ei::mlog
{
    class Pattern;

    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        void log(const LogPattern &pattern, LogBuffer &logBuffer);
    private:
        std::unique_ptr<Pattern> _patternEntry;
    };
}

#endif // PIPELINE_HPP