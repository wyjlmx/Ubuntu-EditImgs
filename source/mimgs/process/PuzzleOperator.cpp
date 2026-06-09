#include "PuzzleOperator.hpp"
#include "common/Utils.hpp"

#include <opencv2/opencv.hpp>

namespace ei::mimgs
{
    void PuzzleParams::Validate() const
    {
        OperatorParams::Validate();
        if (inputKeys.empty())
            throw ProcessingException("Scale: inputKey empty.");
        if (rows <= 0 || cols <= 0 || cellWidth <= 0 || cellHeight <= 0 || padding <= 0)
            throw ProcessingException("Scale: invalid size.");
    }

    std::string_view PuzzleOperator::getName() const noexcept
    {
        return "puzzle";
    }

    void PuzzleOperator::execute(IContext &context, const OperatorParams &params)
    {
        const auto &puzzleParams = CastParams<PuzzleParams>(params);
        puzzleParams.Validate();

        uint32_t totalWidth = puzzleParams.cols * puzzleParams.cellWidth + (puzzleParams.cols > 1 ? (puzzleParams.cols - 1) * puzzleParams.padding : 0);
        uint32_t totalHeight = puzzleParams.rows * puzzleParams.cellHeight + (puzzleParams.rows > 1 ? (puzzleParams.rows - 1) * puzzleParams.padding : 0);

        PixelFormat outputFormat = PixelFormat::RGBA8888;
        int outputCVType = MapToCVType(outputFormat);
        int outputChannels = GetChannelCount(outputFormat);

        cv::Mat dstMat(static_cast<int>(totalHeight), static_cast<int>(totalWidth), outputCVType);
        cv::Scalar cvBgColor(puzzleParams.bgColor[2], puzzleParams.bgColor[1], puzzleParams.bgColor[0], puzzleParams.bgColor[3]);
        dstMat.setTo(cvBgColor);

        size_t currentInputIndex = 0;
        for (uint32_t r = 0; r < puzzleParams.rows; ++r)
        {
            for (uint32_t c = 0; c < puzzleParams.cols; ++c)
            {
                if (currentInputIndex >= puzzleParams.inputKeys.size())
                    break;

                const std::string &inputKey = puzzleParams.inputKeys[currentInputIndex];
                auto srcAsset = context.getAsset(inputKey);

                if (!srcAsset || !srcAsset->data)
                {
                    currentInputIndex++;
                    continue;
                }

                cv::Mat currentSrcMat(srcAsset->height, srcAsset->width, MapToCVType(srcAsset->format), srcAsset->data.get());
                if (srcAsset->format == PixelFormat::RGB888 && outputFormat == PixelFormat::RGBA8888)
                {
                    cv::Mat tempConvertedMat;
                    cv::cvtColor(currentSrcMat, tempConvertedMat, cv::COLOR_RGB2RGBA);
                    currentSrcMat = tempConvertedMat;
                }

                cv::Mat resizedSrcMat;
                cv::resize(currentSrcMat, resizedSrcMat, cv::Size(static_cast<int>(puzzleParams.cellWidth), static_cast<int>(puzzleParams.cellHeight)), 0, 0, cv::INTER_LINEAR);

                int xOffset = static_cast<int>(c * (puzzleParams.cellWidth + puzzleParams.padding));
                int yOffset = static_cast<int>(r * (puzzleParams.cellHeight + puzzleParams.padding));

                cv::Rect roi(xOffset, yOffset, resizedSrcMat.cols, resizedSrcMat.rows);

                resizedSrcMat.copyTo(dstMat(roi));
                currentInputIndex++;
            }

            if (currentInputIndex >= puzzleParams.inputKeys.size())
                break;
        }

        auto dstAsset = std::make_shared<ImageAsset>(puzzleParams.outputKey, totalWidth, totalHeight, outputFormat);

        size_t totalBytes = static_cast<size_t>(totalWidth) * totalHeight * outputChannels;
        if (dstMat.isContinuous())
            std::memcpy(dstAsset->data.get(), dstMat.data, totalBytes);
        else
        {
            size_t rowBytes = static_cast<size_t>(totalWidth) * outputChannels;
            for (int r = 0; r < dstMat.rows; ++r)
            {
                std::memcpy(dstAsset->data.get() + r * rowBytes, dstMat.ptr(r), rowBytes);
            }
        }

        context.setAsset(std::move(dstAsset));
    }
}