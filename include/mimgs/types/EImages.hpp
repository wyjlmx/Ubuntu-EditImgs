#ifndef E_IMAGES_HPP
#define E_IMAGES_HPP

#include <string>
#include <string_view>
#include <memory>
#include <cstdint>
#include <cstring>

namespace ei::mimgs
{
    enum class PixelFormat
    {
        RGBA8888,
        RGB888
    };

    // 暴露给客户端的纯数据资产包，不含任何 OpenCV 级实现细节
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
}

#endif // E_IMAGES_HPP