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

        void execute(IContext &context, const PipelineParams &params);
    };
}

#endif // PIPELINE_HPP