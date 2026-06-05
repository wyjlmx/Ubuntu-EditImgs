cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# 1. 定义项目与版本
project(EditImgs 
    VERSION 1.0.0 
    LANGUAGES CXX
)

# 2. 全局 C++ 标准及编译器属性
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 3. 规范输出路径 (避免编译产物污染源码目录)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 4. 企业级安全与质量：严格的编译器警告 (Warnings as Errors)
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# 5. 符号可见性控制
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)
# set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# 6. 定义全局变量方便子目录引用
set(PROJECT_PUBLIC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 7. 引入子目录
add_subdirectory(source)

# 8.
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

if(UNIX AND NOT APPLE)
    # $ORIGIN 代表可执行文件所在的当前目录
    # 注意：在 CMake 中 $ 符号需要转义，写作 \${ORIGIN} 或直接用双引号内非转义形式（CMake 会处理）
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
elseif(APPLE)
    # macOS 对应的路径变量为 @loader_path
    set(CMAKE_INSTALL_RPATH "@loader_path")
endif()