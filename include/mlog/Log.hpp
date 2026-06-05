#ifndef LOG_HPP
#define LOG_HPP

#include "mlog/types/SLog.hpp" 
#include "mlog/types/ELog.hpp"

#include <memory>
#include <source_location>
#include <format>

#ifdef _WIN32
    #ifdef BUILDING_MLOG
        #define MLOG_API __declspec(dllexport)
    #else
        #define MLOG_API __declspec(dllimport)
    #endif
#else
    #define MLOG_API __attribute__((visibility("default")))
#endif

namespace ei::mlog
{
    class Pattern;
    class FileExport;
    class Pipeline;

    class MLOG_API Log
    {
    public:
        Log();
        ~Log() = default;

        Log(const Log &) = delete;
        Log &operator=(const Log &) = delete;

        static Log& getInstance();

        void importLog(const char *path);
        void setPattern(const char *fmt);

        void write(const LogLevel &level, const std::string &message, const std::source_location &location);

        template<typename...Args>
        static inline void write(const LogLevel &level, const std::source_location &location, std::format_string<Args...> fmt, Args &&...args)
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            getInstance().write(level, message, location);
        }

    private:
        LogOptions _opts;
        std::shared_ptr<Pattern> _pattern;
        std::shared_ptr<FileExport> _fileExport;
        std::unique_ptr<Pipeline> _pipeline;
    };
}

#define LOG_HELPER(level, ...) ei::mlog::Log::write(level, std::source_location::current(), __VA_ARGS__)

#define ilog(...) LOG_HELPER(ei::mlog::LogLevel::INFO, __VA_ARGS__)
#define elog(...) LOG_HELPER(ei::mlog::LogLevel::ERROR, __VA_ARGS__)

#endif // LOG_HPP