这是一套非常经典且高效的现代图像处理 SDK 架构选择。您选择的方案（**调度注册表 + 任务并行 + 算子内部并行 + 中间结果缓存 + LRU 淘汰机制**）在确保高性能的同时，提供了极高的并发安全性和内存控制能力。

> **注：** 您提到的 “LCR” 淘汰机制在计算机科学中通常指 **LRU（Least Recently Used，最近最少使用）** 淘汰机制，以下代码及讲解均以标准的 **LRU 淘汰机制** 进行实现。

下面为您提供完整的、生产环境级别的 C++17 核心层代码实现。为了方便您直接移植到 `.so` SDK 项目中，代码已经按照逻辑分层进行了彻底解耦。

---

### 一、 核心基础定义层 (`Common.hpp`)

这一层定义了图像资源、算子接口、参数基类。我们在参数基类中引入了 `GetCacheSignature()` 纯虚函数，这是实现中间缓存的基础。

```cpp
#ifndef COMMON_HPP
#define COMMON_HPP

#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <stdexcept>

namespace ei::mimgs
{
    class ProcessingException : public std::runtime_error
    {
    public:
        explicit ProcessingException(const std::string &msg)
            : std::runtime_error("[MIMGS Error] " + msg) {}
    };

    enum class PixelFormat
    {
        RGBA8888,
        RGB888
    };

    // 图像基础资源（支持移动语义，禁止拷贝）
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

    // 算子参数基类
    struct OperatorParams
    {
        std::string outputKey;
        virtual ~OperatorParams() = default;
        
        virtual void Validate() const
        {
            if (outputKey.empty())
                throw ProcessingException("Validate: outputKey is empty.");
        }

        // 企业级缓存核心：每个参数类必须生成其独一无二的特征签名（用于做 LRU Cache 的 Key）
        virtual std::string GetCacheSignature() const = 0;
    };

    // 局部、瞬态的执行上下文（线程安全隔离）
    class IContext
    {
    private:
        std::unordered_map<std::string, std::shared_ptr<ImageAsset>> _assets;

    public:
        void setAsset(std::shared_ptr<ImageAsset> asset)
        {
            if (!asset || asset->id.empty())
                throw ProcessingException("Null or invalid asset.");
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

    // 算子接口
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

---

### 二、 缓存层：线程安全的全局 LRU 缓存 (`LRUImageCache.hpp`)

图像缓存生命周期跨越多次管道执行，必须是**全局单例且线程安全**。因为缓存获取（`Get`）会改变双向链表节点的顺序（即“读”操作实质上修改了数据结构），所以我们使用互斥锁（`std::mutex`）来保证并发安全。

```cpp
#ifndef LRU_IMAGE_CACHE_HPP
#define LRU_IMAGE_CACHE_HPP

#include "Common.hpp"
#include <mutex>
#include <list>
#include <unordered_map>
#include <string>
#include <memory>

namespace ei::mimgs
{
    class LRUImageCache
    {
    public:
        static LRUImageCache &GetInstance()
        {
            static LRUImageCache instance(15); // 默认限制缓存 15 张大图，可根据实际物理内存调整
            return instance;
        }

        // 设置最大缓存容量
        void SetCapacity(size_t capacity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _capacity = capacity;
            Evict();
        }

        // 获取缓存（线程安全，O(1)）
        std::shared_ptr<ImageAsset> Get(const std::string &key)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _map.find(key);
            if (it == _map.end())
            {
                return nullptr; // 缓存未命中
            }
            // 移动到链表头部，代表最近被使用
            _list.splice(_list.begin(), _list, it->second);
            return it->second->second;
        }

        // 写入缓存（线程安全，O(1)）
        void Put(const std::string &key, std::shared_ptr<ImageAsset> asset)
        {
            if (!asset) return;

            std::lock_guard<std::mutex> lock(_mutex);
            auto it = _map.find(key);
            if (it != _map.end())
            {
                // 已存在，更新并移动到头部
                it->second->second = asset;
                _list.splice(_list.begin(), _list, it->second);
                return;
            }

            // 写入新节点
            _list.push_front({key, asset});
            _map[key] = _list.begin();

            // 超容淘汰
            Evict();
        }

        // 清空缓存
        void Clear()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _map.clear();
            _list.clear();
        }

    private:
        LRUImageCache(size_t capacity) : _capacity(capacity) {}
        ~LRUImageCache() = default;
        LRUImageCache(const LRUImageCache &) = delete;
        LRUImageCache &operator=(const LRUImageCache &) = delete;

        void Evict()
        {
            while (_list.size() > _capacity)
            {
                auto last = _list.back();
                _map.erase(last.first);
                _list.pop_back();
            }
        }

        size_t _capacity;
        std::mutex _mutex;
        std::list<std::pair<std::string, std::shared_ptr<ImageAsset>>> _list;
        std::unordered_map<std::string, decltype(_list.begin())> _map;
    };
}

#endif // LRU_IMAGE_CACHE_HPP
```

---

### 三、 调度管理层：算子注册表 (`OperatorRegistry.hpp`)

负责管理算子的生命周期，采用读写锁（`std::shared_mutex`），在初始化阶段写入，执行阶段以读锁方式多线程高并发查询，避免锁竞争。

```cpp
#ifndef OPERATOR_REGISTRY_HPP
#define OPERATOR_REGISTRY_HPP

#include "Common.hpp"
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <memory>

namespace ei::mimgs
{
    class OperatorRegistry
    {
    public:
        static OperatorRegistry &GetInstance()
        {
            static OperatorRegistry instance;
            return instance;
        }

        // 注册算子（线程安全，写锁）
        void RegisterOperator(std::unique_ptr<IOperator> op)
        {
            if (!op) return;
            std::unique_lock<std::shared_mutex> lock(_mutex);
            _registry[std::string(op->getName())] = std::move(op);
        }

        // 查找算子（线程安全，并发读锁）
        IOperator *GetOperator(std::string_view name) const
        {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            auto it = _registry.find(std::string(name));
            if (it != _registry.end())
            {
                return it->second.get();
            }
            return nullptr;
        }

    private:
        OperatorRegistry() = default;
        ~OperatorRegistry() = default;
        OperatorRegistry(const OperatorRegistry &) = delete;
        OperatorRegistry &operator=(const OperatorRegistry &) = delete;

        mutable std::shared_mutex _mutex;
        std::unordered_map<std::string, std::unique_ptr<IOperator>> _registry;
    };
}

#endif // OPERATOR_REGISTRY_HPP
```

---

### 四、 门面/封装层：管道控制 (`Pipeline.hpp` & `Pipeline.cpp`)

这一层对外部提供简单整洁的接口。在 `Execute` 函数中：
1. **任务并行**：利用调用者线程执行。
2. **中间缓存拦截**：执行具体算子前，利用算子特有签名检索 `LRUImageCache`。若命中则跳过耗时的算法逻辑。

#### 1. 头文件 (`Pipeline.hpp`)
```cpp
#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "Common.hpp"
#include <vector>
#include <memory>
#include <string>

namespace ei::mimgs
{
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
        Pipeline(Pipeline &&) noexcept = default;
        Pipeline &operator=(Pipeline &&) noexcept = default;

        // 通用步骤添加方法
        Pipeline &AddStep(std::string_view opName, std::unique_ptr<OperatorParams> params);

        // 针对已知内置算法的流式封装方法（Facade 特性）
        Pipeline &Scale(const std::string &inputKey, const std::string &outputKey, uint32_t width, uint32_t height);
        Pipeline &Rotate(const std::string &inputKey, const std::string &outputKey, float angle);

        // 执行调用（线程安全，可由多个线程并发调用不同的 Pipeline 实例）
        std::shared_ptr<ImageAsset> Execute(std::shared_ptr<ImageAsset> initialAsset, const std::string &finalOutputKey);

    private:
        std::vector<PipelineStep> _steps;
    };
}

#endif // PIPELINE_HPP
```

#### 2. 实现文件 (`Pipeline.cpp`)
为了避免强耦合具体的算子实现，`Pipeline.cpp` 只与 `ScaleParams` 和 `RotateParams` 的结构体定义产生依赖。

```cpp
#include "Pipeline.hpp"
#include "OperatorRegistry.hpp"
#include "LRUImageCache.hpp"

namespace ei::mimgs
{
    // 内置 Scale 参数定义
    struct ScaleParams : public OperatorParams
    {
        std::string inputKey;
        uint32_t tarWidth{0};
        uint32_t tarHeight{0};

        void Validate() const override
        {
            OperatorParams::Validate();
            if (inputKey.empty()) throw ProcessingException("ScaleParams: inputKey is empty.");
            if (tarWidth <= 0 || tarHeight <= 0) throw ProcessingException("ScaleParams: invalid size.");
        }

        std::string GetCacheSignature() const override
        {
            // 唯一签名由: 算子名 + 输入 + 参数 决定
            return "scale_" + inputKey + "_" + std::to_string(tarWidth) + "x" + std::to_string(tarHeight);
        }
    };

    // 内置 Rotate 参数定义
    struct RotateParams : public OperatorParams
    {
        std::string inputKey;
        float angle{0.0f};

        void Validate() const override
        {
            OperatorParams::Validate();
            if (inputKey.empty()) throw ProcessingException("RotateParams: inputKey is empty.");
        }

        std::string GetCacheSignature() const override
        {
            return "rotate_" + inputKey + "_" + std::to_string(angle);
        }
    };

    Pipeline &Pipeline::AddStep(std::string_view opName, std::unique_ptr<OperatorParams> params)
    {
        if (opName.empty() || !params)
            throw ProcessingException("Pipeline: Invalid step data.");
        _steps.push_back({std::string(opName), std::move(params)});
        return *this;
    }

    Pipeline &Pipeline::Scale(const std::string &inputKey, const std::string &outputKey, uint32_t width, uint32_t height)
    {
        auto params = std::make_unique<ScaleParams>();
        params->inputKey = inputKey;
        params->outputKey = outputKey;
        params->tarWidth = width;
        params->tarHeight = height;
        return AddStep("scale", std::move(params));
    }

    Pipeline &Pipeline::Rotate(const std::string &inputKey, const std::string &outputKey, float angle)
    {
        auto params = std::make_unique<RotateParams>();
        params->inputKey = inputKey;
        params->outputKey = outputKey;
        params->angle = angle;
        return AddStep("rotate", std::move(params));
    }

    std::shared_ptr<ImageAsset> Pipeline::Execute(std::shared_ptr<ImageAsset> initialAsset, const std::string &finalOutputKey)
    {
        if (!initialAsset)
            throw ProcessingException("Pipeline: Initial asset is null.");

        // 1. 每一个线程调用 Execute 时，都会创建自己独立的局部 context（瞬态缓冲）
        IContext context;
        context.setAsset(initialAsset);

        auto &cache = LRUImageCache::GetInstance();

        for (size_t i = 0; i < _steps.size(); ++i)
        {
            const auto &step = _steps[i];

            // 2. 生成这一步的唯一缓存 Key
            std::string cacheKey = step.params->GetCacheSignature();

            // 3. 拦截：尝试从全局持久缓存中获取
            auto cachedAsset = cache.Get(cacheKey);
            if (cachedAsset)
            {
                // 【修正：直接将指向缓存的智能指针放入当前 context】
                // 这里仅仅是 std::shared_ptr 的引用计数 +1，没有任何大内存拷贝，极其高效且绝对安全！
                context.setAsset(cachedAsset);
                continue; // 跳过当前算子的实际计算
            }

            // 4. 缓存未命中：从注册表获取算子并执行实际计算
            auto *op = OperatorRegistry::GetInstance().GetOperator(step.opName);
            if (!op)
            {
                throw ProcessingException("Pipeline failed: Operator '" + step.opName + "' not registered.");
            }

            try
            {
                step.params->Validate();
                op->execute(context, *(step.params)); // 实际进行像素级计算
            }
            catch (const std::exception &ex)
            {
                throw ProcessingException("Pipeline failed at step [" + std::to_string(i) + "] '" + step.opName + "': " + ex.what());
            }

            // 5. 计算完成后，将产出的结果存入全局持久缓存
            auto outputAsset = context.getAsset(step.params->outputKey);
            if (outputAsset)
            {
                cache.Put(cacheKey, outputAsset);
            }
        }

        // 6. 导出最终成果
        auto finalAsset = context.getAsset(finalOutputKey);
        if (!finalAsset)
            throw ProcessingException("Pipeline success, but final key '" + finalOutputKey + "' missing.");

        return finalAsset;
    }
}
```

---

### 五、 JSON 配置文件读取功能：未来应该放在哪一层？

在系统架构中，**JSON 读取与解析绝对不能写入底层的算法层或算子层**。为了保持高内聚低耦合，未来的 JSON 解析应当作为**独立于底层 SDK 核心的“高级组装层”或者“应用配置层”**。

#### 建议集成位置
推荐将 JSON 解析功能放在以下两处之一：
1. **Pipeline 内部的一个静态工厂方法（推荐）**：例如：`Pipeline Pipeline::CreateFromJson(const std::string& jsonContent)`。
2. **一个独立的、无状态的 `PipelineBuilder` 类**。

#### 为什么？
`Pipeline` 底层已经暴露了极为通用的 `AddStep(opName, params)` 接口。未来新增 JSON 功能时，解析器只需读取 JSON 文件，将键值对映射为 C++ 结构体参数，然后循环调用 `AddStep`，即可组装好管道并启动。

#### 未来集成的伪代码设计：
```cpp
// 放在未来新增的 PipelineParser.hpp 中，属于高层应用组装层
namespace ei::mimgs
{
    class PipelineParser
    {
    public:
        static Pipeline Build(const std::string& jsonConfig)
        {
            Pipeline pipeline;
            auto parsedSteps = CustomJsonLib::Parse(jsonConfig); // 使用第三方 JSON 库
            
            for (const auto& step : parsedSteps)
            {
                if (step.type == "scale")
                {
                    pipeline.Scale(step.input, step.output, step.w, step.h);
                }
                else if (step.type == "rotate")
                {
                    pipeline.Rotate(step.input, step.output, step.angle);
                }
                // 后续新增算子直接在此处添加分支，无需改动 Pipeline 类的执行底层
            }
            return pipeline;
        }
    };
}
```

这样设计，不仅达成了极佳的工程隔离，也使您未来的扩展可以极其顺畅。