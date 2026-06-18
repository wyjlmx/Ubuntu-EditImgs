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
                auto cacheAsset = cache.getCapacity();
                if(cacheAsset)
                {
                    outputs[params.outputKey] = cacheAsset;
                    ilog("Images::execute: Cache hit for key: {}", cacheKey);
                    return true;
                }

                class ContextImpl : public IContext {};
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            
        }
    };
}
