#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "common/Common.hpp"
#include <memory>
#include <vector>

namespace ei::mimgs
{
    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        Pipeline(const Pipeline &) = delete;
        Pipeline &operator=(const Pipeline &) = delete;

        void execute(IContext &context, const std::vector<std::shared_ptr<OperatorParams>> &steps);
    };
}

#endif // PIPELINE_HPP