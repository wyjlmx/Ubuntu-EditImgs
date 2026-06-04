#include "Pipeline.hpp"
#include "process/Pattern.hpp"
#include "process/FileExport.hpp"

namespace ei::mlog
{
    Pipeline::Pipeline(std::shared_ptr<Pattern> pattern, std::shared_ptr<FileExport> fileExport)
        : _pattern(pattern), _fileExport(fileExport) {}

    void Pipeline::configure(const LogOptions &opts)
    {
        _pattern->configure(opts);
        _fileExport->configure(opts);
    }

    void Pipeline::log(const LogPattern &pattern, LogBuffer &logBuffer)
    {
        _pattern->formatTo(pattern, logBuffer);
        _fileExport->exportFile(pattern, logBuffer);
    }
}