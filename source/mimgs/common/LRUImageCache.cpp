#include "LRUImageCache.hpp"

namespace ei::mimgs
{
    LRUImageCache &LRUImageCache::getInstance()
    {
        static LRUImageCache instance(15);
        return instance;
    }

    void LRUImageCache::setCapacity(size_t capacity)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _capacity = capacity;
        evict();
    }

    std::shared_ptr<ImageAsset> LRUImageCache::getCapacity(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = _map.find(key);
        if (it == _map.end())
            return nullptr;

        _list.splice(_list.begin(), _list, it->second);

        return it->second->second;
    }

    void LRUImageCache::putCapacity(const std::string &key, std::shared_ptr<ImageAsset> asset)
    {
        if (!asset)
            return;

        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _map.find(key);

        if (it != _map.end())
        {
            it->second->second = asset;
            _list.splice(_list.begin(), _list, it->second);
            return;
        }

        _list.push_front({key, asset});
        _map[key] = _list.begin();

        evict();
    }

    void LRUImageCache::clearCapacity()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _map.clear();
        _list.clear();
    }

    void LRUImageCache::evict()
    {
        while (_list.size() > _capacity)
        {
            auto last = _list.back();
            _map.erase(last.first);
            _list.pop_back();
        }
    }
}