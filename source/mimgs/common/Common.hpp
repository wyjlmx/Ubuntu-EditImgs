#ifndef COMMON_HPP
#define COMMON_HPP

#include "mlog/Log.hpp"

#include <stdexcept>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ei::mimgs
{
    class ProcessingException : public std::runtime_error
    {
    public:
        explicit ProcessingException(const std::string &msg)
            : std::runtime_error("[Internal Error] " + msg) {}
    };

    enum class PixelFormat
    {
        RGBA8888,
        RGB888
    };

    // 内部图像基础资源
    struct ImageAsset
    {
        std::string id;
        uint32_t width{0};
        uint32_t height{0};
        PixelFormat format{PixelFormat::RGBA8888};
        std::unique_ptr<uint8_t[]> data{nullptr};

        ImageAsset(std::string_view asset_id, uint32_t w, uint32_t h, PixelFormat fmt)
            : id(asset_id), width(w), height(h), format(fmt)
        {
            size_t size = static_cast<size_t>(w) * h * (fmt == PixelFormat::RGBA8888 ? 4 : 3);
            data = std::make_unique<uint8_t[]>(size);
        }

        ImageAsset(const ImageAsset &) = delete;
        ImageAsset &operator=(const ImageAsset &) = delete;
        ImageAsset(ImageAsset &&) noexcept = default;
        ImageAsset &operator=(ImageAsset &&) noexcept = default;
    };

    // 所有算子参数的基类
    struct OperatorParams
    {
        std::string outputKey;
        virtual ~OperatorParams() = default;
        virtual void Validate() const
        {
            if (outputKey.empty())
                throw ProcessingException("output_key is empty");
        }
    };

    // 内部共享上下文
    class IContext
    {
    private:
        std::unordered_map<std::string, std::shared_ptr<ImageAsset>> _assets;

    public:
        void setAsset(std::shared_ptr<ImageAsset> asset)
        {
            if (!asset || asset->id.empty())
            {
                throw ProcessingException("Null or invalid asset.");
            }
            _assets[asset->id] = std::move(asset);
        }

        std::shared_ptr<ImageAsset> getAsset(const std::string &id) const
        {
            auto it = _assets.find(id);
            if (it != _assets.end())
                return it->second;
            return nullptr;
        }
    };

    // 统一算子契约接口
    class IOperator
    {
    public:
        virtual ~IOperator() = default;
        virtual std::string_view getName() const noexcept = 0;
        virtual void execute(IContext &context, const OperatorParams &params) = 0;

    protected:
        template <typename T>
        const T &CastParams(const OperatorParams &params) const
        {
            const auto *derived = dynamic_cast<const T *>(&params);
            if (!derived)
            {
                throw ProcessingException(std::string(getName()) + ": Parameter mismatch.");
            }
            return *derived;
        }
    };

}

#endif // COMMON_HPP