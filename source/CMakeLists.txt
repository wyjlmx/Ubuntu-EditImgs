cmake_minimum_required(VERSION 3.15)

# 1.引导 CMake 进入具体的子模块目录
add_subdirectory(mlog)
# add_subdirectory(mimages)

# 2. 声明主程序二进制目标 "main"（对应 source/main.cpp）
add_executable(main main.cpp)

# 3. 链接 mlog 动态库
target_link_libraries(main PRIVATE mlog)

# 4. 配置头文件搜索路径，使得 main.cpp 能够通过 #include "mlog/Log.hpp" 顺利找到头文件
target_include_directories(main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")