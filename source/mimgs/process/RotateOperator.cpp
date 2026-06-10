#include "RotateOperator.hpp"
#include "common/Utils.hpp"

#include <string>
#include <opencv2/opencv.hpp>

namespace ei::mimgs
{
    void RotateParams::Validate() const
    {
        OperatorParams::Validate();
        if (inputKey.empty())
            throw ProcessingException("Scale: inputKey empty.");
    }

    std::string RotateParams::GetCacheSignature() const
    {
        return "rotate_" + inputKey + "_" + std::to_string(angle);
    }

    std::string_view RotateOperator::getName() const noexcept
    {
        return "rotate";
    }

    void RotateOperator::execute(IContext &context, const OperatorParams &params)
    {
        const auto &rotateParams = CastParams<RotateParams>(params);
        rotateParams.Validate();

        auto srcAsset = context.getAsset(rotateParams.inputKey);
        if (!srcAsset)
            throw ProcessingException("Scale:Source asset '" + rotateParams.inputKey + "' not found in context");

        if (!srcAsset->data)
            throw ProcessingException("Scale: Source asset data is null");

        int cvType = MapToCVType(srcAsset->format);
        cv::Mat srcMat(srcAsset->height, srcAsset->width, cvType, srcAsset->data.get());

        cv::Point2f center(srcMat.cols / 2.0f, srcMat.rows / 2.0f);
        cv::Mat rot = cv::getRotationMatrix2D(center, rotateParams.angle, 1.0);

        cv::Size2f srcSize(static_cast<float>(srcMat.cols), static_cast<float>(srcMat.rows));
        cv::Rect2f bbox = cv::RotatedRect(cv::Point2f(), srcSize, rotateParams.angle).boundingRect2f();

        cv::Size tarSize(static_cast<int>(std::round(bbox.width)), static_cast<int>(std::round(bbox.height)));
        rot.at<double>(0, 2) += tarSize.width / 2.0 - center.x;
        rot.at<double>(1, 2) += tarSize.height / 2.0 - center.y;

        cv::Mat dstMat;
        cv::warpAffine(srcMat, dstMat, rot, tarSize, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));

        auto dstAsset = std::make_shared<ImageAsset>(rotateParams.outputKey, static_cast<uint32_t>(tarSize.width), static_cast<uint32_t>(tarSize.height), srcAsset->format);    
        size_t rowBytes = static_cast<size_t>(tarSize.width) * GetChannelCount(dstAsset->format);

        if(dstMat.isContinuous())
        {
            std::memcpy(dstAsset->data.get(), dstMat.data, rowBytes * tarSize.height);
        }
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