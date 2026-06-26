cmake_minimum_required(VERSION 3.15)

set(MLOG_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Output directory for mlog library")

# 1. 递归寻找当前目录 (source/mlog) 内的源文件与内部私有头文件
file(GLOB_RECURSE MLOG_SOURCES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 2. 声明共享库目标 "mlog" 
add_library(mlog SHARED
    ${MLOG_SOURCES}
)

# 3. 严格区分：公共（对外）和私有（内部）头文件目录
target_include_directories(mlog
    PUBLIC 
        # 对外公开：指向全局 include 目录
        "${PROJECT_PUBLIC_INCLUDE_DIR}"
    PRIVATE 
        # 内部私有：指向当前 source/mlog 目录
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 4. 设置生成的 .so 属性
set_target_properties(mlog PROPERTIES
    OUTPUT_NAME "mlog"                      # 生成 libmlog.so
    # VERSION ${PROJECT_VERSION}              # 对应 1.0.0
    # SOVERSION ${PROJECT_VERSION_MAJOR}      # 对应 1

    # Linux 下的 .so 属于 LIBRARY 类型
    LIBRARY_OUTPUT_DIRECTORY "${MLOG_OUTPUT_DIR}" 
    # Windows 下的 .dll 属于 RUNTIME 类型（顺便兼容 Windows）
    RUNTIME_OUTPUT_DIRECTORY "${MLOG_OUTPUT_DIR}"
)

# 5. 添加 Install (安装) 规则
install(TARGETS mlog
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 暴露日志的头文件
install(DIRECTORY ${PROJECT_PUBLIC_INCLUDE_DIR}/mlog
    DESTINATION include
)