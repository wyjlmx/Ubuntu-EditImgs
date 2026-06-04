#ifndef LOG_HPP
#define LOG_HPP

#include "mlog/types/SLog.hpp" 
#include "mlog/types/ELog.hpp"

#include <memory>
#include <source_location>

namespace ei::mlog
{
    class Pattern;
    class FileExport;
    class Pipeline;

    class Log
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

    private:
        LogOptions _opts;
        std::shared_ptr<Pattern> _pattern;
        std::shared_ptr<FileExport> _fileExport;
        std::unique_ptr<Pipeline> _pipeline;
    };
}

#endif // LOG_HPP