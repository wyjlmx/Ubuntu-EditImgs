#include "mimgs/Mimgs.hpp"
#include "common/Common.hpp"
#include "common/LRUImageCache.hpp"
#include "pipeline/Pipeline.hpp"
#include "pipeline/OperatorRegistry.hpp"
#include "process/ScaleOperator.hpp"
#include "process/RotateOperator.hpp"
#include "process/PuzzleOperator.hpp"
#include "mlog/Log.hpp"

#include <stdexcept>
#include <algorithm>

namespace ei::mimgs
{
    namespace
    {

        // --- 内部辅助函数：专门负责公开 DTO 参数的校验与特征哈希签名生成 ---
        static void ValidatePublicParams(const PipelineParams &params)
        {
            if (params.outputKey.empty())
                throw ProcessingException("PipelineParams: outputKey is empty.");
            if (params.inputKey.empty())
                throw ProcessingException("PipelineParams: inputKey is empty.");

            for (const auto &step : params.steps)
            {
                if (step.outputKey.empty())
                    throw ProcessingException("PipelineStep: outputKey is empty.");

                std::visit([](auto &&arg)
                           {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ScaleStep>) {
                    if (arg.inputKey.empty()) throw ProcessingException("ScaleStep: inputKey is empty.");
                    if (arg.tarWidth == 0 || arg.tarHeight == 0) throw ProcessingException("ScaleStep: invalid size.");
                } 
                else if constexpr (std::is_same_v<T, RotateStep>) {
                    if (arg.inputKey.empty()) throw ProcessingException("RotateStep: inputKey is empty.");
                } 
                else if constexpr (std::is_same_v<T, PuzzleStep>) {
                    if (arg.inputKeys.empty()) throw ProcessingException("PuzzleStep: inputKeys is empty.");
                    if (arg.rows == 0 || arg.cols == 0 || arg.cellWidth == 0 || arg.cellHeight == 0) {
                        throw ProcessingException("PuzzleStep: invalid grid or cell dimensions.");
                    }
                } }, step.params);
            }
        }

        static std::string GenerateCacheSignature(const PipelineParams &params)
        {
            std::string sig = "pub_pipeline_inputs:";
            for (const auto &key : params.inputKey)
            {
                sig += key + ",";
            }
            sig += "_steps:";

            for (const auto &step : params.steps)
            {
                sig += "[out:" + step.outputKey + "_";
                std::visit([&sig](auto &&arg)
                           {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ScaleStep>) {
                    sig += "scale_" + arg.inputKey + "_" + std::to_string(arg.tarWidth) + "x" + std::to_string(arg.tarHeight);
                } 
                else if constexpr (std::is_same_v<T, RotateStep>) {
                    sig += "rotate_" + arg.inputKey + "_" + std::to_string(arg.angle);
                } 
                else if constexpr (std::is_same_v<T, PuzzleStep>) {
                    std::string raw;
                    for (const auto &k : arg.inputKeys) raw += k + ",";
                    raw += std::to_string(arg.rows) + "_" + std::to_string(arg.cols) + "_" + 
                           std::to_string(arg.cellWidth) + "_" + std::to_string(arg.cellHeight) + "_" + 
                           std::to_string(arg.padding);
                    size_t hashVal = std::hash<std::string>{}(raw);
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "pz_%zx", hashVal);
                    sig += buf;
                } }, step.params);
                sig += "]";
            }
            return sig;
        }
    }

    // --- Images::Impl 实体定义 (完美隐藏在防火墙内部) ---
    class Images::Impl
    {
    public:
        Impl()
        {
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
                // 1. 公开 DTO 合理性防卫检验
                ValidatePublicParams(params);

                // 2. 高效的全局 LRU 缓存前置拦截
                std::string cacheKey = GenerateCacheSignature(params);
                auto &cache = LRUImageCache::getInstance();
                auto cachedAsset = cache.getCapacity(cacheKey);
                if (cachedAsset)
                {
                    outputs[params.outputKey] = cachedAsset;
                    ilog("Images::execute: Cache hit for key: {}", cacheKey);
                    return true;
                }

                // 3. 构造线程栈局部的隔离黑板容器
                class ContextImpl : public IContext
                {
                };
                ContextImpl context;

                for (const auto &[key, asset] : inputs)
                {
                    context.setAsset(asset);
                }

                // 4. 核心桥接步骤：将标准化的外部参数单向映射组装为底层的多态参数链条
                std::vector<std::shared_ptr<OperatorParams>> internalSteps;
                for (const auto &step : params.steps)
                {
                    std::visit([&internalSteps, &step](auto &&arg)
                               {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, ScaleStep>) {
                            auto p = std::make_shared<ScaleParams>();
                            p->inputKey = arg.inputKey;
                            p->outputKey = step.outputKey;
                            p->tarWidth = arg.tarWidth;
                            p->tarHeight = arg.tarHeight;
                            internalSteps.push_back(p);
                        } 
                        else if constexpr (std::is_same_v<T, RotateStep>) {
                            auto p = std::make_shared<RotateParams>();
                            p->inputKey = arg.inputKey;
                            p->outputKey = step.outputKey;
                            p->angle = arg.angle;
                            internalSteps.push_back(p);
                        } 
                        else if constexpr (std::is_same_v<T, PuzzleStep>) {
                            auto p = std::make_shared<PuzzleParams>();
                            p->inputKeys = arg.inputKeys;
                            p->outputKey = step.outputKey;
                            p->rows = arg.rows;
                            p->cols = arg.cols;
                            p->cellWidth = arg.cellWidth;
                            p->cellHeight = arg.cellHeight;
                            p->padding = arg.padding;
                            if (arg.bgColor.size() >= 4) {
                                std::copy_n(arg.bgColor.begin(), 4, p->bgColor.begin());
                            }
                            internalSteps.push_back(p);
                        } }, step.params);
                }

                // 5. 调用流水线处理核心
                Pipeline pipeline;
                pipeline.execute(context, internalSteps);

                // 6. 提取成果并更新缓存
                auto finalAsset = context.getAsset(params.outputKey);
                if (!finalAsset)
                {
                    throw ProcessingException("Images::execute: Pipeline output key '" + params.outputKey + "' missing.");
                }

                cache.putCapacity(cacheKey, finalAsset);
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

    // --- Images 门面外壳函数实现 ---
    Images::~Images() = default;

    bool Images::execute(const PipelineParams &params,
                         const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs,
                         std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs)
    {
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