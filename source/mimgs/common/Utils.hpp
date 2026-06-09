#ifndef UTILS_HPP
#define UTILS_HPP

#include <memory>

namespace ei::mimgs
{
    enum class PixelFormat;
    struct ImageAsset;

    // 获取通道数（基础元数据转换）
    int GetChannelCount(PixelFormat fmt) noexcept;

    // 将内部格式映射为 OpenCV 的数据类型 (如 CV_8UC4, CV_8UC3)
    int MapToCVType(PixelFormat format) noexcept;

    // 核心格式转换函数：将 ImageAsset 转换为目标格式（如 RGB888 <-> RGBA8888）
    std::shared_ptr<ImageAsset> ConvertFormat(const ImageAsset& src, PixelFormat tarFormat);
}

#endif // UTILS_HPP