#ifndef LRU_IMAGE_CACHE_HPP
#define LRU_IMAGE_CACHE_HPP

#include "common/Common.hpp"

#include <unordered_map>
#include <mutex>
#include <list>
#include <memory>

namespace ei::mimgs
{
    class LRUImageCache
    {
    public:
        static LRUImageCache &getInstance();

        void setCapacity(size_t capacity);
        std::shared_ptr<ImageAsset> getCapacity(const std::string &key);
        void putCapacity(const std::string &key, std::shared_ptr<ImageAsset> asset);
        void clearCapacity();

    private:
        LRUImageCache(size_t capacity) : _capacity(capacity) {}
        ~LRUImageCache() = default;

        LRUImageCache(const LRUImageCache &) = delete;
        LRUImageCache &operator=(const LRUImageCache &) = delete;

        void evict();

    private:
        size_t _capacity;
        std::mutex _mutex;

        std::list<std::pair<std::string, std::shared_ptr<ImageAsset>>> _list;
        std::unordered_map<std::string, decltype(_list.begin())> _map;
    };
}

#endif // LRU_IMAGE_CACHE_HPP