#include "OperatorRegistry.hpp"


#include <mutex>

namespace ei::mimgs
{
    OperatorRegistry &OperatorRegistry::getInstance()
    {
        static OperatorRegistry instance;
        return instance;
    }

    void OperatorRegistry::RegisterOperator(std::unique_ptr<IOperator> op)
    {
        if (!op)
            return;

        std::unique_lock<std::shared_mutex> lock(_mutex);
        _registry[std::string(op->getName())] = std::move(op);
    }

    IOperator *OperatorRegistry::getOperator(std::string_view name) const
    {
        std::shared_lock<std::shared_mutex> lock(_mutex);

        auto it = _registry.find(std::string(name));
        if(it != _registry.end())
            return it->second.get();

        return nullptr;
    }
}