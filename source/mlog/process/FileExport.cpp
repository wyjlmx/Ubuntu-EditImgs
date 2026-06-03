#include "FileExport.hpp"
#include "mlog/types/ELog.hpp"

#include <filesystem>

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

    void FileExport::importLog(const char *path)
    {
        if (std::filesystem::exists(path))
            std::filesystem::create_directories(path);

        _logDirPath = path;
    }

    void FileExport::exportLog(const LogPattern &pattern, LogBuffer &logBuffer)
    {
        std::string_view leavel = GetLeavelToString(pattern.level);
        auto& ofs = _ofsMap[std::string(leavel)];

        if(!ofs.is_open())
        {
            auto fileName = _logDirPath + std::string(leavel) + ".log";
            ofs.open(fileName, std::ios::out | std::ios::app);

            if(!ofs.is_open())
                return;
        }

        ofs << logBuffer.buffer;
        ofs << '\n';
    }
}