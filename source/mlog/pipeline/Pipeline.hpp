#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "mlog/types/SLog.hpp"

#include <memory>
#include <map>

namespace ei::mlog
{
    class Pattern;
    class FileExport;

    class Pipeline
    {
    public:
        Pipeline(std::shared_ptr<Pattern> pattern, std::shared_ptr<FileExport> fileExport);
        ~Pipeline() = default;

        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        void configure(const LogOptions &opts);
        void log(const LogPattern &pattern, LogBuffer &logBuffer);

    private:
        std::shared_ptr<Pattern> _pattern;
        std::shared_ptr<FileExport> _fileExport;
    };
}

#endif // PIPELINE_HPP