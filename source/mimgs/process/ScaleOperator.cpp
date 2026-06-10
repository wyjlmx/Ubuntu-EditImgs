#include "ScaleOperator.hpp"
#include "common/Utils.hpp"

#include <string>
#include <opencv2/opencv.hpp>

namespace ei::mimgs
{
    std::string_view ScaleOperator::getName() const noexcept
    {
        return "scale";
    }

    void ScaleParams::Validate() const
    {
        OperatorParams::Validate();
        if (inputKey.empty())
            throw ProcessingException("Scale: inputKey empty.");
        if (tarWidth <= 0 || tarHeight <= 0)
            throw ProcessingException("Scale: invalid size.");
    }

    std::string ScaleParams::GetCacheSignature() const
    {
        return "scale_" + inputKey + "_" + std::to_string(tarWidth) + "x" + std::to_string(tarHeight);
    }

    void ScaleOperator::execute(IContext &context, const OperatorParams &params)
    {
        const auto &scaleParams = CastParams<ScaleParams>(params);
        scaleParams.Validate();

        auto srcAsset = context.getAsset(scaleParams.inputKey);
        if (!srcAsset)
            throw ProcessingException("Scale:Source asset '" + scaleParams.inputKey + "' not found in context");

        if (!srcAsset->data)
            throw ProcessingException("Scale: Source asset data is null");

        int cvType = MapToCVType(srcAsset->format);
        cv::Mat srcMat(srcAsset->height, srcAsset->width, cvType, srcAsset->data.get());

        cv::Mat dstMat;
        cv::Size tarSize(static_cast<int>(scaleParams.tarWidth), static_cast<int>(scaleParams.tarHeight));
        cv::resize(srcMat, dstMat, tarSize, 0, 0, cv::INTER_LINEAR);

        auto dstAsset = std::make_shared<ImageAsset>(scaleParams.outputKey, scaleParams.tarWidth, scaleParams.tarHeight, srcAsset->format);
        size_t rowBytes = static_cast<size_t>(scaleParams.tarWidth) * GetChannelCount(srcAsset->format);

        if (dstMat.isContinuous())
            std::memcpy(dstAsset->data.get(), dstMat.data, rowBytes * scaleParams.tarHeight);
        else
        {
            for(int r = 0;r < dstMat.rows; ++r)
            {
                std::memcpy(dstAsset->data.get() + r * rowBytes, dstMat.ptr(r), rowBytes);
            }
        }
 
        context.setAsset(std::move(dstAsset));
    }

}
