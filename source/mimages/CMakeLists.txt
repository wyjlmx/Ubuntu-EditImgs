cmake_minimum_required(VERSION 3.15)

# 1. 声明共享库目标 "MImages"
add_library(MImages SHARED
    Images.cpp
)

# 2. 关联公共头文件目录
target_include_directories(MImages
    PUBLIC "${PROJECT_PUBLIC_INCLUDE_DIR}"
)

# 3. 链接日志模块（如需使用日志）
target_link_libraries(MImages
    PRIVATE MLog
)

# 4. 设置生成的 .so 属性
set_target_properties(MImages PROPERTIES
    OUTPUT_NAME "mimages"                  # 生成 libmimages.so
    VERSION ${PROJECT_VERSION}              # 1.0.0
    SOVERSION ${PROJECT_VERSION_MAJOR}      # 1
)