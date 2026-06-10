#ifndef OPERATOR_REGISTRY_HPP
#define OPERATOR_REGISTRY_HPP

#include "common/Common.hpp"

#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <memory>

namespace ei::mimgs
{
    class OperatorRegistry
    {
    public:
        static OperatorRegistry &getInstance();

        void RegisterOperator(std::unique_ptr<IOperator> op);
        IOperator *getOperator(std::string_view name) const;

    private:
        OperatorRegistry() = default;
        ~OperatorRegistry() = default;

        OperatorRegistry(const OperatorRegistry &) = delete;
        OperatorRegistry &operator=(const OperatorRegistry &) = delete;

    private:
        mutable std::shared_mutex _mutex;
        std::unordered_map<std::string, std::unique_ptr<IOperator>> _registry;
    };
}

#endif // OPERATOR_REGISTRY_HPP