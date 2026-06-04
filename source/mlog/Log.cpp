#include "mlog/Log.hpp"
#include "process/Pattern.hpp"
#include "process/FileExport.hpp"
#include "pipeline/Pipeline.hpp"

#include <thread>

namespace ei::mlog
{
    namespace
    {
        std::string  GetThreadIdString()
        {
            auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return std::to_string(tid);
        }
    }

    Log::Log()
    {
        _pattern = std::make_shared<Pattern>();
        _fileExport = std::make_shared<FileExport>();

        _pipeline = std::make_unique<Pipeline>(_pattern, _fileExport);
        _pipeline->configure(_opts);
    }

    Log &Log::getInstance()
    {
        static Log instance;
        return instance;
    }

    void Log::importLog(const char *path)
    {
        if (path == nullptr)
            return;

        _opts.logDirPath = path;
        _pipeline->configure(_opts);
    }

    void Log::setPattern(const char *fmt)
    {
        if (fmt == nullptr)
            return;

        _opts.patternFmt = fmt;
        _pipeline->configure(_opts);
    }

    void Log::write(const LogLevel &level, const std::string &message, const std::source_location &location)
    {
        LogPattern logPattern;
        logPattern.level = level;
        logPattern.message = message;
        logPattern.location = location;
        logPattern.threadId = GetThreadIdString();

         thread_local LogBuffer logBuffer;
        _pipeline->log(logPattern, logBuffer);
    }
}
