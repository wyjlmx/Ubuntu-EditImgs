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
#include "common/Common.hpp"

#include <vector>
#include <string>
#include <memory>

namespace ei::mimgs
{
    struct MIMGS_API PipelineParams : public OperatorParams
    {
        std::vector<std::string> inputKey;
        std::vector<std::shared_ptr<OperatorParams>> steps; // 直接存放 ScaleParams / RotateParams 的智能指针

        void Validate() const override;
        std::string GetCacheSignature() const override;
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