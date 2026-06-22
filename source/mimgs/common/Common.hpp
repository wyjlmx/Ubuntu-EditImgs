#ifndef COMMON_HPP
#define COMMON_HPP

#include "mlog/Log.hpp"
#include "mimgs/types/EImages.hpp"

#include <stdexcept>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ei::mimgs
{
    // 运行时自定义异常类
    class ProcessingException : public std::runtime_error
    {
    public:
        explicit ProcessingException(const std::string &msg)
            : std::runtime_error("[Internal Error] " + msg) {}
    };

    // 所有算子参数的基类
    struct OperatorParams
    {
        std::string outputKey;
        virtual ~OperatorParams() = default;
        virtual void Validate() const
        {
            if (outputKey.empty())
                throw ProcessingException("output_key is empty");
        }

        virtual std::string GetCacheSignature() const = 0;
    };

    // 内部共享上下文
    class IContext
    {
    private:
        std::unordered_map<std::string, std::shared_ptr<ImageAsset>> _assets;

    public:
        void setAsset(std::shared_ptr<ImageAsset> asset)
        {
            if (!asset || asset->id.empty())
            {
                throw ProcessingException("Null or invalid asset.");
            }
            _assets[asset->id] = std::move(asset);
        }

        std::shared_ptr<ImageAsset> getAsset(const std::string &id) const
        {
            auto it = _assets.find(id);
            if (it != _assets.end())
                return it->second;
            return nullptr;
        }
    };

    // 统一算子契约接口
    class IOperator
    {
    public:
        virtual ~IOperator() = default;
        virtual std::string_view getName() const noexcept = 0;
        virtual void execute(IContext &context, const OperatorParams &params) = 0;

    protected:
        // 参数多态向下转型转换工具方法
        template <typename T>
        const T &CastParams(const OperatorParams &params) const
        {
            const auto *derived = dynamic_cast<const T *>(&params);
            if (!derived)
            {
                throw ProcessingException(std::string(getName()) + ": Parameter mismatch.");
            }
            return *derived;
        }
    };

}

#endif // COMMON_HPP