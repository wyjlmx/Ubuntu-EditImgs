#include "Utils.hpp"
#include "common/Common.hpp"
#include <opencv2/imgproc.hpp>

namespace ei::mimgs
{
    int GetChannelCount(PixelFormat fmt) noexcept
    {
        return (fmt == PixelFormat::RGB888) ? 3 : 4;
    }

    int MapToCVType(PixelFormat format) noexcept
    {
        return (format == PixelFormat::RGBA8888) ? CV_8UC4 : CV_8UC3;
    }

    std::shared_ptr<ImageAsset> ConvertFormat(const ImageAsset &src, PixelFormat tarFormat)
    {
        if (!src.data)
            throw ProcessingException("ConvertFormat: Source data is null.");

        if (src.format == tarFormat)
        {
            auto copy = std::make_shared<ImageAsset>(src.id + "_copy", src.width, src.height, src.format);
            size_t totalBytes = static_cast<size_t>(src.width) * src.height * GetChannelCount(src.format);
            std::memcpy(copy->data.get(), src.data.get(), totalBytes);
            return copy;
        }

        cv::Mat srcMat(src.height, src.width, MapToCVType(src.format), src.data.get());
        cv::Mat dstMat;

        if (src.format == PixelFormat::RGB888 && tarFormat == PixelFormat::RGBA8888)
            cv::cvtColor(srcMat, dstMat, cv::COLOR_RGB2RGBA);
        else if(src.format == PixelFormat::RGBA8888 && tarFormat == PixelFormat::RGB888)
            cv::cvtColor(srcMat, dstMat, cv::COLOR_RGBA2RGB);
        else
            throw ProcessingException("ConvertFormat: Unsupported conversion path.");

        auto dstAsset = std::make_shared<ImageAsset>(src.id + "_conv", src.width, src.height, tarFormat);
        size_t rowBytes = static_cast<size_t>(src.width) * GetChannelCount(tarFormat);

        if(dstMat.isContinuous())
            std::memcpy(dstAsset->data.get(), dstMat.data, rowBytes * src.height);
        else
            for(int r = 0; r < dstMat.rows;++r)
                std::memcpy(dstAsset->data.get() + r*rowBytes, dstMat.ptr(r), rowBytes);

        return dstAsset;
    }
}