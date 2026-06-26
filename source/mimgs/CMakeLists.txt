cmake_minimum_required(VERSION 3.15)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

set(MIMAGES_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Output directory for mlog library")

# 1. 递归寻找当前目录 (source/mlog) 内的源文件与内部私有头文件
file(GLOB_RECURSE MIMAGES_SOURCES 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 2. 声明共享库目标 "mlog" 
add_library(mimgs SHARED
    ${MIMAGES_SOURCES}
)

# 3. 严格区分：公共（对外）和私有（内部）头文件目录
target_include_directories(mimgs
    PUBLIC 
        # 对外公开：指向全局 include 目录
        "${PROJECT_PUBLIC_INCLUDE_DIR}"
    PRIVATE 
        # 内部私有：指向当前 source/mimgs 目录
        "${CMAKE_CURRENT_SOURCE_DIR}"
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(mimgs 
    PRIVATE 
        ${OpenCV_LIBS}
)

# 4. 设置生成的 .so 属性
set_target_properties(mimgs PROPERTIES
    OUTPUT_NAME "mimgs"                     
    # VERSION ${PROJECT_VERSION}              # 对应 1.0.0
    # SOVERSION ${PROJECT_VERSION_MAJOR}      # 对应 1

    # Linux 下的 .so 属于 LIBRARY 类型
    LIBRARY_OUTPUT_DIRECTORY "${MIMAGES_OUTPUT_DIR}" 
    # Windows 下的 .dll 属于 RUNTIME 类型（顺便兼容 Windows）
    RUNTIME_OUTPUT_DIRECTORY "${MIMAGES_OUTPUT_DIR}"
)

# 5. 添加 Install (安装) 规则，将核心资产发布到系统级目录
install(TARGETS mimgs
    LIBRARY DESTINATION lib       # Linux 下将 .so 拷贝到 /usr/local/lib
    RUNTIME DESTINATION bin       # Windows 兼容：将 .dll 拷贝到 bin
)

# 将给外部使用的头文件（IImages.hpp, Mimgs.hpp 等）暴露出去
install(DIRECTORY ${PROJECT_PUBLIC_INCLUDE_DIR}/mimgs
    DESTINATION include           # 拷贝到 /usr/local/include/mimgs
)

