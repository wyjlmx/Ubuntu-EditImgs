#include "Pipeline.hpp"
#include "process/Pattern.hpp"
#include "process/FileExport.hpp"

namespace ei::mlog
{
    Pipeline::Pipeline(std::shared_ptr<Pattern> pattern, std::shared_ptr<FileExport> fileExport)
        : _pattern(std::move(pattern)),
          _fileExport(std::move(fileExport)),
          _stopFlag{false}
    {
        _workThread = std::thread(&Pipeline::processQueue, this);
    }

    Pipeline::~Pipeline()
    {
        _stopFlag.store(true);
        _queue.stopQueue();

        if(_workThread.joinable())
            _workThread.join();
    }

    void Pipeline::configure(const LogOptions &opts)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _pattern->configure(opts);
        _fileExport->configure(opts);
    }

    void Pipeline::log(const LogPattern &pattern, LogBuffer &logBuffer)
    {
        // _pattern->formatTo(pattern, logBuffer);
        // _fileExport->exportFile(pattern, logBuffer);

        _queue.waitPush(LogTask{pattern, std::move(logBuffer)});
    }

    void Pipeline::processQueue()
    {
        std::vector<LogTask> localTasks;

        while (_queue.waitPop(localTasks, _stopFlag))
        {
            for (auto &task : localTasks)
            {
                _pattern->formatTo(task.pattern, task.logBuffer);
                _fileExport->exportFile(task.pattern, task.logBuffer);
            }

            localTasks.clear();
        }
    }
}