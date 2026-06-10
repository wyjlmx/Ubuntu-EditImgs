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
        std::vector<std::string> inputKeys;
        uint32_t rows{0};
        uint32_t cols{0};
        uint32_t cellWidth{0};
        uint32_t cellHeight{0};
        uint32_t padding{0};
        std::array<uint8_t, 4> bgColor{0, 0, 0, 0};

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