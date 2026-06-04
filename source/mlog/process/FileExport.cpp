#include "FileExport.hpp"
#include "mlog/types/ELog.hpp"

#include <filesystem>
#include <iostream>

namespace ei::mlog
{
    namespace
    {
        std::string_view GetLeavelToString(LogLevel level)
        {
            static constexpr std::string_view mapName[] = {"Info", "Error"};
            auto idx = static_cast<size_t>(level);
            return (idx < 2) ? mapName[idx] : "Unknown";
        }
    }

    FileExport::~FileExport()
    {
        for(auto& pair : _ofsMap)
        {
            if(pair.second.is_open())
            {
                pair.second.flush();
                pair.second.close();
            }
        }
    }

    void FileExport::configure(const LogOptions &opts)
    {
        if (_logDirPath == opts.logDirPath)
            return;

        _logDirPath = opts.logDirPath;

        try
        {
            if(!_logDirPath.empty())
                std::filesystem::create_directories(_logDirPath);
        }
        catch(const std::filesystem::filesystem_error& e)
        {
            std::cerr << "[mlog] Failed to create directories: " << e.what() << std::endl;
        }
        
    }

    void FileExport::exportFile(const LogPattern &pattern, LogBuffer &logBuffer)
    {
        std::string_view leavel = GetLeavelToString(pattern.level);
        std::string key(leavel);

        auto &ofs = _ofsMap[key];

        if (!ofs.is_open())
        {
            std::filesystem::path dirPath(_logDirPath);
            std::filesystem::path filePath = dirPath / (key + ".log");

            ofs.open(filePath, std::ios::out | std::ios::app);

            if (!ofs.is_open())
                return;
        }

        ofs << logBuffer.buffer;
        ofs << '\n';
    }
}