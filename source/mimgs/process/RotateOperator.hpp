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