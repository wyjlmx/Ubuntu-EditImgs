cmake_minimum_required(VERSION 3.15)

set(MIMAGES_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Output directory for mlog library")

# 1. 递归寻找当前目录 (source/mlog) 内的源文件与内部私有头文件
file(GLOB_RECURSE MIMAGES_SOURCES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 2. 声明共享库目标 "mlog" 
add_library(mimages SHARED
    ${MIMAGES_SOURCES}
)

# 3. 严格区分：公共（对外）和私有（内部）头文件目录
target_include_directories(mimages
    PUBLIC 
        # 对外公开：指向全局 include 目录
        "${PROJECT_PUBLIC_INCLUDE_DIR}"
    PRIVATE 
        # 内部私有：指向当前 source/mimages 目录
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 4. 设置生成的 .so 属性
set_target_properties(mimages PROPERTIES
    OUTPUT_NAME "mimages"                     
    # VERSION ${PROJECT_VERSION}              # 对应 1.0.0
    # SOVERSION ${PROJECT_VERSION_MAJOR}      # 对应 1

    # Linux 下的 .so 属于 LIBRARY 类型
    LIBRARY_OUTPUT_DIRECTORY "${MIMAGES_OUTPUT_DIR}" 
    # Windows 下的 .dll 属于 RUNTIME 类型（顺便兼容 Windows）
    RUNTIME_OUTPUT_DIRECTORY "${MIMAGES_OUTPUT_DIR}"
)

