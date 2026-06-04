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

        std::filesystem::path GetExecutableDirectory()
        {
            try
            {
                // 解析 "/proc/self/exe" 获取绝对路径，并取其父级目录
                return std::filesystem::canonical("/proc/self/exe").parent_path();
            }
            catch (const std::filesystem::filesystem_error &)
            {
                return std::filesystem::current_path();
            }
        }
    }

    FileExport::~FileExport()
    {
        for (auto &pair : _ofsMap)
        {
            if (pair.second.is_open())
            {
                pair.second.flush();
                pair.second.close();
            }
        }
    }

    void FileExport::configure(const LogOptions &opts)
    {
        std::filesystem::path resolvedPath(opts.logDirPath);

        if (resolvedPath.is_relative())
            resolvedPath = GetExecutableDirectory() / resolvedPath;

        std::string resolvedPathStr = resolvedPath.string();
        if (_logDirPath == resolvedPathStr)
            return;

        _logDirPath = std::move(resolvedPathStr);

        for(auto &pair : _ofsMap)
        {
            if(pair.second.is_open())
                pair.second.close();
        }
        _ofsMap.clear();

        try
        {
            if (!_logDirPath.empty())
                std::filesystem::create_directories(_logDirPath);
        }
        catch (const std::filesystem::filesystem_error &e)
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

            // ofs.open(filePath, std::ios::out | std::ios::app);  // 追加
            ofs.open(filePath, std::ios::out | std::ios::trunc);  // 覆盖

            if (!ofs.is_open())
                return;
        }

        ofs << logBuffer.buffer;
        ofs << '\n';
    }
}