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