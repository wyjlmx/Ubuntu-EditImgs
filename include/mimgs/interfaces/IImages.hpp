#ifndef I_IMAGES_HPP
#define I_IMAGES_HPP

#include <memory>
#include <unordered_map>
#include <string>

namespace ei::mimgs
{
    struct ImageAsset;
    struct PipelineParams;

    class IImages
    {
    public:
        virtual ~IImages() = default;

        virtual bool execute(const PipelineParams& params, const std::unordered_map<std::string, std::shared_ptr<ImageAsset>>& inputs, std::unordered_map<std::string, std::shared_ptr<ImageAsset>>& outputs) = 0;
        virtual void clearCache() = 0;
        virtual void setCacheCapacity(size_t capacity) = 0;
    };
}

#endif // I_IMAGES_HPP