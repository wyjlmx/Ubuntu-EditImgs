cmake_minimum_required(VERSION 3.15)

# 1. 声明共享库目标 "MLog"
add_library(MLog SHARED
    Log.cpp
)

# 2. 关联公共头文件目录
target_include_directories(MLog
    PUBLIC "${PROJECT_PUBLIC_INCLUDE_DIR}"
)

# 3. 设置生成的 .so 属性
set_target_properties(MLog PROPERTIES
    OUTPUT_NAME "mlog"                      # 生成 libmlog.so
    VERSION ${PROJECT_VERSION}              # 1.0.0
    SOVERSION ${PROJECT_VERSION_MAJOR}      # 1
)