# 图像处理 API SDK 架构与开发文档

为了实现企业级 C++ 项目的**物理编译解耦**、**接口稳定（ABI 稳定性）**以及**极佳的并发性能与资源控制**，本项目设计并实现了一套工业级的图像处理 SDK（`mimgs` 模块）。

在本项目设计中，所有的核心组件都遵循高内聚低耦合的原则，划分为以下层级：
1. **门面层 (Facade/SDK Layer)**：客户端可见的头文件，采用 **Pimpl 编译器防火墙模式** 实现物理隔离与二进制接口（ABI）保护。
2. **调度流水线层 (Orchestration/Pipeline Layer)**：接收高层配置参数，将其拆分为一系列算子步骤，管理运行时上下文，并支持中间结果缓存拦截。
3. **业务算子层 (Operator/Process Layer)**：具体的缩放 (`scale`)、旋转 (`rotate`) 和拼图 (`puzzle`) 底层 OpenCV 核心算法实现。
4. **共享与通用层 (Common & Utils Layer)**：提供自定义异常、基础图像资产结构、上下文黑板容器、线程安全的单例缓存（`LRUImageCache`）以及单例算子注册表（`OperatorRegistry`）。

---

### 📂 物理项目结构概览

```text
.
├── include/                              # SDK 外部公共头文件（用户可见）
│   └── mimgs/
│       ├── Mimgs.hpp                     # SDK 门面类声明 (Pimpl 模式 & 管道参数定义)
│       ├── interfaces/
│       │   └── IImages.hpp               # 外部统一接口定义
│       └── types/
│           └── EImages.hpp               # 公共枚举与基础类型占位
└── source/                               # 内部实现代码（用户不可见）
    ├── mlog/                             # 异步日志模块
    └── mimgs/
        ├── CMakeLists.txt                # 编译配置
        ├── Mimgs.cpp                     # 门面实现类 (Impl 与桥接实现) 【已补齐】
        ├── common/
        │   ├── Common.hpp                # 内部基础定义 (ImageAsset, IContext)
        │   ├── LRUImageCache.hpp/cpp     # 线程安全 LRU 图像缓存
        │   └── Utils.hpp/cpp             # OpenCV 格式转换与工具函数
        ├── pipeline/
        │   ├── OperatorRegistry.hpp/cpp  # 算子注册表
        │   ├── Pipeline.hpp              # 调度流水线声明
        │   └── Pipeline.cpp              # 调度流水线具体实现 【已补齐】
        └── process/
            ├── ScaleOperator.hpp/cpp     # 缩放算子 (OpenCV)
            ├── RotateOperator.hpp/cpp     # 旋转算子 (OpenCV)
            └── PuzzleOperator.hpp/cpp     # 拼图算子 (OpenCV)
```

---

## 🛠️ Code Block 1: 门面层 - 对外公共接口与头文件

### 1.1 门面类接口
*文件名：[include/mimgs/interfaces/IImages.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/include/mimgs/interfaces/IImages.hpp)*
```cpp
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

        // 执行流水线任务
        virtual bool execute(const PipelineParams& params, const std::unordered_map<std::string, std::shared_ptr<ImageAsset>>& inputs, std::unordered_map<std::string, std::shared_ptr<ImageAsset>>& outputs) = 0;
        // 清除持久缓存
        virtual void clearCache() = 0;
        // 动态设置 LRU 缓存容量
        virtual void setCacheCapacity(size_t capacity) = 0;
    };
}

#endif // I_IMAGES_HPP
```

### 1.2 外部 SDK 门面声明与管道参数
*文件名：[include/mimgs/Mimgs.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/include/mimgs/Mimgs.hpp)*
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
#include "mimgs/interfaces/IImages.hpp"

#include <vector>
#include <string>
#include <memory>

namespace ei::mimgs
{
    // 流水线任务参数结构体
    struct MIMGS_API PipelineParams : public OperatorParams
    {
        std::vector<std::string> inputKey;                  // 输入的主图像标识列表
        std::vector<std::shared_ptr<OperatorParams>> steps; // 存放具体算子参数 (ScaleParams 等) 的智能指针

        void Validate() const override;
        std::string GetCacheSignature() const override;
    };

    // 门面实现类：外部客户端调用的主要入口
    class MIMGS_API Images : public IImages
    {
    public:
        Images() = default;
        ~Images() override;

        // 禁用拷贝语义，避免大体积的底层状态和内部引用被复制
        Images(const Images&) = delete;
        Images& operator=(const Images&) = delete;

        // 执行主调用
        bool execute(const PipelineParams &params, const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs, std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs) override;
        void clearCache() override;
        void setCacheCapacity(size_t capacity) override;

    private:
        class Impl;                     // 前置声明 Impl 类实现编译隔离
        std::unique_ptr<Impl> _impl;    // Impl 独占指针，实现 Pimpl 编译防火墙
    };
}

#endif // IMAGES_HPP
```

---

## 🔒 Code Block 2: 共享通用层与核心基类

### 2.1 核心公共数据结构与上下文契约
*文件名：[source/mimgs/common/Common.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/common/Common.hpp)*
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
    // 运行时自定义异常类
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

    // 内部图像基础资源：包装原始像素数据 buffer 
    struct ImageAsset
    {
        std::string id;
        uint32_t width{0};
        uint32_t height{0};
        PixelFormat format{PixelFormat::RGBA8888};
        std::unique_ptr<uint8_t[]> data{nullptr}; // 内存独占，通过 std::move 防止不必要的内存复制

        ImageAsset(std::string_view asset_id, uint32_t w, uint32_t h, PixelFormat fmt)
            : id(asset_id), width(w), height(h), format(fmt)
        {
            size_t size = static_cast<size_t>(w) * h * (fmt == PixelFormat::RGBA8888 ? 4 : 3);
            data = std::make_unique<uint8_t[]>(size);
        }

        // 禁用拷贝语义
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

        // 生成特征签名，用于 LRU 缓存命中匹配
        virtual std::string GetCacheSignature() const = 0;
    };

    // 内部共享黑板上下文：用于临时存储执行链路中各节点的中间产物，保证多线程局部隔离
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

    // 统一算子基类接口
    class IOperator
    {
    public:
        virtual ~IOperator() = default;
        virtual std::string_view getName() const noexcept = 0;
        virtual void execute(IContext &context, const OperatorParams &params) = 0;

    protected:
        // 参数多态向下转型转换工具方法
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

### 2.2 OpenCV 图像转换工具
*文件名：[source/mimgs/common/Utils.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/common/Utils.hpp)*
```cpp
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

*文件名：[source/mimgs/common/Utils.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/common/Utils.cpp)*
```cpp
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
            for(int r = 0; r < dstMat.rows; ++r)
                std::memcpy(dstAsset->data.get() + r * rowBytes, dstMat.ptr(r), rowBytes);

        return dstAsset;
    }
}
```

### 2.3 线程安全全局 LRU 图像缓存
*文件名：[source/mimgs/common/LRUImageCache.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/common/LRUImageCache.hpp)*
```cpp
#ifndef LRU_IMAGE_CACHE_HPP
#define LRU_IMAGE_CACHE_HPP

#include "common/Common.hpp"

#include <unordered_map>
#include <mutex>
#include <list>
#include <memory>

namespace ei::mimgs
{
    // 全局单例 LRU 图像缓存系统，通过哈希表结合双向链表实现 O(1) 检索
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

        void evict();

    private:
        size_t _capacity;
        std::mutex _mutex; // 独占锁，用于多线程下读写缓存的互斥操作
        std::list<std::pair<std::string, std::shared_ptr<ImageAsset>>> _list; // 维持最近访问的顺序，头部最近访问，尾部最久未访问
        std::unordered_map<std::string, decltype(_list.begin())> _map; // 快速 O(1) 寻址哈希表
    };
}

#endif // LRU_IMAGE_CACHE_HPP
```

*文件名：[source/mimgs/common/LRUImageCache.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/common/LRUImageCache.cpp)*
```cpp
#include "LRUImageCache.hpp"

namespace ei::mimgs
{
    LRUImageCache &LRUImageCache::getInstance()
    {
        static LRUImageCache instance(15); // 默认维持最多 15 张图片缓存
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

        // 将访问到的节点剪切并转移到双向链表的最前列（代表最近被使用）
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

    void LRUImageCache::evict()
    {
        while(_list.size() > _capacity)
        {
            auto last = _list.back();
            _map.erase(last.first);
            _list.pop_back();
        }
    }
}
```

---

## 🔀 Code Block 3: 调度与流水线管理层

### 3.1 算子多态注册表
*文件名：[source/mimgs/pipeline/OperatorRegistry.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/pipeline/OperatorRegistry.hpp)*
```cpp
#ifndef OPERATOR_REGISTRY_HPP
#define OPERATOR_REGISTRY_HPP

#include "common/Common.hpp"

#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <memory>

namespace ei::mimgs
{
    // 全局单例算子注册中心，采用读写锁设计支持高并发检索
    class OperatorRegistry
    {
    public:
        static OperatorRegistry &getInstance();

        void RegisterOperator(std::unique_ptr<IOperator> op);
        IOperator *getOperator(std::string_view name) const;

    private:
        OperatorRegistry() = default;
        ~OperatorRegistry() = default;

        OperatorRegistry(const OperatorRegistry &) = delete;
        OperatorRegistry &operator=(const OperatorRegistry &) = delete;

    private:
        mutable std::shared_mutex _mutex; // 读写锁，支持单写多读并发访问
        std::unordered_map<std::string, std::unique_ptr<IOperator>> _registry;
    };
}

#endif // OPERATOR_REGISTRY_HPP
```

*文件名：[source/mimgs/pipeline/OperatorRegistry.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/pipeline/OperatorRegistry.cpp)*
```cpp
#include "OperatorRegistry.hpp"
#include <mutex>

namespace ei::mimgs
{
    OperatorRegistry &OperatorRegistry::getInstance()
    {
        static OperatorRegistry instance;
        return instance;
    }

    void OperatorRegistry::RegisterOperator(std::unique_ptr<IOperator> op)
    {
        if (!op)
            return;

        std::unique_lock<std::shared_mutex> lock(_mutex); // 独占写锁
        _registry[std::string(op->getName())] = std::move(op);
    }

    IOperator *OperatorRegistry::getOperator(std::string_view name) const
    {
        std::shared_lock<std::shared_mutex> lock(_mutex); // 共享读锁，避免多线程检索竞争

        auto it = _registry.find(std::string(name));
        if(it != _registry.end())
            return it->second.get();

        return nullptr;
    }
}
```

### 3.2 流水线调度管理声明
*文件名：[source/mimgs/pipeline/Pipeline.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/pipeline/Pipeline.hpp)*
```cpp
#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "common/Common.hpp"
#include <memory>
#include <vector>

namespace ei::mimgs
{
    struct PipelineParams;

    struct PipelineStep
    {
        std::string opName;
        std::unique_ptr<OperatorParams> params;
    };

    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        Pipeline(const Pipeline &) = delete;
        Pipeline &operator=(const Pipeline &) = delete;

        // 根据配置参数数组，在指定上下文中按顺序执行图像算子
        void execute(IContext &context, const PipelineParams &params);
    };
}

#endif // PIPELINE_HPP
```

### 3.3 流水线调度具体实现【新补齐】
*文件名：[source/mimgs/pipeline/Pipeline.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/pipeline/Pipeline.cpp)*
```cpp
#include "Pipeline.hpp"
#include "mimgs/Mimgs.hpp"
#include "pipeline/OperatorRegistry.hpp"
#include "process/ScaleOperator.hpp"
#include "process/RotateOperator.hpp"
#include "process/PuzzleOperator.hpp"

namespace ei::mimgs
{
    void Pipeline::execute(IContext &context, const PipelineParams &params)
    {
        auto &registry = OperatorRegistry::getInstance();

        // 顺序遍历客户端配置的所有管道步骤
        for (const auto &stepParams : params.steps)
        {
            if (!stepParams)
            {
                throw ProcessingException("Pipeline::execute: Encountered null step parameters.");
            }

            // 利用 RTTI 的 dynamic_cast 在底层解耦参数与算子的静态依赖，解析对应的算子标识名称
            std::string opName;
            if (dynamic_cast<const ScaleParams*>(stepParams.get()))
            {
                opName = "scale";
            }
            else if (dynamic_cast<const RotateParams*>(stepParams.get()))
            {
                opName = "rotate";
            }
            else if (dynamic_cast<const PuzzleParams*>(stepParams.get()))
            {
                opName = "puzzle";
            }
            else
            {
                throw ProcessingException("Pipeline::execute: Unknown/unsupported operator parameters type.");
            }

            // 从全局单例注册表中并发查询对应的处理算子实体
            IOperator *op = registry.getOperator(opName);
            if (!op)
            {
                throw ProcessingException("Pipeline::execute: Operator '" + opName + "' is not registered.");
            }

            // 参数合理性防卫校验
            stepParams->Validate();

            // 多态调用底层 OpenCV 算子，进行像素级内存处理
            op->execute(context, *stepParams);
        }
    }
}
```

---

## 🎨 Code Block 4: 业务算子层 (像素处理与算法核心)

### 4.1 缩放算子 (`ScaleOperator`)
*文件名：[source/mimgs/process/ScaleOperator.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/process/ScaleOperator.hpp)*
```cpp
#ifndef SCALEOPERATOR_HPP
#define SCALEOPERATOR_HPP

#include "common/Common.hpp"
#include <string_view>

namespace ei::mimgs
{
    struct ScaleParams : public OperatorParams
    {
        std::string inputKey;
        size_t tarWidth{0};
        size_t tarHeight{0};

        void Validate() const override;
        std::string GetCacheSignature() const override;
    };

    class ScaleOperator : public IOperator
    {
    public:
        ScaleOperator() = default;

        std::string_view getName() const noexcept override;
        void execute(IContext &context, const OperatorParams &params) override;
    };
}

#endif // SCALEOPERATOR_HPP
```

*文件名：[source/mimgs/process/ScaleOperator.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/process/ScaleOperator.cpp)*
```cpp
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

        // 使用 OpenCV 内存覆盖技术，复用已分配 of ImageAsset buffer
        int cvType = MapToCVType(srcAsset->format);
        cv::Mat srcMat(srcAsset->height, srcAsset->width, cvType, srcAsset->data.get());

        cv::Mat dstMat;
        cv::Size tarSize(static_cast<int>(scaleParams.tarWidth), static_cast<int>(scaleParams.tarHeight));
        cv::resize(srcMat, dstMat, tarSize, 0, 0, cv::INTER_LINEAR); // OpenCV 缩放核心

        auto dstAsset = std::make_shared<ImageAsset>(scaleParams.outputKey, scaleParams.tarWidth, scaleParams.tarHeight, srcAsset->format);
        size_t rowBytes = static_cast<size_t>(scaleParams.tarWidth) * GetChannelCount(srcAsset->format);

        // 高效的数据拷贝（优化：考虑是否为连续内存进行快速单步 memcpy 或分行 memcpy）
        if (dstMat.isContinuous())
            std::memcpy(dstAsset->data.get(), dstMat.data, rowBytes * scaleParams.tarHeight);
        else
        {
            for(int r = 0; r < dstMat.rows; ++r)
            {
                std::memcpy(dstAsset->data.get() + r * rowBytes, dstMat.ptr(r), rowBytes);
            }
        }
 
        context.setAsset(std::move(dstAsset));
    }
}
```

### 4.2 旋转算子 (`RotateOperator`)
*文件名：[source/mimgs/process/RotateOperator.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/process/RotateOperator.hpp)*
```cpp
#ifndef ROTATE_OPERATOR_HPP
#define ROTATE_OPERATOR_HPP

#include "common/Common.hpp"

namespace ei::mimgs
{
    struct RotateParams : public OperatorParams
    {
        std::string inputKey;
        float angle{0.0f};

        void Validate() const override;
        std::string GetCacheSignature() const override;
    };

    class RotateOperator : public IOperator
    {
    public:
        RotateOperator() = default;

        std::string_view getName() const noexcept override;
        void execute(IContext &context, const OperatorParams &params) override;
    };
}

#endif // ROTATE_OPERATOR_HPP
```

*文件名：[source/mimgs/process/RotateOperator.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/process/RotateOperator.cpp)*
```cpp
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

        // 计算带边界保护的仿射旋转参数
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
            for(int r = 0; r < dstMat.rows; ++r)
            {
                std::memcpy(dstAsset->data.get() + r * rowBytes, dstMat.ptr(r), rowBytes);
            }
        }

        context.setAsset(std::move(dstAsset));
    }
}
```

### 4.3 拼图算子 (`PuzzleOperator`)
*文件名：[source/mimgs/process/PuzzleOperator.hpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/process/PuzzleOperator.hpp)*
```cpp
#ifndef PUZZLE_OPERATOR_HPP
#define PUZZLE_OPERATOR_HPP

#include "common/Common.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <array>

namespace ei::mimgs
{
    struct PuzzleParams : public OperatorParams
    {
        std::vector<std::string> inputKeys;             // 待拼接的输入图像 key 数组
        uint32_t rows{0};                               // 网格行数
        uint32_t cols{0};                               // 网格列数
        uint32_t cellWidth{0};                          // 单格单元的预缩放宽
        uint32_t cellHeight{0};                         // 单格单元的预缩放高
        uint32_t padding{0};                            // 网格间距
        std::array<uint8_t, 4> bgColor{0, 0, 0, 0};     // 间距填充背景颜色

        void Validate() const override;
        std::string GetCacheSignature() const override;
    };

    class PuzzleOperator : public IOperator
    {
    public:
        PuzzleOperator() = default;

        std::string_view getName() const noexcept override;
        void execute(IContext &context, const OperatorParams &params) override;
    };
}

#endif // PUZZLE_OPERATOR_HPP
```

*文件名：[source/mimgs/process/PuzzleOperator.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/process/PuzzleOperator.cpp)*
```cpp
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

    std::string PuzzleParams::GetCacheSignature() const
    {
        std::string raw;
        for (const auto &key : inputKeys)
            raw += key + ",";
        raw += std::to_string(rows) + "_" + std::to_string(cols) + "_" + std::to_string(cellWidth) + "_" + std::to_string(cellHeight) + "_" + std::to_string(padding) + "_" + std::to_string(bgColor[0]) + "_" + std::to_string(bgColor[1]) + "_" + std::to_string(bgColor[2]) + "_" + std::to_string(bgColor[3]);

        // 使用标准 std::hash 对参数细节进行特征签名生成与散列压缩
        size_t hashVal = std::hash<std::string>{}(raw);

        char buf[32];
        std::snprintf(buf, sizeof(buf), "pz_%zx", hashVal);
        return std::string(buf);
    }

    std::string_view PuzzleOperator::getName() const noexcept
    {
        return "puzzle";
    }

    void PuzzleOperator::execute(IContext &context, const OperatorParams &params)
    {
        const auto &puzzleParams = CastParams<PuzzleParams>(params);
        puzzleParams.Validate();

        // 1. 计算拼图后的全局整体分辨率大小
        uint32_t totalWidth = puzzleParams.cols * puzzleParams.cellWidth + (puzzleParams.cols > 1 ? (puzzleParams.cols - 1) * puzzleParams.padding : 0);
        uint32_t totalHeight = puzzleParams.rows * puzzleParams.cellHeight + (puzzleParams.rows > 1 ? (puzzleParams.rows - 1) * puzzleParams.padding : 0);

        PixelFormat outputFormat = PixelFormat::RGBA8888;
        int outputCVType = MapToCVType(outputFormat);
        int outputChannels = GetChannelCount(outputFormat);

        // 2. 初始化带底色的画布 Mat
        cv::Mat dstMat(static_cast<int>(totalHeight), static_cast<int>(totalWidth), outputCVType);
        cv::Scalar cvBgColor(puzzleParams.bgColor[2], puzzleParams.bgColor[1], puzzleParams.bgColor[0], puzzleParams.bgColor[3]);
        dstMat.setTo(cvBgColor);

        // 3. 循环将每张源图缩放、对齐、并写入 ROI 对应区间
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
                
                // 兼容多色差通道类型转换 (RGB -> RGBA)
                if (srcAsset->format == PixelFormat::RGB888 && outputFormat == PixelFormat::RGBA8888)
                {
                    cv::Mat tempConvertedMat;
                    cv::cvtColor(currentSrcMat, tempConvertedMat, cv::COLOR_RGB2RGBA);
                    currentSrcMat = tempConvertedMat;
                }

                cv::Mat resizedSrcMat;
                cv::resize(currentSrcMat, resizedSrcMat, cv::Size(static_cast<int>(puzzleParams.cellWidth), static_cast<int>(puzzleParams.cellHeight)), 0, 0, cv::INTER_LINEAR);

                // 根据网格行列索引以及间距 padding 决定绘制起始偏移量
                int xOffset = static_cast<int>(c * (puzzleParams.cellWidth + puzzleParams.padding));
                int yOffset = static_cast<int>(r * (puzzleParams.cellHeight + puzzleParams.padding));

                cv::Rect roi(xOffset, yOffset, resizedSrcMat.cols, resizedSrcMat.rows);
                resizedSrcMat.copyTo(dstMat(roi)); // 直接覆写画布 ROI 子矩阵区域
                currentInputIndex++;
            }

            if (currentInputIndex >= puzzleParams.inputKeys.size())
                break;
        }

        // 4. 将完成的画布序列化拷贝到输出 ImageAsset 中
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
```

---

## 🏗️ Code Block 5: 门面实现层与桥接组装 (编译隔离实体)

### 5.1 门面实现类【新补齐】
*文件名：[source/mimgs/Mimgs.cpp](file:///C:/Users/Administrator/source/repos/Ubuntu-EditImgs/source/mimgs/Mimgs.cpp)*
```cpp
#include "mimgs/Mimgs.hpp"
#include "mimgs/common/Common.hpp"
#include "mimgs/common/LRUImageCache.hpp"
#include "mimgs/pipeline/Pipeline.hpp"
#include "mimgs/pipeline/OperatorRegistry.hpp"
#include "mimgs/process/ScaleOperator.hpp"
#include "mimgs/process/RotateOperator.hpp"
#include "mimgs/process/PuzzleOperator.hpp"
#include "mlog/Log.hpp"

#include <stdexcept>
#include <iostream>

namespace ei::mimgs
{
    // --- PipelineParams Implementation ---
    void PipelineParams::Validate() const
    {
        OperatorParams::Validate();
        if (inputKey.empty())
        {
            throw ProcessingException("PipelineParams: inputKey is empty.");
        }
        for (const auto &step : steps)
        {
            if (!step)
            {
                throw ProcessingException("PipelineParams: steps contains null pointer.");
            }
            step->Validate();
        }
    }

    std::string PipelineParams::GetCacheSignature() const
    {
        // 拼接生成全局唯一的管道配置签名，用于命中检查
        std::string sig = "pipeline_inputs:";
        for (const auto &key : inputKey)
        {
            sig += key + ",";
        }
        sig += "_steps:";
        for (const auto &step : steps)
        {
            if (step)
            {
                sig += "[" + step->GetCacheSignature() + "]";
            }
        }
        return sig;
    }

    // --- Images::Impl Definition (实体层：被 pimpl_ 隐藏) ---
    class Images::Impl
    {
    public:
        Impl()
        {
            // 在构造阶段将系统内置的三个算子注册到单例注册表中
            auto &registry = OperatorRegistry::getInstance();
            registry.RegisterOperator(std::make_unique<ScaleOperator>());
            registry.RegisterOperator(std::make_unique<RotateOperator>());
            registry.RegisterOperator(std::make_unique<PuzzleOperator>());
        }

        ~Impl() = default;

        bool execute(const PipelineParams &params,
                     const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs,
                     std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs)
        {
            try
            {
                // 1. 合理性检验
                params.Validate();

                // 2. 检查 LRU 全局缓存，如命中则零拷贝直接返回缓存结果
                std::string cacheKey = params.GetCacheSignature();
                auto &cache = LRUImageCache::getInstance();
                auto cachedAsset = cache.getCapacity(cacheKey);
                if (cachedAsset)
                {
                    outputs[params.outputKey] = cachedAsset;
                    ilog("Images::execute: Cache hit for key: {}", cacheKey);
                    return true;
                }

                // 3. 构建线程栈内局部的隔离 context (黑板容器)
                class ContextImpl : public IContext {};
                ContextImpl context;

                // 载入所有的初级输入资产到黑板容器
                for (const auto &[key, asset] : inputs)
                {
                    context.setAsset(asset);
                }

                // 4. 执行管道任务
                Pipeline pipeline;
                pipeline.execute(context, params);

                // 5. 抓取终极生成图像资产
                auto finalAsset = context.getAsset(params.outputKey);
                if (!finalAsset)
                {
                    throw ProcessingException("Images::execute: Pipeline finished but output key '" + params.outputKey + "' was not found in context.");
                }

                // 6. 将输出结果压入全局缓存，以供下轮提速检索
                cache.putCapacity(cacheKey, finalAsset);

                // 7. 返回结果给客户端
                outputs[params.outputKey] = finalAsset;
                return true;
            }
            catch (const std::exception &ex)
            {
                elog("Images::execute error: {}", ex.what());
                return false;
            }
        }
    };

    // --- Images Facade (门面包装函数) ---
    Images::~Images() = default;

    bool Images::execute(const PipelineParams &params,
                         const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs,
                         std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs)
    {
        // 延迟懒初始化 Impl 实体，保证极致加载效率与健壮性
        if (!_impl)
        {
            _impl = std::make_unique<Impl>();
        }
        return _impl->execute(params, inputs, outputs);
    }

    void Images::clearCache()
    {
        LRUImageCache::getInstance().clearCapacity();
    }

    void Images::setCacheCapacity(size_t capacity)
    {
        LRUImageCache::getInstance().setCapacity(capacity);
    }
}
```

---

## 🧭 模块功能完整路径解析 (Execution Path Analysis)

当外部客户端调用本项目提供的 `mimgs` 图像处理服务时，其整体函数的指令执行与数据流转路径如下：

1. **客户端发起请求与算子预注册**
   - 外部客户端准备好待处理的图片哈希映射表 `inputs` 与调度步骤参数 `PipelineParams`，并调用 `Images::execute`。
   - 如果这是程序运行中首次调用该接口，门面实现类的构造函数会通过 `OperatorRegistry::getInstance().RegisterOperator` 方法，自动向全局单例算子注册中心注册 `ScaleOperator`、`RotateOperator` 与 `PuzzleOperator` 实例。

2. **参数防御性验证与缓存特征签名生成**
   - 首先调用 `PipelineParams::Validate()`，确保主管道参数与子算子配置都合法（如行/列大于0，宽度高度大于0，且无悬空指针）。
   - 接着，调用 `PipelineParams::GetCacheSignature()` 生成特征字符串（例如：`pipeline_inputs:img_A,_steps:[scale_img_A_800x600][rotate_scale_img_A_90.0]`）。

3. **LRU 缓存前置拦截**
   - 使用刚才生成的特征字符串，对全局 `LRUImageCache` 实施匹配 `getCapacity(cacheKey)`：
     * **命中拦截**：如果全局持久缓存中曾经执行并存储过此流转配置下的图片，直接将其智能指针引用返回给客户端，不再重复计算。缓存检索操作为 $O(1)$。
     * **未命中**：如果未命中，则分配临时栈上下文进入下一步流转。

4. **栈内隔离上下文 black-board 组装**
   - 临时构造局部变量 `ContextImpl context`。
   - 通过 `context.setAsset` 将客户端传入的原始图像资产加载进入上下文。由于每个线程的 `execute` 都在各自的栈中实例化自己独立的 `ContextImpl`，因此在多线程并发执行时，各个任务之间的图像暂存容器完全物理隔离，**天然支持无锁高并发（No-Lock Multithreading）**。

5. **流水线步骤多态调度执行**
   - 由 `Pipeline` 顺序遍历 `PipelineParams::steps` 中的各个步骤：
     1. **类型识别识别**：通过 RTTI 的 `dynamic_cast` 检测参数智能指针的真实动态类型（是否为 `ScaleParams` / `RotateParams` / `PuzzleParams`），并翻译为对应的字符标识（如 `"scale"`）。
     2. **算子查询**：使用共享读锁在全局 `OperatorRegistry` 检索匹配该类型的算子类指针 `IOperator*`。
     3. **图像像素计算**：多态调用底层的 `IOperator::execute(context, *stepParams)`。

6. **底层算子与 OpenCV 处理**
   - 具体算子被调用后：
     1. 从 `context` 容器中以只读指针提取所需输入的图片。
     2. 利用 OpenCV 矩阵指针覆写技术，在不产生数据深拷贝的情况下，通过 OpenCV 高性能组件执行缩放 (`cv::resize`)、旋转 (`cv::warpAffine`) 或拼接 (`cv::Rect` ROI 画布覆写)。
     3. 为生成的新图像申请一块一维连续数据缓冲区，封装为 `ImageAsset`，并以 `std::move` 回填到 `context` 中以备下阶段步骤链条读取。

7. **导出最终成果并补充缓存**
   - 全部流水线节点流转执行完毕后，主控 `Impl` 从 `context` 中提取 `finalOutputKey` 关联的最终图片资产，将其存入 `LRUImageCache` 模块中以备下次拦截使用。
   - 如果缓存超出所限定容量，将触发 LRU 机制淘汰最近最少使用的数据包。最终将成果赋值给客户端传入的 `outputs` map 容器。

---

## 💡 架构亮点旁解解析 (Architectural Insight & Commentary)

本项目在 C++ 架构设计、编译、性能以及内存控制方面拥有诸多高水平的工业标准设计，以下针对核心模式进行深度剖析：

### 1. Pimpl 编译器防火墙模式 (Pimpl Idiom)
在分发本动态链接库（`.so` 或 `.dll`）供外部使用时，只向客户暴露 `IImages.hpp` 与 `Mimgs.hpp`。
* **物理隔离**：外部客户端的 `main.cpp` 在包含 `Mimgs.hpp` 时，完全不需要知道 OpenCV 库、调度系统、甚至 LRU 缓存等底层的头文件和实现细节。
* **ABI 稳定性**：由于客户端直接访问的只有门面类 `Images`，且它唯一的成员变量是前置声明的指针 `std::unique_ptr<Impl> _impl`（32位下大小为4字节，64位下为8字节）。即使后期 `Impl` 内部私有成员发生大规模重构、增加或删除变量，**门面类 `Images` 的物理大小和二进制布局（Vtable & Size）也绝不会发生任何改变**，外部客户端完全无需重新编译即可平滑替换库升级，这达成了极强的 ABI 稳定性。
* **编译提速**：极大地减少了多文件之间的头文件依赖与重复展开，从而显著降低了客户端项目的构建编译周期。

### 2. 黑板模式 (Blackboard Design Pattern)
系统的共享执行上下文 `IContext` 运用了经典的**黑板模式**。
* **算子无状态化**：每个算子（如 `ScaleOperator`）都是没有类成员变量的无状态实例。它们只负责从“黑板”（`IContext`）上根据 input key 读下图片，完成计算后写回 output key 到“黑板”上。
* **灵活拼装**：流水线调度器可以任意通过配置文件或参数，重新编排、颠倒、增加或删除执行的算子，而无需改变任何底层的 `execute` 接口，具备了绝佳的可伸缩性。

### 3. $O(1)$ 线程安全最近最少使用缓存设计 (Thread-Safe LRU Image Cache)
高维图像像素数据如果重复读取与转换，对 CPU 算力和内存带宽将产生极大的浪费。
* **存储与检索优化**：通过 `std::unordered_map` 加双向列表 `std::list` 组成组合容器。借助哈希表对链表 Iterator 进行缓存定位，实现了在 $O(1)$ 时间复杂度下完成数据定位；同时通过将使用到的链表节点使用高效的 `std::list::splice` 移动到链表头部来实现 $O(1)$ 的顺序调整，淘汰尾部的最旧资产。
* **临界区控制与读写互斥**：在 LRU 单例检索中，哪怕是“读”操作（即调用 `getCapacity`）实质上也会在内部对 `std::list` 的节点位置进行修改。因此这里使用 `std::lock_guard<std::mutex>` 进行独占锁锁定，保障了多线程下缓存更新和淘汰机制的数据一致性。

### 4. 读写分离锁与算子动态检索机制 (Shared Mutex / Operator Registry)
在多线程并发流水线中，如果频繁从全局注册表获取算子实例，将会产生激烈的锁竞争。
* **写少读多优化**：`OperatorRegistry` 采用 `std::shared_mutex`（C++17 读写锁机制）。系统仅在 `Images` 的构造/懒初始化阶段获取独占写锁（`std::unique_lock`）进行算子的配置写入；而在程序运行和频繁的管道调度执行阶段，各个并发工作线程只需获取读锁（`std::shared_lock`）即可安全并发读取算子实例，消除了多线程只读状态下的并发卡顿瓶颈。