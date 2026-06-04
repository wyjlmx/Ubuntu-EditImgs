#include "mlog/Log.hpp"

#include <iostream>

int main()
{
    std::cout << "hello, world" << std::endl;

    ilog("INFO log printf");
    ilog("INFO Value: {}", "hhh");

    return 0;
}