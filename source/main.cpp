// #define DoMain
#define DoTest

#ifdef DoMain
int main()
{
    return 0;
}

#elif defined(DoTest)
#include "mlog/Log.hpp"

#include <iostream>
#include <thread>
#include <sstream>
#include <vector>

void TaskT1(size_t id)
{
    auto tid_obj = std::this_thread::get_id();

    std::stringstream ss;
    ss << tid_obj;

    std::string tid = ss.str();

    for (size_t i = 0; i < 10; ++i)
    {
        ilog("id[{}] -- threadId[{}]:> {}", id, tid, i);
    }
}

int main()
{
    std::cout << "hello, world" << std::endl;

    ilog("INFO log printf");
    ilog("INFO Value: {}", "good boys");

    std::vector<std::thread> threads;
    for (size_t i = 0; i < 3; ++i)
    {
        threads.push_back(std::thread(TaskT1, i));
    }

    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }

    return 0;
}

#endif
