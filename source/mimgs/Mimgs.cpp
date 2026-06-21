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
#include <iostream>

namespace ei::mimgs
{
    void PipelineParams::Validate() const
    {
        OperatorParams::Validate();
        if (inputKey.empty())
            throw ProcessingException("PipelineParams: inputKey is empty.");

        for (const auto &step : steps)
        {
            if (!step)
                throw ProcessingException("PipelineParams: steps contains null pointer.");

            step->Validate();
        }
    }

    std::string PipelineParams::GetCacheSignature() const
    {
        std::string sig = "pipeline_inputs:";
        for (const auto &key : inputKey)
        {
            sig += key + ",";
        }

        sig += "_steps";
        for (const auto &step : steps)
        {
            if (step)
            {
                sig += "[" + step->GetCacheSignature() + "]";
            }
        }

        return sig;
    }

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

        bool execute(const PipelineParams &params, const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs, std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs)
        {
            try
            {
                params.Validate();

                std::string cacheKey = params.GetCacheSignature();
                auto &cache = LRUImageCache::getInstance();
                auto cacheAsset = cache.getCapacity(cacheKey);
                if (cacheAsset)
                {
                    outputs[params.outputKey] = cacheAsset;
                    ilog("Images::execute: Cache hit for key: {}", cacheKey);
                    return true;
                }

                class ContextImpl : public IContext
                {
                };
                ContextImpl context;

                for (const auto &[key, asset] : inputs)
                {
                    context.setAsset(asset);
                }

                Pipeline pipeline;
                pipeline.execute(context, params);

                auto finalAsset = context.getAsset(params.outputKey);
                if (!finalAsset)
                {
                    throw ProcessingException("Images::execute: Pipeline finished but output key '" + params.outputKey + "' was not found in context.");
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

    Images::~Images() = default;

    bool Images::execute(const PipelineParams &params,
                         const std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &inputs,
                         std::unordered_map<std::string, std::shared_ptr<ImageAsset>> &outputs)
    {
        if (!_impl)
            _impl = std::make_unique<Impl>();

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
