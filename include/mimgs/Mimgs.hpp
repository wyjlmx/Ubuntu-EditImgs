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

#include "mimgs/common/Common.hpp"

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

    class Images
    {
    public:
        Images() = default;
        ~Images() = default;
    };
}

#endif // IMAGES_HPP