#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "mlog/types/SLog.hpp"
#include "ConcurrentQueue.hpp"

#include <memory>
#include <thread>
#include <atomic>

namespace ei::mlog
{
    class Pattern;
    class FileExport;

    class Pipeline
    {
    public:
        struct LogTask
        {
            LogPattern pattern;
            LogBuffer logBuffer;
        };

        Pipeline(std::shared_ptr<Pattern> pattern, std::shared_ptr<FileExport> fileExport);
        ~Pipeline();

        Pipeline(const Pipeline &) = delete;
        Pipeline &operator=(const Pipeline &) = delete;

        void configure(const LogOptions &opts);
        void log(const LogPattern &pattern, LogBuffer &logBuffer);

    private:
        void processQueue();

    private:
        std::shared_ptr<Pattern> _pattern;
        std::shared_ptr<FileExport> _fileExport;

        ConcurrentQueue<LogTask> _queue;
        std::atomic<bool> _stopFlag;
        std::thread _workThread;
        std::mutex _mutex;
    };
}

#endif // PIPELINE_HPP