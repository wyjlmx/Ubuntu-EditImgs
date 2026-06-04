#ifndef FILE_EXPORT_HPP
#define FILE_EXPORT_HPP

#include "mlog/types/SLog.hpp"

#include <unordered_map>
#include <fstream>
#include <string>

namespace ei::mlog
{
    class LogOptions;

    class FileExport
    {
    public:
        FileExport() = default;
        ~FileExport();

        FileExport(const FileExport&) = delete;
        FileExport& operator=(const FileExport&) = delete;

        void configure(const LogOptions& opts);
        void exportFile(const LogPattern &pattern, LogBuffer &logBuffer);

    private:
        std::string _logDirPath;
        std::unordered_map<std::string, std::ofstream> _ofsMap;
    };
}

#endif // FILE_EXPORT_HPP