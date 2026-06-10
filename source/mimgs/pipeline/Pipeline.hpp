#ifndef PIPELINE_HPP
#define PIPELINE_HPP
#include "common/Common.hpp"
#include <memory>
#include <vector>

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

        void addStep(std::string_view opName, std::unique_ptr<OperatorParams> params);
        const std::vector<PipelineStep> &getSteps() const noexcept;
    };
}

#endif // PIPELINE_HPP