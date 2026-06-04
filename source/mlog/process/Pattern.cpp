#include "Pattern.hpp"
#include "mlog/types/ELog.hpp"

#include <ctime>
#include <thread>
#include <chrono>

namespace ei::mlog
{
    namespace
    {
        std::string_view GetTimePattern(const char *timePattern)
        {
            struct TimeBuffer
            {
                char timeBuffer[64];
                size_t timeSize;
                std::time_t lastTime;
                const char *lastPattern;
            };

            thread_local TimeBuffer timeBuffer{
                .timeBuffer = {0},
                .timeSize = 0,
                .lastTime = 0,
                .lastPattern = nullptr};

            auto now = std::chrono::system_clock::now();
            auto nowSec = std::chrono::system_clock::to_time_t(now);

            if (nowSec != timeBuffer.lastTime || timePattern != timeBuffer.lastPattern)
            {
                timeBuffer.lastTime = nowSec;
                timeBuffer.lastPattern = timePattern;

                std::tm tmTime;
#if defined(_MSC_VER)
                localtime_s(&tmTime, &nowSec);
#else
                localtime_r(&nowSec, &tmTime);
#endif

                timeBuffer.timeSize = std::strftime(timeBuffer.timeBuffer, sizeof(timeBuffer.timeBuffer), timePattern, &tmTime);
            }

            return {timeBuffer.timeBuffer, timeBuffer.timeSize};
        }

        std::string_view GetLeavelToString(LogLevel level)
        {
            static constexpr std::string_view mapName[] = {"Info", "Error"};
            auto idx = static_cast<size_t>(level);
            return (idx < 2) ? mapName[idx] : "Unknown";
        }

        struct TimeOP
        {
            static void format([[maybe_unused]] const LogPattern &pattern, LogBuffer &buffer)
            {
                auto timeStr = GetTimePattern("%Y-%m-%d %H:%M:%S");
                buffer.buffer.append(timeStr.data(), timeStr.size());
            }
        };

        struct LeavelOP
        {
            static void format(const LogPattern &pattern, LogBuffer &buffer)
            {
                auto leavelStr = GetLeavelToString(pattern.level);
                buffer.buffer.append(leavelStr);
            }
        };

        struct MessageOP
        {
            static void format(const LogPattern &pattern, LogBuffer &buffer)
            {
                buffer.buffer.append(pattern.message);
            }
        };

        constexpr std::string_view KThisFile = __FILE__;
        constexpr std::string_view KThisSuffix = "source/mlog/process/Pattern.cpp";
        constexpr size_t KRootPrefixLen = (KThisFile.size() >= KThisSuffix.size() && KThisFile.ends_with(KThisSuffix)) ? (KThisFile.size() - KThisSuffix.size()) : 0;
        constexpr std::string_view KRootPrefix = KThisFile.substr(0, KRootPrefixLen);

        struct FileNameOp
        {
            static void format(const LogPattern &pattern, LogBuffer &buffer)
            {
                std::string_view fullPath(pattern.location.file_name());
                if (KRootPrefixLen > 0 && fullPath.starts_with(KRootPrefix))
                    fullPath.remove_prefix(KRootPrefixLen);

                buffer.buffer.append(fullPath);
            }
        };

        struct LineNumberOp
        {
            static void format(const LogPattern &pattern, LogBuffer &buffer)
            {
                buffer.buffer.append(std::to_string(pattern.location.line()));
            }
        };

        struct FunctionNameOp
        {
            static void format(const LogPattern &pattern, LogBuffer &buffer)
            {
                buffer.buffer.append(pattern.location.function_name());
            }
        };

        class LiteralOp : public IFormatOP
        {
        private:
            std::string _literal;

        public:
            LiteralOp(std::string_view literal) : _literal(literal) {}

            void format([[maybe_unused]] const LogPattern &pattern, LogBuffer &buffer) override
            {
                buffer.buffer.append(_literal);
            }
        };
    }

    void Pattern:: configure(const LogOptions& opts)
    {
        if(_currentPatternFmt == opts.patternFmt)
            return;

        _currentPatternFmt = opts.patternFmt;

        _patternPipelines.clear();

        std::string_view pattern(_currentPatternFmt);
        for (size_t i = 0; i < pattern.size();)
        {
            if (pattern[i] == '%')
            {
                if (i + 1 < pattern.size())
                {
                    char op = pattern[i + 1];
                    switch (op)
                    {
                    case 't':
                        _patternPipelines.push_back(std::make_unique<FormatOP<TimeOP>>());
                        break;
                    case 'l':
                        _patternPipelines.push_back(std::make_unique<FormatOP<LeavelOP>>());
                        break;
                    case 'm':
                        _patternPipelines.push_back(std::make_unique<FormatOP<MessageOP>>());
                        break;
                    case 'f':
                        _patternPipelines.push_back(std::make_unique<FormatOP<FileNameOp>>());
                        break;
                    case 'n':
                        _patternPipelines.push_back(std::make_unique<FormatOP<LineNumberOp>>());
                        break;
                    case 'F':
                        _patternPipelines.push_back(std::make_unique<FormatOP<FunctionNameOp>>());
                        break;
                    default:
                        _patternPipelines.push_back(std::make_unique<LiteralOp>(std::string("%") + op));
                        break;
                    }

                    i += 2;
                }
                else
                {
                    _patternPipelines.push_back(std::make_unique<LiteralOp>(std::string("%")));
                    i++;
                }
            }
            else
            {
                size_t start = i;
                while (i < pattern.size() && pattern[i] != '%')
                    i++;

                _patternPipelines.push_back(std::make_unique<LiteralOp>(pattern.substr(start, i - start)));
            }
        }
    }

    void Pattern::formatTo(const LogPattern &pattern, LogBuffer &logBuffer)
    {
        logBuffer.reset();

        for (const auto &op : _patternPipelines)
            op->format(pattern, logBuffer);
    }
}