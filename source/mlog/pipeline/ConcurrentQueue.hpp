#ifndef CONCURRENT_QUEUE_HPP
#define CONCURRENT_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>

namespace ei::mlog
{
    template <typename T>
    class ConcurrentQueue
    {
    public:
        ConcurrentQueue() = default;
        ~ConcurrentQueue() = default;

        void waitPush(T task)
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _queue.push(std::move(task));
            }

            _cond.notify_one();
        }

        bool waitPop(std::vector<T> &dest, std::atomic<bool> &stopFlag)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            _cond.wait(lock, [this, &stopFlag]
                       { return !_queue.empty() || stopFlag.load(); });

            if (_queue.empty() && stopFlag.load())
                return false;

            while (!_queue.empty())
            {
                dest.push_back(std::move(_queue.front()));
                _queue.pop();
            }

            return true;
        }

        bool tryPop(std::vector<T> &dest)
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_queue.empty())
                return false;

            while (!_queue.empty())
            {
                dest.push_back(std::move(_queue.front()));
                _queue.pop();
            }

            return true;
        }

        void stopQueue()
        {
            std::lock_guard<std::mutex> lock(_mutex);

            _cond.notify_all();
        }

    private:
        std::queue<T> _queue;
        std::mutex _mutex;
        std::condition_variable _cond;
    };
}

#endif // CONCURRENT_QUEUE_HPP