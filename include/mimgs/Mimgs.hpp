#ifndef IMAGES_HPP
#define IMAGES_HPP

#if defined(_MSC_VER)
#ifdef MIMGS_EXPORTS
#define MIMGS_API __declspec(dllexport)
#else
#define MIMGS_API __declspec(dllimport)
#endif
#else
#define MIMGS_API __attribute__((visibility("default")))
#endif

#include "mimgs/interfaces/IImages.hpp"
// #include "mimgs/common/Common.hpp"

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <variant>
#include <cstdint>

namespace ei::mimgs
{
    struct ScaleStep
    {
        std::string inputKey;
        size_t tarWidth{0};
        size_t tarHeight{0};
    };

    struct RotateStep
    {
        std::string inputKey;
        float angle{0.0f};
    };

    struct PuzzleStep
    {
        std::vector<std::string> inputKeys;
        uint32_t rows{0};
        uint32_t cols{0};
        uint32_t cellWidth{0};
        uint32_t cellHeight{0};
        uint32_t padding{0};
        std::vector<uint8_t> bgColor{0, 0, 0, 0};
    };

    // 统一管道步骤包装器
    struct PipelineStep
    {
        std::string outputKey;
        std::variant<ScaleStep, RotateStep, PuzzleStep> params;
    };

    // 顶级流水线任务参数结构体：完全采用标准库类型，解除对 OperatorParams 的继承
    struct MIMGS_API PipelineParams
    {
        std::string outputKey;             // 最终输出资产标识
        std::vector<std::string> inputKey; // 初始输入的源图像标识列表
        std::vector<PipelineStep> steps;   // 顺序执行的管道步骤
    };

    class MIMGS_API Images : public IImages
    {
    public:
        Images() = default;
        ~Images() override;

        bool execute(const PipelineParams &params, const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs, std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs) override;
        void clearCache() override;
        void setCacheCapacity(size_t capacity) override;

    private:
        class Impl;
        std::unique_ptr<Impl> _impl;
    };
}

#endif // IMAGES_HPP