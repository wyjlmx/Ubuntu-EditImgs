#ifndef PATTERN_HPP
#define PATTERN_HPP

#include "mlog/types/SLog.hpp"

#include <vector>
#include <memory>

namespace ei::mlog
{
    class IFormatOP
    {
    public:
        ~IFormatOP() = default;

        virtual void format(const LogPattern &pattern, LogBuffer &buffer) = 0;
    };

    template <typename OP>
    class FormatOP : public IFormatOP
    {
    public:
        void format(const LogPattern &pattern, LogBuffer &buffer) override
        {
            OP::format(pattern, buffer);
        }
    };

    class Pattern
    {
    public:
        Pattern() = default;
        ~Pattern() = default;

        void setPattern(const char *fmt);
        void formatTo(const LogPattern &pattern, LogBuffer &logBuffer);

    private:
        std::vector<std::unique_ptr<IFormatOP>> _patternPipelines;
    };
}

#endif // PATTERN_HPP