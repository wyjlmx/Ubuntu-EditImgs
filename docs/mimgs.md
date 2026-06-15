**mimgs 模块代码汇总**

**概述**
- **模块说明**: 本文档汇总并展示仓库中 `mimgs` 模块已完成的源码，包含算子（Scale/Rotate/Puzzle）、流水线与注册表、LRU 缓存与工具函数等，便于他人阅读与复用。

**包含文件**
- [include/mimgs/Mimgs.hpp](include/mimgs/Mimgs.hpp)
- [include/mimgs/interfaces/IImages.hpp](include/mimgs/interfaces/IImages.hpp)
- [include/mimgs/types/EImages.hpp](include/mimgs/types/EImages.hpp)
- [source/mimgs/Mimgs.cpp](source/mimgs/Mimgs.cpp)
- [source/mimgs/CMakeLists.txt](source/mimgs/CMakeLists.txt)
- [source/mimgs/pipeline/OperatorRegistry.hpp](source/mimgs/pipeline/OperatorRegistry.hpp)
- [source/mimgs/pipeline/OperatorRegistry.cpp](source/mimgs/pipeline/OperatorRegistry.cpp)
- [source/mimgs/pipeline/Pipeline.hpp](source/mimgs/pipeline/Pipeline.hpp)
- [source/mimgs/process/ScaleOperator.hpp](source/mimgs/process/ScaleOperator.hpp)
- [source/mimgs/process/ScaleOperator.cpp](source/mimgs/process/ScaleOperator.cpp)
- [source/mimgs/process/RotateOperator.hpp](source/mimgs/process/RotateOperator.hpp)
- [source/mimgs/process/RotateOperator.cpp](source/mimgs/process/RotateOperator.cpp)
- [source/mimgs/process/PuzzleOperator.hpp](source/mimgs/process/PuzzleOperator.hpp)
- [source/mimgs/process/PuzzleOperator.cpp](source/mimgs/process/PuzzleOperator.cpp)
- [source/mimgs/common/Common.hpp](source/mimgs/common/Common.hpp)
- [source/mimgs/common/Utils.hpp](source/mimgs/common/Utils.hpp)
- [source/mimgs/common/Utils.cpp](source/mimgs/common/Utils.cpp)
- [source/mimgs/common/LRUImageCache.hpp](source/mimgs/common/LRUImageCache.hpp)
- [source/mimgs/common/LRUImageCache.cpp](source/mimgs/common/LRUImageCache.cpp)

---

**接口与类型说明**

`include/mimgs/Mimgs.hpp` - 模块对外参数与基础类型定义：

```cpp
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
```

`include/mimgs/interfaces/IImages.hpp` 与 `include/mimgs/types/EImages.hpp` 当前为空（占位）。

---

**核心实现**

`source/mimgs/common/Common.hpp` - 公共类型与接口契约：

```cpp
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

        virtual std::string GetCacheSignature() const = 0;
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
```

`source/mimgs/common/Utils.hpp` / `Utils.cpp` - OpenCV 类型映射与格式转换：

```cpp
// Utils.hpp
#ifndef UTILS_HPP
#define UTILS_HPP

#include <memory>

namespace ei::mimgs
{
    enum class PixelFormat;
    struct ImageAsset;

    int GetChannelCount(PixelFormat fmt) noexcept;
    int MapToCVType(PixelFormat format) noexcept;
    std::shared_ptr<ImageAsset> ConvertFormat(const ImageAsset& src, PixelFormat tarFormat);
}

#endif // UTILS_HPP
```

```cpp
// Utils.cpp
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
```

`source/mimgs/common/LRUImageCache.hpp` / `.cpp` - 简易线程安全 LRU 缓存：

```cpp
// LRUImageCache.hpp
#ifndef LRU_IMAGE_CACHE_HPP
#define LRU_IMAGE_CACHE_HPP

#include "common/Common.hpp"

#include <unordered_map>
#include <mutex>
#include <list>
#include <memory>

namespace ei::mimgs
{
    class LRUImageCache
    {
    public:
        static LRUImageCache &getInstance();

        void setCapacity(size_t capacity);
        std::shared_ptr<ImageAsset> getCapacity(const std::string &key);
        void putCapacity(const std::string &key, std::shared_ptr<ImageAsset> asset);
        void clearCapacity();

    private:
        LRUImageCache(size_t capacity) : _capacity(capacity) {}
        ~LRUImageCache() = default;

        LRUImageCache(const LRUImageCache &) = delete;
        LRUImageCache &operator=(const LRUImageCache &) = delete;

        void evict()
        {
            while(_list.size() > _capacity)
            {
                auto last = _list.back();
                _map.erase(last.first);
                _list.pop_back();
            }
        }

    private:
        size_t _capacity;
        std::mutex _mutex;
        std::list<std::pair<std::string, std::shared_ptr<ImageAsset>>> _list;
        std::unordered_map<std::string, decltype(_list.begin())> _map;
    };
}

#endif // LRU_IMAGE_CACHE_HPP
```

```cpp
// LRUImageCache.cpp
#include "LRUImageCache.hpp"

namespace ei::mimgs
{
    LRUImageCache &LRUImageCache::getInstance()
    {
        static LRUImageCache instance(15);
        return instance;
    }

    void LRUImageCache::setCapacity(size_t capacity)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _capacity = capacity;
        evict();
    }

    std::shared_ptr<ImageAsset> LRUImageCache::getCapacity(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _map.find(key);
        if (it == _map.end())
            return nullptr;

        _list.splice(_list.begin(), _list, it->second);

        return it->second->second;
    }

    void LRUImageCache::putCapacity(const std::string &key, std::shared_ptr<ImageAsset> asset)
    {
        if(!asset) return;

        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _map.find(key);

        if(it != _map.end())
        {
            it->second->second = asset;
            _list.splice(_list.begin(), _list, it->second);
            return;
        }

        _list.push_front({key, asset});
        _map[key] = _list.begin();

        evict();
    }

    void LRUImageCache::clearCapacity()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _map.clear();
        _list.clear();
    }
}
```

---

**算子与流水线**

`OperatorRegistry` - 单例算子注册与查询（源码见上方文件链接）。

算子实现包括 `scale`、`rotate`、`puzzle` 三类算子，关键逻辑已在 `source/mimgs/process` 中实现，示例：

`RotateOperator::execute`（摘录）使用 OpenCV 计算旋转矩阵并写回 `ImageAsset`：相关实现见 `source/mimgs/process/RotateOperator.cpp`。

---

**空文件与未实现项**
- `include/mimgs/interfaces/IImages.hpp` - 空
- `include/mimgs/types/EImages.hpp` - 空
- `source/mimgs/Mimgs.cpp` - 空（模块入口/桥接处，待实现或为占位）

---

**已包含的源码文件清单**
- 上文“包含文件”部分列出了本文档包含并嵌入/引用的完整文件列表。

---

如果你需要我把每个文件的完整源码都逐一原样嵌入到文档中（目前保留了关键文件的全部源码并以链接引用其余），我可以将文档改为把所有源码全文完整列出并按文件索引环节。要我现在把所有文件都完整展开吗？
