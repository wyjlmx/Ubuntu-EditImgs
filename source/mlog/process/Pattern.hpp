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

    class LogOptions;

    class Pattern
    {
    public:
         Pattern() = default;
        ~Pattern() = default;

        Pattern(const Pattern&) = delete;
        Pattern& operator=(const Pattern&) = delete;

        Pattern(Pattern&&) = delete;
        Pattern& operator=(Pattern&&) = delete;

        void configure(const LogOptions& opts);
        void formatTo(const LogPattern &pattern, LogBuffer &logBuffer);

    private:
        std::string _currentPatternFmt;
        std::vector<std::unique_ptr<IFormatOP>> _patternPipelines;
    };
}

#endif // PATTERN_HPP